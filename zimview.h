#define BLOCKSIZE 0x10000 //try to avoid allocating more than 1 meg of memory during file operations

struct sZimHeader //Endian oblivious - used to fread direct from zim
{
  DWORD dwChecksum; //Adler-32 of all the file (minus checksum)
  WORD  wCustomerNumber; //probably Little Endian
  WORD  wNumBlocks;
  DWORD dwTotalFileLen;
};

struct sBlockHeader //Endian oblivious - used to fread direct from zim
{
  char  name[4]; // 'ROOT', 'CODE', 'VERI', 'BOXI', 'KERN', 'LOAD','NVRM'
  DWORD dwDataLength;
  DWORD reserved[2]; // zero-filled
  CHAR  blockSignature[4]; // 'B','S', 0x00, 0x00
  DWORD dwChecksum; // Broken Adler-32
};

typedef struct internalBlockStructure BLOCK_STRUCTURE;
typedef struct internalZimStructure ZIM_STRUCTURE;
typedef struct boxiStructure BOXI_STRUCTURE;
typedef struct boxiBlockStructure BOXIBLOCK_STRUCTURE;
typedef struct veriStructure VERI_STRUCTURE;
typedef struct veriBlockStructure VERIBLOCK_STRUCTURE;
typedef struct usualBlockStructure USUAL_STRUCTURE;

#define BID_DWORD_ROOT_SWAP 0x524F4F54
#define BID_DWORD_CODE_SWAP 0x434F4445
#define BID_DWORD_VERI_SWAP 0x56455249
#define BID_DWORD_BOXI_SWAP 0x424F5849
#define BID_DWORD_KERN_SWAP 0x4B45524E
#define BID_DWORD_LOAD_SWAP 0x4C4F4144
#define BID_DWORD_NVRM_SWAP 0x4E56524D

#define BID_DWORD_ROOT 0x544f4F52
#define BID_DWORD_CODE 0x45444F43
#define BID_DWORD_VERI 0x49524556
#define BID_DWORD_BOXI 0x49584F42
#define BID_DWORD_KERN 0x4E52454B
#define BID_DWORD_LOAD 0x44414F4C
#define BID_DWORD_NVRM 0x4D52564E


#define BTYPE_ROOT 1
#define BTYPE_CODE 2
#define BTYPE_VERI 3
#define BTYPE_BOXI 4
#define BTYPE_KERN 5
#define BTYPE_LOAD 6
#define BTYPE_NVRM 7

struct veriBlockStructure {
	char  blockName[4];
	char  version_d;
    char  version_c;
    char  version_b;
    char  version_a; // version = a.b.d (c never appears to be reported)
    DWORD reserved[2]; // 0xFF filled
    BYTE  md5Digest[16]; // Calculated on the relevant Block.data
};

struct veriStructure {
	VERIBLOCK_STRUCTURE veriFileData;
	WORD numberVeriObjects;
	char displayblockname[5];
	struct veriStructure *nextStructure;
	struct veriStructure *prevStructure;
};

struct boxiBlockStructure {
  WORD  uiOUI;
  WORD  reserved[3]; // 0x00 filled
  WORD  wHwVersion;
  WORD  wSwVersion;
  WORD  wHwModel;
  WORD  wSwModel;
  char  abStarterMD5Digest[16];
  DWORD uiStarterImageSize;
};

struct boxiStructure {
	BOXIBLOCK_STRUCTURE boxiFileData;
};

struct usualBlockStructure {
	char displaymagicnumber[5]; //room for null-terminated string
	DWORD magicnumber;
	SQUASHFS_SUPER_BLOCK *sqshHeader;
	GZIP_HEADER_BLOCK *gzipHeader;
};

struct internalZimStructure //used internally
{
 char displayFilename[MAX_PATH];
 char *displayFilenameNoPath;
 FILE *pZimFile;
 DWORD dwChecksum;
 WORD wCustomerNumber;
 WORD wNumBlocks;
 DWORD dwTotalFileLen; //from the header
 DWORD *dwBlockStartLocArray;
 DWORD dwRealFileLen; //using fseek
 DWORD dwRealChecksum; //calculated checksum

 BLOCK_STRUCTURE *first; //first block in linked list of blocks
};

#define BSFLAG_INMEMORY		0x01 //the block can be represented without needing external file
#define BSFLAG_DONTWRITE	0x02 //don't write the block when saving
#define BSFLAG_ISSELECTED	0x04 //the block is selected

struct internalBlockStructure //used internally
{
  CHAR  name[5]; // 'ROOT', 'CODE', 'VERI', 'BOXI', 'KERN', 'LOAD','NVRM'. (an extra space for terminating string)
  DWORD dwDataLength;
  DWORD reserved[2]; // zero-filled
  CHAR  blockSignature[4]; // 'B','S', 0x00, 0x00
  DWORD dwChecksum; //actual checksum (Adler-32) in the file
  DWORD dwRealChecksum; //calculated checksum

  DWORD dwBlockStartLoc; //the location of the start of the block data in the main zim file
  DWORD dwDestStartLoc; //when we write a block to a new zim file, this is where. this is calculated before Save.

  unsigned char md5[16];

  char typeOfBlock; //1=ROOT, 2=CODE, 3=VERI, 4=BOXI, 5=KERN, 6=LOAD, 7=NVRM
  void *ptrFurtherBlockDetail; //pointer to a structure that holds further specific information about the block
  char sourceFilename[MAX_PATH]; //link to external file
  DWORD dwSourceOffset; //where the data starts in this external block

  char flags; //bit 0b00000001 true:memory, false:zimfile, bit 0b00000010 true:don't write, false:to be written

  BLOCK_STRUCTURE * next; //pointer to next block in linked list.
};

#define LOADZIM_ERR_SUCCESS 0 //Successful load of Zim file
#define LOADZIM_ERR_SIZEDESCREP 1 //Discrepancy between size in header and actual file size (can still load)
#define LOADZIM_ERR_SIZEIMPOSSIBLE 2 //Real size is too small for even a header
#define LOADZIM_ERR_BLOCKOUTOFBOUNDS 3 // The stated block location is too big, small or negative to be real
#define LOADZIM_ERR_BLOCKNUMBERUNLIKELY 4 //The number of stated blocks won't fit
#define LOADZIM_ERR_BLOCKNUMBERIMPOSSIBLE 5 //This blocknumber is too big or small (no malloc)
#define LOADZIM_ERR_BLOCKSIZEIMPOSSIBLE 6 //The size of a block is either too small or too large
#define LOADZIM_ERR_BLOCKSIGWRONG 7 //The signiture BS00 is not present
#define LOADZIM_ERR_MEMORYPROBLEM 8 //Unable to malloc (i.e. malloc returns NULL)

#define EXPORTBLOCK_ERR_SUCCESS 0
#define EXPORTBLOCK_ERR_MEMORYPROBLEM 8


int MD5HexString(char * outputString, char * MD5array);
int MD5StringToArray(char * MD5array, char * inputString);

DWORD DWORD_swap_endian(DWORD dw);
WORD WORD_swap_endian(WORD w);

int OpenZimFile(HWND hwnd, ZIM_STRUCTURE *ZimToOpen, char * filename);
int LoadZimFile(ZIM_STRUCTURE *LoadedZim);
int CloseZimFile(ZIM_STRUCTURE *LoadedZim);
int ActivateZimFile(ZIM_STRUCTURE *LoadedZim);

int WriteBlockToFile(ZIM_STRUCTURE *LoadedZim, BLOCK_STRUCTURE *Block, FILE *exportFile, int includeHeader);
int WriteZimFile(ZIM_STRUCTURE *LoadedZim, FILE *outputZim);
int SaveAsZim(ZIM_STRUCTURE *LoadedZim);

void *NewBlock(ZIM_STRUCTURE *LoadedZim);
void *ReadUsualBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start);
void *ReadBoxiBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start);
void *ReadVeriBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start);

int GenerateBlockHeader(struct sBlockHeader *blockHeader, DWORD dwDataLength, DWORD dwChecksum, char *name);
WORD CalculateOffsetForWriting(ZIM_STRUCTURE *LoadedZim);

int PaintWindow(HWND hwnd);
int DrawCaret(HDC hdc, RECT *lpRect, COLORREF colour1, COLORREF colour2);
void WINAPI InitMenu(HMENU hmenu);
BOOL _stdcall BlockExportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall BlockImportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall PropertiesDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int DetectTypeOfBlock(HWND hwnd, char *filename);
int DisableSelected(ZIM_STRUCTURE *LoadedZim);
int DeleteBlock(ZIM_STRUCTURE *LoadedZim, WORD blocknumber);
int DeleteSelectedBlocks(ZIM_STRUCTURE *LoadedZim);
int SwapBlocks(ZIM_STRUCTURE *LoadedZim, WORD blockA, WORD blockB);
int MoveBlockAfter(ZIM_STRUCTURE *LoadedZim, int blockSource, int blockDest);

BLOCK_STRUCTURE *GetBlockNumber(ZIM_STRUCTURE *LoadedZim, WORD blocknumber);
int FreeBlockMemory(BLOCK_STRUCTURE *Block);

int SelectToggleBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD blockToSelect);
int	SelectOnlyBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD blockToSelect);
int SelectRangeBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD startblock, WORD endblock);
int SelectAllBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim);
int SelectNoBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim);
int SelectedCount(ZIM_STRUCTURE *LoadedZim); //returns number of selected items
int SelectedFirst(ZIM_STRUCTURE *LoadedZim, BLOCK_STRUCTURE **ptrBlock); //returns first selected item, and put pointer into ptrBlock

void MouseBlockSelect(int *ptrBlockNumber, HWND hwnd, ZIM_STRUCTURE *LoadedZim, int x, int y);
void MouseBlockMoveSelect(int *ptrBlockNumber, HWND hwnd, ZIM_STRUCTURE *LoadedZim, int x, int y);

long OnMouseWheel(HWND hwnd, ZIM_STRUCTURE *LoadedZim, short nDelta);
void ScrollUpdate(HWND hwnd, ZIM_STRUCTURE *LoadedZim);

int RedrawSelectedBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim);
int RedrawBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int block);
int RedrawBetweenBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int startblock, int endblock);
int RedrawMoveIndicator(HWND hwnd, int loc);

void BlockExportDetailsUpdate(HWND hwnd, int blockid);
int PopulatePopupMenu(HMENU hMenu);

