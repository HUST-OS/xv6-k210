#include "include/param.h"
#include "include/types.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/spinlock.h"
#include "include/sleeplock.h"
#include "include/buf.h"
#include "include/proc.h"
#include "include/stat.h"
#include "include/fat32.h"

static struct {
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

} fat;

static struct entry_cache {
    struct spinlock lock;
    struct dirent entries[ENTRY_CACHE_NUM];
} ecache;

static struct dirent root;

/**
 * Read the Boot Parameter Block.
 * @return  0       if success
 *          -1      if fail
 */
int fat32_init()
{
    struct buf *b = bread(0, 0);
    if (strncmp((char const*)(b->data + 82), "FAT32", 5))
        panic("not FAT32 volume");
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
    fat.byts_per_clus = fat.bpb.sec_per_clus * fat.bpb.byts_per_sec;
    brelse(b);

    // printf("[FAT32 init]byts_per_sec: %d\n", fat.bpb.byts_per_sec);
    // printf("[FAT32 init]root_clus: %d\n", fat.bpb.root_clus);
    // printf("[FAT32 init]sec_per_clus: %d\n", fat.bpb.sec_per_clus);
    // printf("[FAT32 init]fat_cnt: %d\n", fat.bpb.fat_cnt);
    // printf("[FAT32 init]fat_sz: %d\n", fat.bpb.fat_sz);

    // make sure that byts_per_sec has the same value with BSIZE 
    if (BSIZE != fat.bpb.byts_per_sec) 
        panic("byts_per_sec != BSIZE");
    
    initlock(&ecache.lock, "ecache");
    memset(&root, 0, sizeof(root));
    initsleeplock(&root.lock, "entry");
    root.attribute = (ATTR_DIRECTORY | ATTR_SYSTEM);
    root.first_clus = root.cur_clus = fat.bpb.root_clus;
    root.valid = 1;
    root.prev = &root;
    root.next = &root;
    for(struct dirent *de = ecache.entries; de < ecache.entries + ENTRY_CACHE_NUM; de++) {
        de->dev = 0;
        de->valid = 0;
        de->ref = 0;
        de->dirty = 0;
        de->parent = 0;
        de->next = root.next;
        de->prev = &root;
        initsleeplock(&de->lock, "entry");
        root.next->prev = de;
        root.next = de;
    }
    
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
}

/**
 * Write the FAT region content corresponded to the given cluster number.
 * @param   cluster     the number of cluster to write its content in FAT table
 * @param   content     the content which should be the next cluster number of FAT end of chain flag
 */
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

static uint32 alloc_clus(uint8 dev)
{
    // should we keep a free cluster list? instead of searching fat every time.
    struct buf *b;
    uint32 sec = fat.bpb.rsvd_sec_cnt;
    uint32 const ent_per_sec = fat.bpb.byts_per_sec / sizeof(uint32);
    for (uint32 i = 0; i < fat.bpb.fat_sz; i++, sec++) {
        b = bread(dev, sec);
        for (uint32 j = 0; j < ent_per_sec; j++) {
            if (((uint32 *)(b->data))[j] == 0) {
                ((uint32 *)(b->data))[j] = FAT32_EOC + 7;
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

static void free_clus(uint32 cluster)
{
    write_fat(cluster, 0);
}

static uint rw_clus(uint32 cluster, int write, int user, uint64 data, uint off, uint n)
{
    if (off + n > fat.byts_per_clus)
        panic("offset out of range");
    uint tot, m;
    struct buf *bp;
    uint sec = first_sec_of_clus(cluster) + off / fat.bpb.byts_per_sec;
    off = off % fat.bpb.byts_per_sec;

    int bad = 0;
    for (tot = 0; tot < n; tot += m, off += m, data += m, sec++) {
        bp = bread(0, sec);
        m = BSIZE - off % BSIZE;
        if (n - tot < m) {
            m = n - tot;
        }
        if (write) {
            if ((bad = either_copyin(bp->data + (off % BSIZE), user, data, m)) != -1) {
                bwrite(bp);
            }
        } else {
            bad = either_copyout(user, data, bp->data + (off % BSIZE), m);
        }
        brelse(bp);
        if (bad == -1) {
            break;
        }
    }
    return tot;
}

/**
 * for the given entry, relocate the cur_clus field based on the off
 * @param   entry       modify its cur_clus field
 * @param   off         the offset from the beginning of the relative file
 * @return              the offset from the new cur_clus
 */
static uint reloc_clus(struct dirent *entry, uint off)
{
    int clus_num = off / fat.byts_per_clus;
    while (clus_num > entry->clus_cnt) {
        entry->cur_clus = read_fat(entry->cur_clus);
        entry->clus_cnt++;
    }
    if (clus_num < entry->clus_cnt) {
        entry->cur_clus = entry->first_clus;
        entry->clus_cnt = 0;
        while (entry->clus_cnt < clus_num) {
            entry->cur_clus = read_fat(entry->cur_clus);
            entry->clus_cnt++;
        }
    }
    return off % fat.byts_per_clus;
}

/* like the original readi, but "reade" is odd, let alone "writee" */
// Caller must hold entry->lock.
int eread(struct dirent *entry, int user_dst, uint64 dst, uint off, uint n)
{
    if (off > entry->file_size || off + n < off || (entry->attribute & ATTR_DIRECTORY)) {
        return 0;
    }
    if (off + n > entry->file_size) {
        n = entry->file_size - off;
    }

    uint tot, m;
    for (tot = 0; entry->cur_clus < FAT32_EOC && tot < n; tot += m, off += m, dst += m) {
        reloc_clus(entry, off);
        m = fat.byts_per_clus - off % fat.byts_per_clus;
        if (n - tot < m) {
            m = n - tot;
        }
        if (rw_clus(entry->cur_clus, 0, user_dst, dst, off % fat.byts_per_clus, m) != m) {
            break;
        }
    }
    return tot;
}

// Caller must hold entry->lock.
int ewrite(struct dirent *entry, int user_src, uint64 src, uint off, uint n)
{
    if (off > entry->file_size || off + n < off || (entry->attribute & ATTR_READ_ONLY)) {
        return -1;
    }
    if (entry->first_clus == 0) {   // so file_size if 0 too, which requests off == 0
        entry->cur_clus = entry->first_clus = alloc_clus(entry->dev);
        entry->clus_cnt = 0;
        entry->dirty = 1;
    }

    uint tot, m;
    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        reloc_clus(entry, off);
        if (entry->cur_clus >= FAT32_EOC) {
            uint32 new_clus = alloc_clus(entry->dev);
            write_fat(entry->cur_clus, new_clus);
        }
        m = fat.byts_per_clus - off % fat.byts_per_clus;
        if (n - tot < m) {
            m = n - tot;
        }
        if (rw_clus(entry->cur_clus, 1, user_src, src, off % fat.byts_per_clus, m) != m) {
            break;
        }
    }
    if(n > 0) {
        if(off > entry->file_size) {
            entry->file_size = off;
            entry->dirty = 1;
            eupdate(entry);
        }
    }
    return tot;
}

// should never get root by eget
static struct dirent *eget(struct dirent *parent, char *name)
{
    struct dirent *ep;
    acquire(&ecache.lock);
    if (name) {
        for (ep = root.next; ep != &root; ep = ep->next) {
            if (ep->parent == parent && strncmp(ep->filename, name, FAT32_MAX_FILENAME) == 0) {
                ep->ref++;
                ep->valid = 1;
                release(&ecache.lock);
                edup(ep->parent);
                return ep;
            }
        }
    }
    for (ep = root.prev; ep != &root; ep = ep->prev) {
        if (ep->ref == 0) {
            ep->ref = 1;
            ep->dev = parent->dev;
            ep->off = 0;
            ep->valid = 0;
            release(&ecache.lock);
            return ep;
        }
    }
    panic("eget: insufficient ecache");
    return 0;
}

static void wnstr_lazy(wchar *dst, char const *src, int len) {
    while (len-- && *src) {
        *dst = *src++;
        dst++;
    }
}

struct dirent *ealloc(struct dirent *dp, char *name, int dir)
{
    if (!(dp->attribute & ATTR_DIRECTORY)) { return 0; }
    struct dirent *ep;
    uint off = 0;
    ep = dirlookup(dp, name, &off);
    if (ep != 0) {      // entry exists
        eput(ep);
        return 0;
    }
    ep = eget(dp, name);
    if (ep->valid) {    // shouldn't be valid
        panic("ealloc");
    }
    elock(ep);

    ep->attribute = 0;
    ep->file_size = 0;
    ep->first_clus = 0;
    ep->parent = edup(dp);
    ep->off = off;
    ep->clus_cnt = 0;
    ep->cur_clus = 0;
    strncpy(ep->filename, name, FAT32_MAX_FILENAME);    

    int len = strlen(name);
    int entcnt = (len + CHAR_LONG_NAME - 1) / CHAR_LONG_NAME;   // count of l-n-entries, rounds up

    uint8 ebuf[32] = {0};
    if(dir){    // generate "." and ".." for ep
        ep->attribute |= ATTR_DIRECTORY;
        ep->cur_clus = ep->first_clus = alloc_clus(dp->dev);
        strncpy((char *)ebuf, ".", 11);
        ebuf[11] = ATTR_DIRECTORY;
        *(uint16 *)(ebuf + 20) = (uint16)(ep->first_clus >> 16);
        *(uint16 *)(ebuf + 26) = (uint16)(ep->first_clus & 0xff);
        *(uint32 *)(ebuf + 28) = 0;
        rw_clus(ep->cur_clus, 1, 0, (uint64)ebuf, 0, sizeof(ebuf));
        strncpy((char *)ebuf, "..", 11);
        ebuf[11] = ATTR_DIRECTORY;
        *(uint16 *)(ebuf + 20) = (uint16)(dp->first_clus >> 16);
        *(uint16 *)(ebuf + 26) = (uint16)(dp->first_clus & 0xff);
        *(uint32 *)(ebuf + 28) = 0;
        rw_clus(ep->cur_clus, 1, 0, (uint64)ebuf, 32, sizeof(ebuf));
    } else {
        ep->attribute |= ATTR_ARCHIVE;
    }
    memset(ebuf, 0, sizeof(ebuf));
    off = reloc_clus(dp, off);
    for (uint8 i = entcnt; i > 0; i--) {                          // ignore checksum
        ebuf[0] = i;
        if (i == entcnt) {
            ebuf[0] |= LAST_LONG_ENTRY;
            memset(ebuf + 1, 0xff, 10);
            memset(ebuf + 14, 0xff, 12);
            memset(ebuf + 28, 0xff, 4);
        }
        ebuf[11] = ATTR_LONG_NAME;
        wnstr_lazy((wchar *) (ebuf + 1), ep->filename + i * CHAR_LONG_NAME, 5);
        wnstr_lazy((wchar *) (ebuf + 14), ep->filename + i * CHAR_LONG_NAME + 5, 6);
        wnstr_lazy((wchar *) (ebuf + 28), ep->filename + i * CHAR_LONG_NAME + 11, 2);
        rw_clus(dp->cur_clus, 1, 0, (uint64)ebuf, off, sizeof(ebuf));
        off = reloc_clus(dp, off + 32);
    }
    ep->dirty = 1;
    eupdate(ep);
    ep->valid = 1;
    eunlock(ep);
    return ep;
}

struct dirent *edup(struct dirent *entry)
{
    if (entry != 0) {
        acquire(&ecache.lock);
        entry->ref++;
        release(&ecache.lock);
    }
    return entry;
}

void eupdate(struct dirent *entry)
{
    if (!entry->dirty) { return; }
    printf("[eupdate] %s\n", entry->filename);
    uint entcnt;
    uint32 clus = entry->parent->first_clus;
    uint32 off = entry->off % fat.byts_per_clus;
    for (uint clus_cnt = entry->off / fat.byts_per_clus; clus_cnt > 0; clus_cnt--) {
        clus = read_fat(clus);
    }
    rw_clus(clus, 0, 0, (uint64) &entcnt, off, 1);
    entcnt &= ~LAST_LONG_ENTRY;
    off += (entcnt << 5);
    if (off / fat.byts_per_clus != 0) {
        clus = read_fat(clus);
        off %= fat.byts_per_clus;
    }
    uint16 clus_high = (uint16)(entry->first_clus >> 16);
    uint16 clus_low = (uint16)(entry->first_clus & 0xff);
    rw_clus(clus, 1, 0, (uint64) &entry->attribute, off + 11, sizeof(entry->attribute));
    rw_clus(clus, 1, 0, (uint64) &clus_high, off + 20, sizeof(clus_high));
    rw_clus(clus, 1, 0, (uint64) &clus_low, off + 26, sizeof(clus_low));
    rw_clus(clus, 1, 0, (uint64) &entry->file_size, off + 28, sizeof(entry->file_size));
    entry->dirty = 0;
}

void etrunc(struct dirent *entry)
{
    uint entcnt;
    uint32 clus = entry->parent->first_clus;
    uint32 off = entry->off % fat.byts_per_clus;
    for (uint clus_cnt = entry->off / fat.byts_per_clus; clus_cnt > 0; clus_cnt--) {
        clus = read_fat(clus);
    }
    rw_clus(clus, 0, 0, (uint64) &entcnt, off, 1);
    entcnt &= ~LAST_LONG_ENTRY;
    uint8 flag = EMPTY_ENTRY;
    for (int i = 0; i <= entcnt; i++) {
        rw_clus(clus, 1, 0, (uint64) &flag, off, 1);
        off += 32;
        if (off / fat.byts_per_clus != 0) {
            off %= fat.byts_per_clus;
            clus = read_fat(clus);
        }
    }
    entry->valid = 0;
    for (clus = entry->first_clus; clus < FAT32_EOC; ) {
        uint32 next = read_fat(clus);
        free_clus(clus);
        clus = next;
    }
}

void elock(struct dirent *entry)
{
    if (entry == 0 || entry->ref < 1)
        panic("elock");
    acquiresleep(&entry->lock);
}

void eunlock(struct dirent *entry)
{
    if (entry == 0 || !holdingsleep(&entry->lock) || entry->ref < 1)
        panic("eunlock");
    releasesleep(&entry->lock);
}

void eput(struct dirent *entry)
{
    acquire(&ecache.lock);
    if (entry->valid && entry->ref == 1) {
        // ref == 1 means no other process can have entry locked,
        // so this acquiresleep() won't block (or deadlock).
        acquiresleep(&entry->lock);
        release(&ecache.lock);
        if (entry != &root) {
            entry->next->prev = entry->prev;
            entry->prev->next = entry->next;
            entry->next = root.next;
            entry->prev = &root;
            root.next->prev = entry;
            root.next = entry;
            eupdate(entry);
            eput(entry->parent);
        }
        releasesleep(&entry->lock);
        acquire(&ecache.lock);
    }
    entry->ref--;
    release(&ecache.lock);
}

void estat(struct dirent *entry, struct stat *st)
{
    strncpy(st->name, entry->filename, STAT_MAX_NAME);
    st->type = (entry->attribute & ATTR_DIRECTORY) ? T_DIR : T_FILE;
    st->dev = entry->dev;
    st->size = entry->file_size;
}

/**
 * Read filename from directory entry.
 * @param   buffer      pointer to the array that stores the name
 * @param   raw_entry   pointer to the entry in a sector buffer
 * @param   islong      if non-zero, read as l-n-e, otherwise s-n-e.
 */
static void read_entry_name(char *buffer, uint8 *raw_entry, int islong)
{
    if (islong) {                       // long entry branch
        snstr(buffer, (wchar *) (raw_entry + 1), 5);
        snstr(buffer + 5, (wchar *) (raw_entry + 14), 6);
        snstr(buffer + 11, (wchar *) (raw_entry + 28), 2);
    } else {
        // assert: only "." and ".." will enter this branch
        memset(buffer, 0, 12 << 1);
        int i = 7;
        if (raw_entry[i] == ' ') {
            do {
                i--;
            } while (i >= 0 && raw_entry[i] == ' ');
        }
        i++;
        memmove(buffer, raw_entry, i);
        if (raw_entry[8] != ' ') {
            memmove(buffer + i + 1, raw_entry + 8, 3);
            buffer[i] = '.';
        }
    }
}

/**
 * Read entry_info from directory entry.
 * @param   entry       pointer to the structure that stores the entry info
 * @param   raw_entry   pointer to the entry in a sector buffer
 */
static void read_entry_info(struct dirent *entry, uint8 *raw_entry)
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
    entry->cur_clus = entry->first_clus;
    entry->clus_cnt = 0;
}
/**
 * Read a directory from off
 * 
 * 
 */
int enext(struct dirent *dp, struct dirent *ep, uint off, int *count)
{
    if (!(dp->attribute & ATTR_DIRECTORY))
        panic("enext not dir");
    if (ep->valid)
        panic("enext ep valid");
    if (off % 32)
        panic("enext not align");

    uint8 ebuf[32];
    int cnt = 0;
    memset(ep->filename, 0, FAT32_MAX_FILENAME + 1);
    for (uint off2 = reloc_clus(dp, off); dp->cur_clus < FAT32_EOC; off2 = reloc_clus(dp, off2 + 32)) {
        if (rw_clus(dp->cur_clus, 0, 0, (uint64)ebuf, off2, 32) != 32 || ebuf[0] == END_OF_ENTRY) {
            return -1;
        }
        if (ebuf[0] == EMPTY_ENTRY) {
            cnt++;
            continue;
        } else if (cnt) {
            *count = cnt;
            return 0;
        }
        if (ebuf[11] == ATTR_LONG_NAME) {
            int lcnt = ebuf[0] & ~LAST_LONG_ENTRY;
            if (ebuf[0] & LAST_LONG_ENTRY) {
                *count = lcnt + 1;                              // plus the s-n-e;
                count = 0;
            }
            read_entry_name(ep->filename + (lcnt - 1) * CHAR_LONG_NAME, ebuf, 1);
        } else {
            if (count) {
                *count = 1;
                read_entry_name(ep->filename, ebuf, 0);
            }
            read_entry_info(ep, ebuf);
            return 1;
        }
    }
    return -1;
}

/**
 * Seacher for the entry in a directory and return a structure.
 * @param   entry       entry of a directory file
 * @param   filename    target filename
 * @param   poff        offset of proper empty entries slot from the dir
 */
struct dirent *dirlookup(struct dirent *entry, char *filename, uint *poff)
{
    if (!(entry->attribute & ATTR_DIRECTORY))
        panic("dirlookup not DIR");
    if (strncmp(filename, ".", FAT32_MAX_FILENAME) == 0) {
        return edup(entry);
    } else if (strncmp(filename, "..", FAT32_MAX_FILENAME) == 0) {
        return edup(entry->parent);
    }
    struct dirent *de = eget(entry, filename);
    if (de->valid) { return de; }                               // ecache hits

    int len = strlen(filename);
    int entcnt = (len + CHAR_LONG_NAME - 1) / CHAR_LONG_NAME + 1;   // count of l-n-entries, rounds up
    int count = 0;
    int type;
    uint off = reloc_clus(entry, 0);

    while ((type = enext(entry, de, off, &count) != -1)) {
        if (type == 0) {
            if (poff && count >= entcnt) {
                *poff = off;
                poff = 0;
            }
        } else if (strncmp(filename, de->filename, FAT32_MAX_FILENAME) == 0) {
            de->parent = edup(entry);
            de->off = off;
            de->valid = 1;
            return de;
        }
        off = reloc_clus(entry, off + count * 32);
    }
    if (poff) {
        *poff = off;
    }
    eput(de);
    return 0;
}

static char *skipelem(char *path, char *name)
{
    while (*path == '/') {
        path++;
    }
    if (*path == 0) { return 0; }
    char *s = path;
    while (*path != '/' && *path != 0) {
        path++;
    }
    int len = path - s;
    if (len > FAT32_MAX_FILENAME) {
        len = FAT32_MAX_FILENAME;
    } else {
        name[len] = 0;
    }
    memmove(name, s, len);
    while (*path == '/') {
        path++;
    }
    return path;
}

// FAT32 version of namex in xv6's original file system.
static struct dirent *lookup_path(char *path, int parent, char *name)
{
    struct dirent *entry, *next;
    if (*path == '/') {
        entry = edup(&root);
    } else {
        entry = edup(myproc()->cwd);
    }
    while ((path = skipelem(path, name)) != 0) {
        elock(entry);
        if (!(entry->attribute & ATTR_DIRECTORY)) {
            eunlock(entry);
            eput(entry);
            return 0;
        }
        if (parent && *path == '\0') {
            eunlock(entry);
            return entry;
        }
        if ((next = dirlookup(entry, name, 0)) == 0) {
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

struct dirent *ename(char *path)
{
    char name[FAT32_MAX_FILENAME + 1] = {0};
    return lookup_path(path, 0, name);
}

struct dirent *enameparent(char *path, char *name)
{
    return lookup_path(path, 1, name);
}
