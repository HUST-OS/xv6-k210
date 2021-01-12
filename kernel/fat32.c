// test operating fat32 sd card.

#include "include/param.h"
#include "include/types.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/spinlock.h"
#include "include/sleeplock.h"
#include "include/buf.h"
#include "include/proc.h"
#include "include/fat32.h"

static struct {
    uint32  first_data_sec;
    uint32  data_sec_cnt;
    uint32  data_clus_cnt;

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

} fat;

// how about a entry pool with lock?
static struct entry_cache {
    struct spinlock lock;
    struct dir_entry entries[ENTRY_CACHE_NUM];
} ecache;

static struct dir_entry root;

/**
 * Read the Boot Parameter Block.
 * @return  0       if success
 *          -1      if fail
 */
int fat32_init()
{
    struct buf *b = bread(0, 0);
    if (b == 0 || strncmp((char const*)(b->data + 82), "FAT32", 5)) {
        return -1;
    }
    fat.bpb.byts_per_sec = *(uint16 *)(b->data + 11);
    fat.bpb.sec_per_clus = *(b->data + 13);
    fat.bpb.rsvd_sec_cnt = *(uint16 *)(b->data + 14);
    fat.bpb.fat_cnt = *(b->data + 16);
    fat.bpb.hidd_sec = *(uint32 *)(b->data + 28);
    fat.bpb.tot_sec = *(uint32 *)(b->data + 32);
    fat.bpb.fat_sz = *(uint32 *)(b->data + 36);
    fat.bpb.root_clus = *(uint32 *)(b->data + 44);
    fat.first_data_sec = fat.bpb.rsvd_sec_cnt + fat.bpb.fat_cnt * fat.bpb.fat_sz;
    fat.data_sec_cnt = fat.bpb.tot_sec - fat.first_data_sec;
    fat.data_clus_cnt = fat.data_sec_cnt / fat.bpb.sec_per_clus;
    brelse(b);

	// make sure that byts_per_sec has the same value with BSIZE 
	if (BSIZE != fat.bpb.byts_per_sec) 
		panic("byts_per_sec != BSIZE");
    
    memset(ecache.entries, 0, sizeof(struct dir_entry) * ENTRY_CACHE_NUM);
    memset(&root, 0, sizeof(root));
    initlock(&ecache.lock, "ecache");
    initsleeplock(&root.lock, "entry");
    root.prev = &root;
    root.next = &root;
    for(struct dir_entry *de = ecache.entries; de < ecache.entries + ENTRY_CACHE_NUM; de++) {
        de->next = root.next;
        de->prev = &root;
        initsleeplock(&de->lock, "entry");
        root.next->prev = de;
        root.next = de;
    }
    root.attribute = ATTR_DIRECTORY;
    root.first_clus = fat.bpb.root_clus;
    
    return 0;
}


/**
 * @param   cluster   cluster number starts from 2, which means no 0 and 1
 */
static inline uint32 first_sec_of_clus(uint32 cluster)
{
    return ((cluster - 2) * fat.bpb.sec_per_clus) + fat.first_data_sec;
}


/**
 * For the given number of a data cluster, return the number of the sector in a FAT table.
 * @param   cluster     number of a data cluster
 * @param   fat_num     number of FAT table from 1, shouldn't be larger than bpb::fat_cnt
 */
static inline uint32 fat_sec_of_clus(uint32 cluster, uint8 fat_num)
{
    return fat.bpb.rsvd_sec_cnt + (cluster << 2) / fat.bpb.byts_per_sec + fat.bpb.fat_sz * (fat_num - 1);
}


/**
 * For the given number of a data cluster, return the offest in the corresponding sector in a FAT table.
 * @param   cluster   number of a data cluster
 */
static inline uint32 fat_offset_of_clus(uint32 cluster)
{
    return (cluster << 2) % fat.bpb.byts_per_sec;
}


/**
 * Read the FAT table content corresponded to the given cluster number.
 * @param   cluster     the number of cluster which you want to read its content in FAT table
 */
static uint32 read_fat(uint32 cluster)
{
    if (cluster >= FAT32_EOC) {
        //panic or ?
        return cluster;
    }
    if (cluster > fat.data_clus_cnt + 1) {     // because cluster number starts at 2, not 0
        return 0;
    }
    uint32 fat_sec = fat_sec_of_clus(cluster, 1);
    // here should be a cache layer for FAT table, but not implemented yet.
    struct buf *b = bread(0, fat_sec);
    uint32 next_clus = *(uint32 *)(b->data + fat_offset_of_clus(cluster));
    brelse(b);
    return next_clus;

	// if the cluster is the last, what should be returned? 
}

static int write_fat(uint32 cluster, uint32 content)
{
    if (cluster > fat.data_clus_cnt + 1) {
        return -1;
    }
    uint32 fat_sec = fat_sec_of_clus(cluster, 1);
    struct buf *b = bread(0, fat_sec);
    uint off = fat_offset_of_clus(cluster);
    *(uint32 *)(b->data + off) = content;
    bwrite(b);
    brelse(b);
    return 0;
}

static void zero_clus(uint32 cluster)
{
    uint32 sec = first_sec_of_clus(cluster);
    struct buf *b;
    for (int i = 0; i < fat.bpb.sec_per_clus; i++) {
        b = bread(0, sec++);
        memset(b->data, 0, BSIZE);
        bwrite(b);
        brelse(b);
    }
}

static uint32 alloc_clus()
{
    // should we keep a free cluster list? instead of searching fat every time.
    struct buf *b;
    uint32 sec = fat.bpb.rsvd_sec_cnt;
    uint32 const ent_per_sec = fat.bpb.byts_per_sec / sizeof(uint32);
    for (uint32 i = 0; i < fat.bpb.fat_sz; i++, sec++) {
        b = bread(0, sec);
        for (uint32 j = 0; j < ent_per_sec; j++) {
            if (((uint32 *)(b->data))[j] == 0) {
                ((uint32 *)(b->data))[j] = FAT32_EOC;
                bwrite(b);
                brelse(b);
                uint32 clus = i * ent_per_sec + j;
                zero_clus(clus);
                return clus;
            }
        }
        brelse(b);
    }
    panic("no clusters");
}

// static void free_clus(uint32 cluster)
// {
//     write_fat(cluster, 0);
// }

// should never root by eget
static struct dir_entry *eget(uint dev, uint32 parent, ushort *name)
{
    struct dir_entry *ep;
    acquire(&ecache.lock);
    for (ep = root.next; ep != &root; ep = ep->next) {
        if (ep->dev == dev && ep->parent == parent
            && wcsncmp(ep->filename, name, FAT32_MAX_FILENAME) == 0) {
            ep->ref++;
            ep->valid = 1;
            release(&ecache.lock);
            return ep;
        }
    }
    for (ep = root.prev; ep != &root; ep = ep->prev) {
        if (ep->ref == 0) {
            ep->ref = 1;
            ep->valid = 0;
            release(&ecache.lock);
            return ep;
        }
    }
    panic("eget: insufficient ecache");
    return 0;
}

struct dir_entry *edup(struct dir_entry *entry)
{
    acquire(&ecache.lock);
    entry->ref++;
    release(&ecache.lock);
    return entry;
}

// only update file size
void eupdate(struct dir_entry *entry)
{
    uint const byts_per_clus = fat.bpb.sec_per_clus * fat.bpb.byts_per_sec;
    int clus_num = entry->off / byts_per_clus;
    int off = entry->off % byts_per_clus;
    uint32 cluster = entry->parent;
    while (clus_num-- > 0) {
        cluster = read_fat(cluster);
        if (cluster >= FAT32_EOC) {
            panic("eupdate");
        }
    }
    uint sec = first_sec_of_clus(cluster) + off / fat.bpb.byts_per_sec;
    off = off % fat.bpb.byts_per_sec;
    struct buf *b = bread(entry->dev, sec);
    *(uint32 *)(b->data + off + 28) = entry->file_size;
    bwrite(b);
    brelse(b);
}


void elock(struct dir_entry *entry)
{
    if (entry == 0 || entry->ref < 1)
        panic("elock");
    acquiresleep(&entry->lock);

}


void eunlock(struct dir_entry *entry)
{
    if (entry == 0 || !holdingsleep(&entry->lock) || entry->ref < 1)
        panic("eunlock");
    releasesleep(&entry->lock);
}


void eput(struct dir_entry *entry)
{
    acquire(&ecache.lock);
    if (entry->ref == 1) {
        // ref == 1 means no other process can have entry locked,
        // so this acquiresleep() won't block (or deadlock).
        acquiresleep(&entry->lock);
        release(&ecache.lock);

        // should have write-back op

        if (entry != &root) {
            entry->next->prev = entry->prev;
            entry->prev->next = entry->next;
            entry->next = root.next;
            entry->prev = &root;
            root.next->prev = entry;
            root.next = entry;
            eupdate(entry);
        }
        releasesleep(&entry->lock);
        acquire(&ecache.lock);
    }
    entry->ref--;
    release(&ecache.lock);
}


/**
 * Read filename from directory entry.
 * @param   filename    pointer to the array that stores the name
 * @param   raw_entry   pointer to the entry in a sector buffer
 * @param   islong      if non-zero, read as l-n-e, otherwise s-n-e.
 */
void read_entry_name(wchar *filename, uint8 *raw_entry, int longcnt)
{
    if (longcnt) {                       // long entry branch
        int offset = (longcnt - 1) * 13;
        filename += offset;
        memmove(filename, raw_entry + 1, 10);
        filename += 5;
        memmove(filename, raw_entry + 14, 12);
        filename += 6;
        memmove(filename, raw_entry + 28, 4);
    } else {
        // assert: only "." and ".." will enter this branch
        memset(filename, 0, 12 << 1);
        int i = 7;
        if (raw_entry[i] == ' ') {
            do {
                i--;
            } while (i >= 0 && raw_entry[i] == ' ');
        }
        i++;
        wnstr(filename, raw_entry, i);
        if (raw_entry[8] != ' ') {
            wnstr(filename + i + 1, raw_entry + 8, 3);
            filename[i] = '.';
        }
    }
}


/**
 * Read entry_info from directory entry.
 * @param   entry       pointer to the structure that stores the entry info
 * @param   raw_entry   pointer to the entry in a sector buffer    
 */
void read_entry_info(struct dir_entry *entry, uint8 *raw_entry)
{
    entry->attribute = raw_entry[11];
    // entry->create_time_tenth = raw_entry[13];
    // entry->create_time = *(uint16 *)(raw_entry + 14);
    // entry->create_date = *(uint16 *)(raw_entry + 16);
    // entry->last_access_date = *(uint16 *)(raw_entry + 18);
    // entry->last_write_time = *(uint16 *)(raw_entry + 22);
    // entry->last_write_date = *(uint16 *)(raw_entry + 24);
    entry->first_clus = ((uint32) *(uint16 *)(raw_entry + 20)) << 16;
    entry->first_clus += *(uint16 *)(raw_entry + 26);
    entry->file_size = *(uint32 *)(raw_entry + 28);
}


/**
 * Seacher for the entry in a directory and return a structure.
 * @param   entry       entry of a directory file
 * @param   filename    target filename
 * @param   len         length of the filename
 */
static struct dir_entry *lookup_dir(struct dir_entry *entry, wchar *filename, int len)
{
    if (entry->attribute != ATTR_DIRECTORY) {
        return 0;
    }
    struct dir_entry *de = eget(0, entry->first_clus, filename);
    if (de->valid) { return de; }
    uint32 cluster = entry->first_clus;
    uint32 sec1 = first_sec_of_clus(cluster);
    uint32 sec = sec1;
    uint32 clus_cnt = 0;
    int entcnt = (len + CHAR_LONG_NAME - 1) / CHAR_LONG_NAME;               // count of l-n-entries, rounds up
    int skip = 0;                   // while switching sector, skip some entry;
    int match = 0;
    while (cluster < FAT32_EOC) {
        struct buf *b = bread(0, sec);
        if (b == 0) { return 0; }
        uint8 *ep = b->data + skip;
        while (ep < b->data + fat.bpb.byts_per_sec) {
            if (ep[0] == EMPTY_ENTRY) {
                ep += 32;
                continue;
            } else if (ep[0] == END_OF_ENTRY) {
                brelse(b);
                return 0;
            }
            if (ep[11] == ATTR_LONG_NAME) {
                int count = ep[0] & ~LAST_LONG_ENTRY;
                if ((ep[0] & LAST_LONG_ENTRY) && count != entcnt) {         // meet first l-n-e, and count is unequal
                    ep += (count + 1) << 5;                                   // skip over, plus corresponding s-n-e
                } else {
                    read_entry_name(de->filename, ep, count);
                    int offset = (count - 1) * CHAR_LONG_NAME;
                    if (wcsncmp(de->filename + offset, filename + offset, 13) != 0) {
                        ep += (count + 1) << 5;
                    } else {
                        if (count == 1) {
                            match = 1;
                        }
                        ep += 32;
                    }
                }
            } else {
                if (match) {
                    match = 0;
                } else {
                    read_entry_name(de->filename, ep, 0);
                    if (wcsncmp(de->filename, filename, 11) != 0) {
                        ep += 32;
                        continue;
                    }
                }
                de->filename[len] = '\0';
                de->valid = 1;
                de->parent = entry->first_clus;
                de->off = (clus_cnt * fat.bpb.sec_per_clus + (sec - sec1)) * fat.bpb.byts_per_sec + (ep - b->data);
                read_entry_info(de, ep);
                brelse(b);
                return de;
            }
        }
        skip = ep - (b->data + fat.bpb.byts_per_sec);       // set offset after switching sectors
        brelse(b);
        sec++;
        if (sec - sec1 >= fat.bpb.sec_per_clus) {
            cluster = read_fat(cluster);
            sec = sec1 = first_sec_of_clus(cluster);
            clus_cnt++;
        }
    }
    return 0;
}


static int skipelem(wchar **path, wchar *name)
{
    int len;
    while (**path == '/') {
        (*path)++;
    }
    if (**path == 0) { return -1; }
    wchar *s = *path;
    while (**path != '/' && **path != 0) {
        (*path)++;
    }
    len = *path - s;
    if (len > FAT32_MAX_FILENAME) {
        len = FAT32_MAX_FILENAME;
    } else {
        name[len] = 0;
    }
    memmove(name, s, len << 1);
    while (**path == '/') {
        (*path)++;
    }
    return len;
}


/**
 * FAT32 version of namex in xv6's original file system.
 * 
 */
static struct dir_entry *lookup_path(wchar *path, int parent, wchar *name)
{
    struct dir_entry *entry, *next;
    if (*path == '/') {
        entry = edup(&root);
    } else {
        entry = edup(myproc()->cwd);
    }
    int len;
    while ((len = skipelem(&path, name)) != -1) {
        elock(entry);
        if (entry->attribute != ATTR_DIRECTORY) {
            eunlock(entry);
            eput(entry);
            return 0;
        }
        if (parent && *path == '\0') {
            eunlock(entry);
            return entry;
        }
        if ((next = lookup_dir(entry, name, len)) == 0) {
            eunlock(entry);
            eput(entry);
            return 0;
        }
        eunlock(entry);
        eput(entry);
        entry = next;
    }
    if (parent) {
        eput(entry);
        return 0;
    }
    return entry;
}

struct dir_entry *get_entry(char *path)
{
    wchar wpath[FAT32_MAX_FILENAME + 1] = {0};
	wchar name[FAT32_MAX_FILENAME + 1] = {0};
    wnstr(wpath, (uchar *) path, FAT32_MAX_PATH);
	return lookup_path(wpath, 0, name);
}

struct dir_entry *get_parent(char *path, char *name)
{
    wchar wpath[FAT32_MAX_FILENAME + 1] = {0};
	wchar wname[FAT32_MAX_FILENAME + 1] = {0};
    wnstr(wpath, (uchar *) path, FAT32_MAX_PATH);
    struct dir_entry *ep = lookup_path(wpath, 1, wname);
    snstr((uchar *) name, wname, FAT32_MAX_FILENAME);
	return ep;
}

static uint eread_clus(uint32 cluster, int user_dst, uint64 dst, uint off, uint n)
{
	int const MAX_OFFSET = fat.bpb.byts_per_sec * fat.bpb.sec_per_clus;

	// check if offset is valid 
	// in my opinion, this should be checked outside the function, 
	// we check it here as we're still debugging the filesystem 
	if (off + n > MAX_OFFSET) {
		panic("offset out of range");
	}

    uint tot, m;
    struct buf *bp;
    uint sec = first_sec_of_clus(cluster) + off / fat.bpb.byts_per_sec;
    off = off % fat.bpb.byts_per_sec;

    for (tot = 0; tot < n; tot += m, off += m, dst += m, sec++) {
        bp = bread(0, sec);
        m = BSIZE - off % BSIZE;
        if (n - tot < m) {
            m = n - tot;
        }
        if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
            brelse(bp);
            break;
        }
        brelse(bp);
    }
    return tot;
}

/* like the original readi, but "reade" is odd, let alone "writee" */
int eread(struct dir_entry *entry, int user_dst, uint64 dst, uint off, uint n)
{
    if (off > entry->file_size || off + n < off) {
        return 0;
    }
    if (off + n > entry->file_size) {
        n = entry->file_size - off;
    }
    uint const byts_per_clus = fat.bpb.sec_per_clus * fat.bpb.byts_per_sec;
    int clus_num = off / byts_per_clus;
    off = off % byts_per_clus;
    uint32 cluster = entry->first_clus;
    while (clus_num-- > 0 && cluster < FAT32_EOC) {
        cluster = read_fat(cluster);
    }

    uint tot, m;
    for (tot = 0; cluster < FAT32_EOC && tot < n; tot += m, off += m, dst += m) {
        m = byts_per_clus - off % byts_per_clus;
        if (n - tot < m) {
            m = n - tot;
        }
        if (eread_clus(cluster, user_dst, dst, off % byts_per_clus, m) != m) {
            break;
        }
        cluster = read_fat(cluster);
    }
    return tot;
}

static uint ewrite_clus(uint32 cluster, int user_src, uint64 src, uint off, uint n)
{
	int const MAX_OFFSET = fat.bpb.byts_per_sec * fat.bpb.sec_per_clus;

	if (off + n > MAX_OFFSET) {
		panic("offset out of range");
	}

    uint tot, m;
    struct buf *bp;
    uint sec = first_sec_of_clus(cluster) + off / fat.bpb.byts_per_sec;
    off = off % fat.bpb.byts_per_sec;

    for (tot = 0; tot < n; tot += m, off += m, src += m, sec++) {
        bp = bread(0, sec);
        m = BSIZE - off % BSIZE;
        if (n - tot < m) {
            m = n - tot;
        }
        if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
            brelse(bp);
            break;
        }
        bwrite(bp);
        brelse(bp);
    }
    return tot;
}

// Caller must hold entry->lock.
int ewrite(struct dir_entry *entry, int user_src, uint64 src, uint off, uint n)
{
    if (off > entry->file_size || off + n < off) {
        return -1;
    }

    uint const byts_per_clus = fat.bpb.sec_per_clus * fat.bpb.byts_per_sec;
    int clus_num = off / byts_per_clus;
    off = off % byts_per_clus;
    uint32 cluster = entry->first_clus;
    while (clus_num-- > 0 && cluster < FAT32_EOC) {
        cluster = read_fat(cluster);
    }

    uint tot, m;
    for (tot = 0; cluster < FAT32_EOC && tot < n; tot += m, off += m, src += m) {
        m = byts_per_clus - off % byts_per_clus;
        if (n - tot < m) {
            m = n - tot;
        }
        if (ewrite_clus(cluster, user_src, src, off % byts_per_clus, m) != m) {
            break;
        }
        if ((cluster = read_fat(cluster)) >= FAT32_EOC) {
            uint32 new_clus = alloc_clus(entry->dev);
            write_fat(cluster, new_clus);
            write_fat(new_clus, FAT32_EOC + 7);
        }
    }
    if(n > 0) {
        if(off > entry->file_size) {
            entry->file_size = off;
            eupdate(entry);
        }
    }
    return tot;
}
