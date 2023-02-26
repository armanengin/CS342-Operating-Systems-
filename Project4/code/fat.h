#ifndef FAT_H
#define FAT_H

#define SECTORSIZE 512
#define CLUSTERSIZE 1024

#define 	bitwise
#define 	__bitwise
#define 	aligned_u64   __u64 __attribute((aligned(8)))
#define 	aligned_be64   __be64 __attribute((aligned(8)))
#define 	aligned_le64   __le64 __attribute((aligned(8)))

#include <asm/types.h>
#include <linux/posix_types.h>

typedef __u16 __bitwise 	__le16;
typedef __u16 __bitwise 	__be16;
typedef __u32 __bitwise 	__le32;
typedef __u32 __bitwise 	__be32;
typedef __u64 __bitwise 	__le64;
typedef __u64 __bitwise 	__be64;
typedef __u16 __bitwise 	__sum16;
typedef __u32 __bitwise 	__wsum;


struct fat_boot_sector {
	__u8	ignored[3];	/* Boot strap short or near jump */
	__u8	system_id[8];	/* Name - can be used to special case
				   partition manager volumes */
	__u8	sector_size[2];	/* bytes per logical sector */
	__u8	sec_per_clus;	/* sectors/cluster */
	__le16	reserved;	/* reserved sectors */
	__u8	fats;		/* number of FATs */
	__u8	dir_entries[2];	/* root directory entries */
	__u8	sectors[2];	/* number of sectors */
	__u8	media;		/* media code */
	__le16	fat_length;	/* sectors/FAT */
	__le16	secs_track;	/* sectors per track */
	__le16	heads;		/* number of heads */
	__le32	hidden;		/* hidden sectors (unused) */
	__le32	total_sect;	/* number of sectors (if sectors == 0) */
    __le32	length;		/* sectors/FAT */
    __le16	flags;		/* bit 8: fat mirroring,
						   low 4: active fat */
    __u8	version[2];	/* major, minor filesystem
                        version */
    __le32	root_cluster;	/* first cluster in
						   root directory */
    __le16	info_sector;	/* filesystem info sector */
    __le16	backup_boot;	/* backup boot sector */
    __le16	reserved2[6];	/* Unused */
			/* Extended BPB Fields for FAT32 */
    __u8	drive_number;   /* Physical drive number */
    __u8    state;       	/* undocumented, but used
						   for mount state. */
    __u8	signature;  /* extended boot signature */
    __u8	vol_id[4];	/* volume ID */
    __u8	vol_label[11];	/* volume label */
    //__u8	vol_label[MSDOS_NAME];	/* volume label */
    __u8	fs_type[8];		/* file system type */
    /* other fields are not added here */

};

struct msdos_dir_entry {
    uint8_t name[MSDOS_NAME]; /* name and extension */
    uint8_t attr; /* attribute bits */
    uint8_t lcase; /* Case for base and extension */
    uint8_t ctime_cs; /* Creation time, centiseconds (0-199) */
    uint16_t ctime; /* Creation time */
    uint16_t cdate; /* Creation date */
    uint16_t adate; /* Last access date */
    uint16_t starthi; /* High 16 bits of cluster in FAT32 */
    uint16_t time,date,start; /* time, date and first cluster */
    uint32_t size; /* file size (in bytes) */
};



#endif