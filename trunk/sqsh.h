/* this contains the desquashing stuff */

#define SQUASHFS_MAGIC                  0x73717368
#define SQUASHFS_MAGIC_SWAP             0x68737173
#define SQUASHFS_MAGIC_LZMA             0x71736873
#define SQUASHFS_MAGIC_LZMA_SWAP        0x73687371



typedef long long   squashfs_block_t;
typedef long long   squashfs_inode_t;

struct squashfs_super_block {
	unsigned int	s_magic;
	unsigned int	inodes;
	unsigned int	bytes_used_2;
	unsigned int	uid_start_2;
	unsigned int	guid_start_2;
	unsigned int	inode_table_start_2;
	unsigned int	directory_table_start_2;
	WORD	s_major;
	WORD	s_minor;
	WORD	block_size_1;
	WORD	block_log;
	unsigned char	flags;
	unsigned char	no_uids;
	unsigned char	no_guids;
	unsigned int	mkfs_time; //time of filesystem creation
	squashfs_inode_t	root_inode;
	unsigned int	block_size;
	unsigned int	fragments;
	unsigned int	fragment_table_start_2;
	long long		bytes_used;
	long long		uid_start;
	long long		guid_start;
	long long		inode_table_start;
	long long		directory_table_start;
	long long		fragment_table_start;
	long long		lookup_table_start;
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
	//we read the above separately - note from below this is not the order things occur in the file
	char filename[MAX_PATH];
};

typedef struct gzip_header_block GZIP_HEADER_BLOCK;

#define GZIP_FTEXT	0x1
#define GZIP_FHCRC	0x2
#define GZIP_FEXTRA	0x4
#define GZIP_FNAME	0x8
#define GZIP_FCOMMENT	0x10
