/**
 * Driver for file system image file.
 * 
 * A image file with file system can be mounted,
 * This module provide the interface to operate
 * the file as if it is a real device.
 * 
 * Only FAT32 supported.
 * 
 */

#ifndef __DEBUG_imgfs
#undef DEBUG
#endif

#include "include/types.h"
#include "include/fs.h"
#include "include/fat32.h"
#include "include/kmalloc.h"
#include "include/string.h"
#include "include/debug.h"


extern struct inode_op fat32_inode_op;
extern struct file_op fat32_file_op;

extern struct dentry_op rootfs_dentry_op;


static int imgfs_write(struct superblock *sb, int usr, char *src, uint64 sectorno, uint64 off, uint64 len)
{
	__debug_info("imgfs_write", "sec:%d off:%d len:%d\n", sectorno, off, len);
	
	struct inode *img = sb->dev;
	
	ilock(img);
	int ret = img->fop->write(img, usr, (uint64)src, sb->blocksz * sectorno + off, len);
	iunlock(img);

	return ret;
}


static int imgfs_read(struct superblock *sb, int usr, char *dst, uint64 sectorno, uint64 off, uint64 len)
{
	__debug_info("imgfs_read", "sec:%d off:%d len:%d\n", sectorno, off, len);
	
	struct inode *img = sb->dev;
	
	ilock(img);
	int ret = img->fop->read(img, usr, (uint64)dst, sb->blocksz * sectorno + off, len);
	iunlock(img);

	return ret;
}


static int imgfs_clear(struct superblock *sb, uint64 sectorno, uint64 sectorcnt)
{
	struct inode *img = sb->dev;
	int ret = 0;
	uint64 const blksz = sb->blocksz;

	char *buf = kmalloc(blksz);
	if (buf == NULL) 
		return -1;
	memset(buf, 0, blksz);

	uint64 pos = sectorno * blksz;
	ilock(img);
	for (; sectorcnt--; pos += blksz) {
		if (img->fop->write(img, 0, (uint64)buf, pos, blksz) != blksz) {
			ret = -1;
			break;
		}
	}
	iunlock(img);
	
	kfree(buf);
	return ret;
}


// Caller must hold img->lock.
struct superblock *image_fs_init(struct inode *img)
{
	if (img->size < 512) // Can such a small file contain a file system image?
		return NULL;

	struct superblock *sb = NULL;
	char *sb_buf = NULL;
	struct fat32_sb *fat = NULL;
	struct inode *iroot = NULL;
	
	__debug_info("image_fs_init", "start\n");
	if ((sb = kmalloc(sizeof(struct superblock))) == NULL) {
		return NULL;
	}
	if ((sb_buf = kmalloc(128)) == NULL || 
		img->fop->read(img, 0, (uint64)sb_buf, 0, 128) != 128 ||
		(fat = fat32_init(sb_buf)) == NULL)
	{
		goto fail;
	}
	kfree(sb_buf);
	sb_buf = NULL;

	initsleeplock(&sb->sb_lock, "imgfs_sb");
	initlock(&sb->cache_lock, "imgfs_dcache");
	sb->next = NULL;
	sb->devnum = img->inum;
	sb->real_sb = fat;
	sb->ref = 0;
	sb->blocksz = fat->bpb.byts_per_sec;
	sb->op.alloc_inode = fat_alloc_inode;
	sb->op.destroy_inode = fat_destroy_inode;
	sb->op.read = imgfs_read;
	sb->op.write = imgfs_write;
	sb->op.clear = imgfs_clear;

	// Initialize in-mem root.
	iroot = fat32_root_init(sb);
	sb->root = kmalloc(sizeof(struct dentry));
	if (iroot == NULL || sb->root == NULL)
		goto fail;

	iroot->entry = sb->root;
	memset(sb->root, 0, sizeof(struct dentry));
	sb->root->inode = iroot;
	sb->root->op = &rootfs_dentry_op;
	// sb->root->filename[0] = '\0';

	sb->dev = idup(img);

	__debug_info("image_fs_init", "done\n");
	return sb;

fail:
	__debug_warn("image_fs_init", "fail\n");
	if (iroot)
		fat_destroy_inode(iroot);
	if (fat)
		kfree(fat);
	if (sb_buf)
		kfree(sb_buf);
	if (sb)
		kfree(sb);
	return NULL;
}


static void fs_clean(struct superblock *sb, struct dentry *de)
{
	sb->op.destroy_inode(de->inode);
	for (struct dentry *child = de->child; child != NULL;) {
		struct dentry *temp = child->next;
		fs_clean(sb, child);
		child = temp;
	}
	kfree(de);
}

void image_fs_uninstall(struct superblock *sb)
{
	iput(sb->dev);
	fs_clean(sb, sb->root);
	kfree(sb->real_sb);
	kfree(sb);
}
