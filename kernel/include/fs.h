#ifndef __FS_H
#define __FS_H

/**
 * A very simple implement of Virtual File System.
 * 
 * To support some unix-style operations like 'mount',
 * It's easier to design the mechanism on a vfs.
 * 
 * This implement is not complete, and the interfaces
 * are poorly considered, which are just designed for
 * FAT32.
 */


#include "types.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"

struct superblock;
struct inode;
struct dentry;

/**
 * This collection specifies how to access the disk.
 * All file ops end up calling to these.
 */
struct fs_op {
	struct inode *(*alloc_inode)(struct superblock *sb);
	void (*destroy_inode)(struct inode *ip);
	int (*write)(struct superblock *sb, int usr, char *src, uint64 blockno, uint64 off, uint64 len);
	int (*read)(struct superblock *sb, int usr, char *dst, uint64 blockno, uint64 off, uint64 len);
	int (*clear)(struct superblock *sb, uint64 blockno, uint64 blockcnt);
};

struct inode_op {
	struct inode *(*create)(struct inode *ip, char *name, int mode);
	struct inode *(*lookup)(struct inode *dir, char *filename, uint *poff);
	int (*truncate)(struct inode *ip);
	int (*unlink)(struct inode *ip);
	int (*update)(struct inode *ip);
	int (*getattr)(struct inode *ip, struct stat *st);

};

struct dentry_op {
	int (*delete)(struct dentry *de);
	// int (*compare)(struct dentry *de, char *name);
};

struct file_op {
	int (*read)(struct inode *ip, int usr, uint64 dst, uint off, uint n);
	int (*write)(struct inode *ip, int usr, uint64 src, uint off, uint n);
	int (*readdir)(struct inode *ip, struct stat *st, uint off);
};


struct superblock {
	uint				blocksz;
	uint				devnum;
	struct inode		*dev;

	void				*real_sb;
	struct superblock	*next;
	int					ref;
	
	struct sleeplock	sb_lock;
	struct fs_op		op;

	struct spinlock		cache_lock;
	struct dentry		*root;
};


// For inode.state
#define I_STATE_VALID		(1 << 0)
#define I_STATE_DIRTY		(1 << 2)
#define I_STATE_FREE		(1 << 3)
#define I_STATE_LOCKED		(1 << 4)

struct inode {
	uint64				inum;
	int					ref;
	int					state;
	int					mode;
	int					size;
	int					nlink;	// useless on FAT
	void				*real_i;
	struct superblock	*sb;

	struct sleeplock	lock;
	struct inode_op		*op;
	struct file_op		*fop;
	// struct spinlock		lock;

	struct dentry		*entry;
};


#define MAXNAME 255

struct dentry {
	char				filename[MAXNAME + 1];
	// int					ref;
	struct inode		*inode;
	struct dentry		*parent;
	struct dentry		*next;
	struct dentry		*child;
	struct dentry_op	*op;
	struct superblock	*mount;
};


#define IMODE_READ_ONLY		(1 << 0)
#define IMODE_HIDDEN		(1 << 1)
#define IMODE_DIRECTORY		(1 << 2)


void rootfs_init();
void rootfs_print();

struct inode *create(char *path, int type);
int unlink(struct inode *ip);

struct inode *idup(struct inode *ip);
void iput(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);

struct inode *namei(char *path);
struct inode *nameiparent(char *path, char *name);
int namepath(struct inode *ip, char *path, int max);

int do_mount(struct inode *dev, struct inode *mntpoint, char *type, int flag, void *data);

static inline void iunlockput(struct inode *ip)
{
	iunlock(ip);
	iput(ip);
}


#endif
