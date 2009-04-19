/* this contains the desquashing stuff */

#define SQUASHFS_MAGIC                  0x73717368
#define SQUASHFS_MAGIC_SWAP             0x68737173
#define SQUASHFS_MAGIC_LZMA             0x71736873
#define SQUASHFS_MAGIC_LZMA_SWAP        0x73687371



typedef long long               squashfs_block_t;
typedef long long               squashfs_inode_t;

struct squashfs_super_block {
	unsigned int            s_magic;
    unsigned int            inodes;
    unsigned int            bytes_used_2;
    unsigned int            uid_start_2;
    unsigned int            guid_start_2;
    unsigned int            inode_table_start_2;
    unsigned int            directory_table_start_2;
    unsigned int            s_major:16;
    unsigned int            s_minor:16;
    unsigned int            block_size_1:16;
    unsigned int            block_log:16;
    unsigned int            flags:8;
    unsigned int            no_uids:8;
    unsigned int            no_guids:8;
    unsigned int            mkfs_time /* time of filesystem creation */;
    squashfs_inode_t        root_inode;
    unsigned int            block_size;
    unsigned int            fragments;
    unsigned int            fragment_table_start_2;
    long long               bytes_used;
    long long               uid_start;
    long long               guid_start;
    long long               inode_table_start;
    long long               directory_table_start;
    long long               fragment_table_start;
    long long               lookup_table_start;
};

typedef struct squashfs_super_block SQUASHFS_SUPER_BLOCK;


struct gzip_header_block {
	char id1;
	char id2;
	char cm;
	char flg;
	DWORD mtime;
	char xfl;
	char os;
};

typedef struct gzip_header_block GZIP_HEADER_BLOCK;
