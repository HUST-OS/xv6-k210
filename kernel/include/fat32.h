#ifndef __FAT32_H
#define __FAT32_H

#include "sleeplock.h"
#include "stat.h"
#include "fs.h"

// FAT32 file attribute
#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      0x0F

// FAT32 flags
// last fat chain flag
#define FAT32_EOC           0x0ffffff8
// last long entry flag
#define LAST_LONG_ENTRY     0x40
#define EMPTY_ENTRY         0xe5
// end of dir
#define END_OF_DIR          0x00
// filename capacity each entry
#define CHAR_LONG_NAME      13
#define CHAR_SHORT_NAME     11

#define FAT32_MAX_FILENAME  255
#define FAT32_MAX_PATH      260


/* FAT32 superblock */
struct fat32_sb {
    uint32  first_data_sec;
    uint32  data_sec_cnt;
    uint32  data_clus_cnt;
    uint32  byts_per_clus;
    struct {
        uint16  byts_per_sec;
        uint8   sec_per_clus;
        uint16  rsvd_sec_cnt;
        uint8   fat_cnt;            /* count of FAT regions */
        uint32  hidd_sec;           /* count of hidden sectors */
        uint32  tot_sec;            /* total count of sectors including all regions */
        uint32  fat_sz;             /* count of sectors for a FAT region */
        uint32  root_clus;
    } bpb;
};

/* Inode of FAT32 in-memory format */
struct fat32_entry {
    char  filename[FAT32_MAX_FILENAME + 1];
    uint8   attribute;
    // uint8   create_time_tenth;
    // uint16  create_time;
    // uint16  create_date;
    // uint16  last_access_date;
    uint32  first_clus;
    // uint16  last_write_time;
    // uint16  last_write_date;
    uint32  file_size;

    uint    ent_cnt;
    uint32  cur_clus;
    uint    clus_cnt;
};


struct fat32_sb*    fat32_init(char *boot_sector);
struct inode*       fat32_root_init(struct superblock *sb);
struct inode*       fat_lookup_dir(struct inode *dir, char *filename, uint *poff);
struct inode*       fat_alloc_inode(struct superblock *sb);
void                fat_destroy_inode(struct inode *ip);
struct inode*       fat_alloc_entry(struct inode *dir, char *name, int mode);
int                 fat_update_entry(struct inode *ip);
int                 fat_remove_entry(struct inode *ip);
int                 fat_truncate_file(struct inode *ip);
int                 fat_stat_file(struct inode *ip, struct stat *st);
int                 fat_read_dir(struct inode *dir, struct stat *st, uint off);
int                 fat_read_file(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int                 fat_write_file(struct inode *ip, int user_src, uint64 src, uint off, uint n);

#endif