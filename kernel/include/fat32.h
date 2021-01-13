#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      0x0F

#define LAST_LONG_ENTRY     0x40
#define FAT32_EOC           0x0ffffff8
#define EMPTY_ENTRY         0xe5
#define END_OF_ENTRY        0x00
#define CHAR_LONG_NAME      13

#define FAT32_MAX_FILENAME  255
#define FAT32_MAX_PATH      260
#define ENTRY_CACHE_NUM     50

struct dir_entry {
    // uchar   path[260];
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

    /* for OS */
    uint     dev;
    int     valid;
    int     ref;
    uint32  parent;     // because FAT32 doesn't have such thing like inum, use this for cache trick
    uint32  off;        // offset in the parent dir entry, for writing convenience
    struct dir_entry *next;
    struct dir_entry *prev;
    struct sleeplock    lock;
};
