#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "zinwellres.h"
#include "sqsh.h"
#include "md5.c"

#define BLOCKSIZE 0x10000 //try to avoid allocating more than 1 meg of memory during file operations

int MD5HexString(char * outputString, char * MD5array);
int MD5StringToArray(char * MD5array, char * inputString);

DWORD DWORD_swap_endian(DWORD dw);
WORD WORD_swap_endian(WORD w);

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

int RedrawSelectedBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim);
int RedrawBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int block);
int RedrawBetweenBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int startblock, int endblock);
int RedrawMoveIndicator(HWND hwnd, int loc);

int EditCopySelected(HWND hwnd, ZIM_STRUCTURE *LoadedZim, BOOL bCut);
int EditPaste(HWND hwnd, ZIM_STRUCTURE *LoadedZim);
UINT uZimBlockFormat;

void BlockExportDetailsUpdate(HWND hwnd, int blockid);
int PopulatePopupMenu(HMENU hMenu);

int isFocused=0;
int showCaret=0;
int mouseBeenPushed=0;
int draggingBlock=0;
int moveIndicatorLocation=-1;
#define MOVEINDICATOR_WIDTH 6
#define EDGE_MARGIN 4
#define ICON_SIZE 48

int caretedBlock=0; //the block that has caret
int startRangeSelectedBlock=0; //where to start a range selection from (i.e. when shift is held)
int topDisplayBlock=0;
int numberDisplayedBlocks=0;		//used for calculating what to draw
int numberFullyDisplayedBlocks=0;	//used for calculating scrolling (a half block displayed should not be counted, and it should include the highest number that fits)
RECT paintSelectRects[255];

struct internalZimStructure pZim;

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed int int16_t;
typedef unsigned int uint16_t;
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
typedef signed long long int int64_t;
typedef unsigned long long int uint64_t;

#define MOD_ADLER 65521

/*Because I will be calculating adler-32 for 'parts' of file
  (e.g. in blocks that checksum wraps around, or chunks of
  data that are not all in a file) I need to hold the adler value*/
struct adlerstruct
{
	unsigned long int a;
	unsigned long int b;
};

typedef struct adlerstruct ADLER_STRUCTURE;

//memset the adlerhold to zero to begin
unsigned long int ChecksumAdler32(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len)
{
	unsigned long int a;
	unsigned long int b;

	a=adlerhold->a;
	b=adlerhold->b;

    while (len != 0)
    {
        a = (a + *data++) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;

        len--;
    }

	adlerhold->a=a;
	adlerhold->b=b;

    return (b << 16) | a;
}

unsigned long int AdlerOnFile(FILE *fileToRead, ADLER_STRUCTURE *adlerhold, DWORD offset, DWORD len)
{
	char *buffer;
	DWORD finish;		//the last byte to read
	DWORD lengthtoread; //how much to read each step (should not exceed BLOCKSIZE)

	if (len==0) {
		return (adlerhold->b << 16) | adlerhold->a;
	}

	buffer=malloc(min(BLOCKSIZE, len)); //allocate some space to read chunks of the file
	if (buffer==NULL) {
		adlerhold->a=0xDE; adlerhold->b=0xAD;
		return 0; //if not enough memory, return 0 with inconsistent DE AD in adlerhold.
	}

	finish=offset+len;
	fseek(fileToRead, offset, SEEK_SET);
	while (offset<finish)	{
		lengthtoread=min(BLOCKSIZE, finish-offset);
		fread(buffer, lengthtoread, 1, fileToRead);
		ChecksumAdler32(adlerhold, buffer, lengthtoread);
		offset+=lengthtoread;
	}
	free(buffer);

	return (adlerhold->b << 16) | adlerhold->a;
};

void MD5OnFile(FILE *fileToRead, struct cvs_MD5Context *ctx, DWORD offset, DWORD len)
{
	char *buffer;
	DWORD finish;		//the last byte to read
	DWORD lengthtoread; //how much to read each step (should not exceed BLOCKSIZE)

	if (len==0) {
		return;
	}

	buffer=malloc(min(BLOCKSIZE, len));
	if (buffer==NULL) return;

	finish=offset+len;
	fseek(fileToRead, offset, SEEK_SET);
	while (offset<finish)	{
		lengthtoread=min(BLOCKSIZE, finish-offset);
		fread(buffer, lengthtoread, 1, fileToRead);
		cvs_MD5Update(ctx, buffer, lengthtoread);
		offset+=lengthtoread;
	}
	free(buffer);

	return;
};



/*<---------------------------------------------------------------------->*/
HINSTANCE hInst;		// Instance handle
HWND hwndMain;		//Main window handle

HWND hwndToolBar;
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg,WPARAM wParam,LPARAM lParam);

HWND  hWndStatusbar;

void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
    SendMessage(hWndStatusbar, SB_SETTEXT, partNumber | displayFlags, (LPARAM)lpszStatusString);
}


LRESULT MsgMenuSelect(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    static char szBuffer[256];
    UINT   nStringID = 0;
    UINT   fuFlags = GET_WM_MENUSELECT_FLAGS(wparam, lparam) & 0xffff;
    UINT   uCmd    = GET_WM_MENUSELECT_CMD(wparam, lparam);
    HMENU  hMenu   = GET_WM_MENUSELECT_HMENU(wparam, lparam);

    szBuffer[0] = 0;                            // First reset the buffer
    if (fuFlags == 0xffff && hMenu == NULL)     // Menu has been closed
        nStringID = 0;

    else if (fuFlags & MFT_SEPARATOR)           // Ignore separators
        nStringID = 0;

    else if (fuFlags & MF_POPUP)                // Popup menu
    {
        if (fuFlags & MF_SYSMENU)               // System menu
            nStringID = IDS_SYSMENU;
        else
            // Get string ID for popup menu from idPopup array.
            nStringID = 0;
    }  // for MF_POPUP
    else                                        // Must be a command item
        nStringID = uCmd;                       // String ID == Command ID

    // Load the string if we have an ID
    if (0 != nStringID)
        LoadString(hInst, nStringID, szBuffer, sizeof(szBuffer));
    // Finally... send the string to the status bar
    UpdateStatusBar(szBuffer, 0, 0);
    return 0;
}


void InitializeStatusBar(HWND hwndParent,int nrOfParts)
{
    int   ptArray[40];   // Array defining the number of parts/sections
    RECT  rect;
    HDC   hDC;

    hDC = GetDC(hwndParent);
    GetClientRect(hwndParent, &rect);

    ptArray[nrOfParts-1] = rect.right;

    ReleaseDC(hwndParent, hDC);
    SendMessage(hWndStatusbar, SB_SETPARTS, nrOfParts, (LPARAM)(LPINT)ptArray);

    UpdateStatusBar("Ready", 0, 0);
}


static BOOL CreateStatusBar(HWND hwndParent,char *initialText,int nrOfParts)
{
    hWndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER|SBARS_SIZEGRIP, initialText, hwndParent, IDM_STATUSBAR);
    if(hWndStatusbar)
    {
        InitializeStatusBar(hwndParent,nrOfParts);
        return TRUE;
    }

    return FALSE;
}

int GetFilename(char *buffer,int buflen)
{
	char tmpfilter[46];
	int i = 0;
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = buflen;
	ofn.lpstrTitle = "Open";
	ofn.nFilterIndex = 2;
	ofn.lpstrDefExt = "zim";
	strcpy(buffer,"*.zim");
	strcpy(tmpfilter,"All files,*.*,Zinwell Firmware (*.zim),*.zim");
	while(tmpfilter[i]) {
		if (tmpfilter[i] == ',')
			tmpfilter[i] = 0;
		i++;
	}
	tmpfilter[i++] = 0; tmpfilter[i++] = 0;
	ofn.Flags = OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = tmpfilter;
	return GetOpenFileName(&ofn);
}

static BOOL InitApplication(void)
{
	WNDCLASS wc;
	INITCOMMONCONTROLSEX InitCtrlEx;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "ZimViewWndClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_FIRST));
	if (!RegisterClass(&wc))
		return 0;

	//Register the clipboard format
	uZimBlockFormat = RegisterClipboardFormat("Zim Block Data");

	//Load common controls dll
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_TAB_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);


	return 1;
}

BOOL _stdcall AboutDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}

BOOL _stdcall ChangeCustomerNumberDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	char customerNumber[16];

	switch(msg) {
		case WM_INITDIALOG:
			sprintf(&customerNumber[0], "%i", pZim.wCustomerNumber);
			SetDlgItemText(hwnd, IDC_CUSTOMERNUMBER, &customerNumber[0]);
			break;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
				case IDOK:
					GetDlgItemText(hwnd, IDC_CUSTOMERNUMBER, &customerNumber[0], 16);
					pZim.wCustomerNumber=strtol(&customerNumber[0], NULL, 0);
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}



BOOL _stdcall BlockCreateVeriDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
BLOCK_STRUCTURE *newBlock;
BLOCK_STRUCTURE *tempBlockStruct;
VERI_STRUCTURE *tempVeriStruct;

struct sBlockHeader blockHeader; //temp to hold block to calculate adler
ADLER_STRUCTURE adlerholder;
struct cvs_MD5Context MD5context;

int i;
LVCOLUMN column;
LVITEM lvItem;
HWND hList;
int iItem;
int iSelect;

char veriBlockname[8];
char veriVersion[24];
char veriMD5[40];
char *pEnd;

int veriVerA, veriVerB, veriVerC, veriVerD;

WORD numberSelectedLB;
int *arraySelectedLB;

	switch(msg) {
		case WM_INITDIALOG:

			SetDlgItemText(hwnd, IDC_VERIVERA, "");
			SetDlgItemText(hwnd, IDC_VERIVERB, "");
			SetDlgItemText(hwnd, IDC_VERIVERC, "0");
			SetDlgItemText(hwnd, IDC_VERIVERD, "");

			tempBlockStruct=pZim.first;

			for (i=0;i<pZim.wNumBlocks;i++) {
				SendDlgItemMessage(hwnd, IDC_BLOCKLIST, LB_ADDSTRING, 0, (LPARAM)tempBlockStruct->name);
				tempBlockStruct=tempBlockStruct->next;
			}


			//Now do the listview stuff
			hList=GetDlgItem(hwnd,IDC_VERIBLOCKSTOADD);

			SendMessage(hList,LVM_SETEXTENDEDLISTVIEWSTYLE, 0,LVS_EX_FULLROWSELECT);

			column.mask=LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			column.fmt=LVCFMT_LEFT;
			column.pszText="Block";
			column.cchTextMax=5;
			column.cx=40;
			ListView_InsertColumn(hList,   0,    &column);

			column.pszText="Version";
			column.cchTextMax=5;
			column.cx=90;
			ListView_InsertColumn(hList,   1,    &column);

			column.pszText="MD5";
			column.cchTextMax=5;
			column.cx=204;
			ListView_InsertColumn(hList,   2,    &column);
			return 1;


		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_BLOCKADD:

				//First get the information
				//Version from the dialog box
				GetDlgItemText(hwnd, IDC_VERIVERA, &veriVersion[0],24);
				veriVerA=strtol(&veriVersion[0], NULL, 0);
				GetDlgItemText(hwnd, IDC_VERIVERB, &veriVersion[0],24);
				veriVerB=strtol(&veriVersion[0], NULL, 0);
				GetDlgItemText(hwnd, IDC_VERIVERC, &veriVersion[0],24);
				veriVerC=strtol(&veriVersion[0], NULL, 0);
				GetDlgItemText(hwnd, IDC_VERIVERD, &veriVersion[0],24);
				veriVerD=strtol(&veriVersion[0], NULL, 0);

				sprintf(veriVersion, "%i.%i.%i.%i",veriVerA, veriVerB, veriVerC, veriVerD);


				//Determine which blocks are selected in the listbox
				hList=GetDlgItem(hwnd,IDC_BLOCKLIST);
				numberSelectedLB=SendMessage(hList, LB_GETSELCOUNT, 0, 0);

				if (numberSelectedLB) {
					arraySelectedLB=malloc(numberSelectedLB * sizeof(int));
					SendMessage(hList, LB_GETSELITEMS, numberSelectedLB, (LPARAM)arraySelectedLB);


					for (i=0;i<numberSelectedLB; i++) {
				//Write info to the listview
						hList=GetDlgItem(hwnd,IDC_VERIBLOCKSTOADD);

 						iItem=SendMessage(hList,LVM_GETITEMCOUNT,0,0);

						tempBlockStruct=GetBlockNumber(&pZim, arraySelectedLB[i]);

						memset(&lvItem,0,sizeof(lvItem));
						lvItem.mask=LVIF_TEXT;
						lvItem.cchTextMax = 8;
						lvItem.iItem=iItem;
						lvItem.iSubItem=0;
						sprintf(&veriBlockname[0], "%s", tempBlockStruct->name);
						lvItem.pszText=veriBlockname;
						SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&lvItem);

						lvItem.cchTextMax = 24;
						lvItem.iSubItem=1;
						lvItem.pszText=veriVersion;
						SendMessage(hList,LVM_SETITEM,0,(LPARAM)&lvItem);

						lvItem.cchTextMax = 40;
						lvItem.iSubItem=2;
						MD5HexString(&veriMD5[0], tempBlockStruct->md5);
						lvItem.pszText=veriMD5;
						SendMessage(hList,LVM_SETITEM,0,(LPARAM)&lvItem);
					}

					free(arraySelectedLB);
				}

					break;

				case IDC_BLOCKREMOVE:
					hList=GetDlgItem(hwnd,IDC_VERIBLOCKSTOADD);
					iSelect=SendMessage(hList,LB_GETCARETINDEX,0,0);
					SendMessage(hList,LVM_DELETEITEM,iSelect,0); // delete the item selected
					break;
				case IDOK:
					hList=GetDlgItem(hwnd,IDC_VERIBLOCKSTOADD);
					numberSelectedLB=SendMessage(hList, LVM_GETITEMCOUNT, 0,0); //we'll use this var for the number of block in veri
					if (numberSelectedLB<1)	{
						MessageBox(hwnd, "One or more blocks need to be selected to create a verification block.", "Create VERI block", MB_OK|MB_ICONEXCLAMATION);
						return 1;
					}
					newBlock=NewBlock(&pZim);
					newBlock->dwDataLength= sizeof(VERIBLOCK_STRUCTURE) * numberSelectedLB;
					sprintf(newBlock->name, "VERI");
					newBlock->typeOfBlock=BTYPE_VERI;
					newBlock->ptrFurtherBlockDetail=malloc(sizeof(VERI_STRUCTURE)); //the internal structure
					tempVeriStruct=newBlock->ptrFurtherBlockDetail;
					tempVeriStruct->prevStructure=NULL;
					tempVeriStruct->numberVeriObjects=numberSelectedLB;

					//reset the checksums
					memset(&adlerholder, 0, sizeof(adlerholder));
					cvs_MD5Init(&MD5context);

					for (i=0; i<numberSelectedLB; i++) {
						tempVeriStruct->numberVeriObjects=numberSelectedLB;

						memset(&lvItem,0,sizeof(lvItem)); //first load the blockname
						lvItem.mask=LVIF_TEXT;
						lvItem.cchTextMax = 8;
						lvItem.iItem=i;
						lvItem.iSubItem=0;
						lvItem.pszText=veriBlockname;
						SendMessage(hList,LVM_GETITEM,0,(LPARAM)&lvItem);
						sprintf(tempVeriStruct->displayblockname, "%s", veriBlockname);
						tempVeriStruct->veriFileData.blockName[0]=tempVeriStruct->displayblockname[0];
						tempVeriStruct->veriFileData.blockName[1]=tempVeriStruct->displayblockname[1];
						tempVeriStruct->veriFileData.blockName[2]=tempVeriStruct->displayblockname[2];
						tempVeriStruct->veriFileData.blockName[3]=tempVeriStruct->displayblockname[3];

						memset(&lvItem,0,sizeof(lvItem)); //then the version
						lvItem.mask=LVIF_TEXT;
						lvItem.cchTextMax = 24;
						lvItem.iItem=i;
						lvItem.iSubItem=1;
						lvItem.pszText=veriVersion;
						SendMessage(hList,LVM_GETITEM,0,(LPARAM)&lvItem);
						tempVeriStruct->veriFileData.reserved[0]=0xFFFFFFFF;
						tempVeriStruct->veriFileData.reserved[1]=0xFFFFFFFF;
						tempVeriStruct->veriFileData.version_a=strtol(veriVersion, &pEnd, 0); //strtol reads until we get the . (which isn't valid)
						tempVeriStruct->veriFileData.version_b=strtol(pEnd+1, &pEnd, 0);
						tempVeriStruct->veriFileData.version_c=strtol(pEnd+1, &pEnd, 0);
						tempVeriStruct->veriFileData.version_d=strtol(pEnd+1, NULL, 0);

						memset(&lvItem,0,sizeof(lvItem)); //then the md5
						lvItem.mask=LVIF_TEXT;
						lvItem.cchTextMax = 40;
						lvItem.iItem=i;
						lvItem.iSubItem=2;
						lvItem.pszText=veriMD5;
						SendMessage(hList,LVM_GETITEM,0,(LPARAM)&lvItem);
						MD5StringToArray(tempVeriStruct->veriFileData.md5Digest, veriMD5);

						//Generate the Adler32 and MD5 on this section
						ChecksumAdler32(&adlerholder, &(tempVeriStruct->veriFileData), sizeof(VERIBLOCK_STRUCTURE));
						cvs_MD5Update (&MD5context, &(tempVeriStruct->veriFileData), sizeof(VERIBLOCK_STRUCTURE));
						//Now add another veri if there's another one to add
						if ((i+1)<numberSelectedLB) {
							tempVeriStruct->nextStructure=malloc(sizeof(VERI_STRUCTURE));
							tempVeriStruct->nextStructure->prevStructure=tempVeriStruct;
							tempVeriStruct=tempVeriStruct->nextStructure;
						}
						else
							tempVeriStruct->nextStructure=NULL;
					}

					//The MD5 is easy, as we don't need to include the block itself, just the data
					cvs_MD5Final (&newBlock->md5, &MD5context);
					//The Adler needs to be calculated with a blockheader
					GenerateBlockHeader(&blockHeader, newBlock->dwDataLength, 0, "VERI");
					newBlock->blockSignature[0]=blockHeader.blockSignature[0];
					newBlock->blockSignature[1]=blockHeader.blockSignature[1];
					newBlock->blockSignature[2]=blockHeader.blockSignature[2];
					newBlock->blockSignature[3]=blockHeader.blockSignature[3];

					newBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum


					InvalidateRect(hwndMain, NULL, TRUE);
					EndDialog(hwnd,1);
					return 1;
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}


BOOL _stdcall BlockCreateBoxiDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
BLOCK_STRUCTURE *newBlock;
BOXI_STRUCTURE *tempBoxiStruct;

ADLER_STRUCTURE adlerholder;
struct cvs_MD5Context MD5context;
struct sBlockHeader blockHeader;

char buffer[255];

	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					newBlock=NewBlock(&pZim);

					newBlock->typeOfBlock=BTYPE_BOXI;
					sprintf(newBlock->name, "BOXI");
					newBlock->dwDataLength= sizeof(BOXIBLOCK_STRUCTURE);

					newBlock->ptrFurtherBlockDetail=malloc(sizeof(BOXI_STRUCTURE));
					if (newBlock->ptrFurtherBlockDetail==NULL) {
						MessageBox(hwnd, "There is insufficient memory to create a new BOXI block.", "Create BOXI block", MB_OK|MB_ICONEXCLAMATION);
						return 0;
					}
					tempBoxiStruct=newBlock->ptrFurtherBlockDetail;
					memset(&tempBoxiStruct->boxiFileData, 0, sizeof(BOXIBLOCK_STRUCTURE));

					GetDlgItemText(hwnd, IDC_BOXIOUI, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.uiOUI=strtoul(&buffer[0], NULL, 0);
					GetDlgItemText(hwnd, IDC_BOXISTARTERIMAGESIZE, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.uiStarterImageSize=strtoul(&buffer[0], NULL, 0);
					GetDlgItemText(hwnd, IDC_BOXIHWV, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.wHwVersion=strtoul(&buffer[0], NULL, 0);
					GetDlgItemText(hwnd, IDC_BOXISWV, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.wSwVersion=strtoul(&buffer[0], NULL, 0);
					GetDlgItemText(hwnd, IDC_BOXIHWM, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.wHwModel=strtoul(&buffer[0], NULL, 0);
					GetDlgItemText(hwnd, IDC_BOXISWM, &buffer[0], 255);
					tempBoxiStruct->boxiFileData.wSwModel=strtoul(&buffer[0], NULL, 0);

					GetDlgItemText(hwnd, IDC_BOXISTARTERMD5, &buffer[0], 255);
					MD5StringToArray(tempBoxiStruct->boxiFileData.abStarterMD5Digest, &buffer[0]);

					//Now calculate MD5 and adler on this block
					//MD5 just on the data
					cvs_MD5Init(&MD5context);
					cvs_MD5Update (&MD5context, &(tempBoxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE));
					cvs_MD5Final (&newBlock->md5, &MD5context);
					//Adler32 on the data then block header
					memset(&adlerholder, 0, sizeof(adlerholder));
					ChecksumAdler32(&adlerholder, &(tempBoxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE)); //the boxi we loaded
					GenerateBlockHeader(&blockHeader, newBlock->dwDataLength, 0, "BOXI");
					newBlock->blockSignature[0]=blockHeader.blockSignature[0];
					newBlock->blockSignature[1]=blockHeader.blockSignature[1];
					newBlock->blockSignature[2]=blockHeader.blockSignature[2];
					newBlock->blockSignature[3]=blockHeader.blockSignature[3];

					newBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum

					InvalidateRect(hwndMain, NULL, TRUE);
					EndDialog(hwnd,1);
					return 1;
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}


BOOL _stdcall BlockImportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
OPENFILENAME ofnImport;
FILE *importFile;
int i;
char tmpfilter[255];
char buffer[MAX_PATH];

int blocktype;
UINT includesHeader;

struct sBlockHeader blockHeader;

BLOCK_STRUCTURE *newBlock;
BOXI_STRUCTURE *tempBoxiStruct;
VERI_STRUCTURE *tempVeriStruct;

int counterVeriPart;

ADLER_STRUCTURE adlerholder;
struct cvs_MD5Context MD5context;

	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_BLOCKIMPORTROOT:
				case IDC_BLOCKIMPORTCODE:
				case IDC_BLOCKIMPORTKERN:
				case IDC_BLOCKIMPORTBOXI:
				case IDC_BLOCKIMPORTVERI:
				case IDC_BLOCKIMPORTLOAD:
				case IDC_BLOCKIMPORTNVRM:
                    CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, LOWORD(wParam));
					return 1;
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
				case IDOK:
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTROOT)==BST_CHECKED) {blocktype=BTYPE_ROOT;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTKERN)==BST_CHECKED) {blocktype=BTYPE_KERN;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTCODE)==BST_CHECKED) {blocktype=BTYPE_CODE;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTVERI)==BST_CHECKED) {blocktype=BTYPE_VERI;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTBOXI)==BST_CHECKED) {blocktype=BTYPE_BOXI;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTNVRM)==BST_CHECKED) {blocktype=BTYPE_NVRM;}
					if (IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTLOAD)==BST_CHECKED) {blocktype=BTYPE_LOAD;}

					if (blocktype==0)	{
						MessageBox(hwnd, "Please select the type of block you want to import.","Import file",MB_OK|MB_ICONEXCLAMATION);
						return 0;
					}

					GetDlgItemText(hwnd, IDC_BLOCKIMPORTFILENAME, &buffer[0], 255);
					importFile=fopen(buffer, "rb");
					if (importFile==NULL) {
						MessageBox(hwnd, "This file cannot be opened. Please chose another file.","Import file",MB_OK|MB_ICONEXCLAMATION);
						return 0;
					}
					if (importFile) {
						newBlock=NewBlock(&pZim);
						if (newBlock==NULL) {
							MessageBox(hwnd, "There has been a memory error while importing this block.", "Import file", MB_OK|MB_ICONEXCLAMATION);
							return 0;
						}
						includesHeader = IsDlgButtonChecked(hwnd, IDC_BLOCKIMPORTINCLUDEHEADER);
						memset(newBlock, 0, sizeof(BLOCK_STRUCTURE));
						sprintf(newBlock->name, "TEST");
						newBlock->typeOfBlock=blocktype;
						sprintf(newBlock->sourceFilename, "%s", buffer);
						if (includesHeader==BST_UNCHECKED) newBlock->dwSourceOffset=0; else newBlock->dwSourceOffset=sizeof(struct sBlockHeader);

						sprintf(newBlock->name, "BLOK");
						if (newBlock->typeOfBlock==BTYPE_ROOT) sprintf(newBlock->name, "ROOT");
						if (newBlock->typeOfBlock==BTYPE_KERN) sprintf(newBlock->name, "KERN");
						if (newBlock->typeOfBlock==BTYPE_CODE) sprintf(newBlock->name, "CODE");
						if (newBlock->typeOfBlock==BTYPE_VERI) sprintf(newBlock->name, "VERI");
						if (newBlock->typeOfBlock==BTYPE_NVRM) sprintf(newBlock->name, "NVRM");
						if (newBlock->typeOfBlock==BTYPE_LOAD) sprintf(newBlock->name, "LOAD");
						if (newBlock->typeOfBlock==BTYPE_BOXI) sprintf(newBlock->name, "BOXI");

						if (includesHeader==BST_CHECKED) {
							fread(&blockHeader, sizeof(struct sBlockHeader), 1, importFile);
							fseek(importFile, 0, SEEK_END);
							newBlock->dwDataLength = ftell(importFile)-sizeof(struct sBlockHeader);
							rewind(importFile);
							if (((blockHeader.name[0]<65)||(blockHeader.name[0]>90))
							|| ((blockHeader.name[1]<65)||(blockHeader.name[1]>90))
							|| ((blockHeader.name[2]<65)||(blockHeader.name[2]>90))
							|| ((blockHeader.name[3]<65)||(blockHeader.name[3]>90)))
								{
									MessageBox(hwnd, "The block header seems malformed. This block cannot be imported with the selected settings.", "Import", MB_OK|MB_ICONEXCLAMATION);
									free(newBlock);
									pZim.wNumBlocks--;
									fclose(importFile);
									return 0;
								}
						}
						else {
							fseek(importFile, 0, SEEK_END);
							newBlock->dwDataLength = ftell(importFile);
							GenerateBlockHeader(&blockHeader, newBlock->dwDataLength, 0, newBlock->name);
							rewind(importFile);
						}

							newBlock->dwDataLength=DWORD_swap_endian(blockHeader.dwDataLength);
							newBlock->dwChecksum=DWORD_swap_endian(blockHeader.dwChecksum);
							newBlock->blockSignature[0]=blockHeader.blockSignature[0];
							newBlock->blockSignature[1]=blockHeader.blockSignature[1];
							newBlock->blockSignature[2]=blockHeader.blockSignature[2];
							newBlock->blockSignature[3]=blockHeader.blockSignature[3];

							newBlock->name[0]=blockHeader.name[0];
							newBlock->name[1]=blockHeader.name[1];
							newBlock->name[2]=blockHeader.name[2];
							newBlock->name[3]=blockHeader.name[3];
							newBlock->name[4]=0;


						//Load BOXI block and calculate checksum
						if (newBlock->typeOfBlock==BTYPE_BOXI) {
							if (includesHeader==BST_CHECKED)
								newBlock->ptrFurtherBlockDetail=ReadBoxiBlock(newBlock, importFile, sizeof(struct sBlockHeader));
							else
								newBlock->ptrFurtherBlockDetail=ReadBoxiBlock(newBlock, importFile, 0);

							tempBoxiStruct=newBlock->ptrFurtherBlockDetail;
							newBlock->dwDataLength=(sizeof(BOXIBLOCK_STRUCTURE));

							//Calculate checksum
							memset(&adlerholder, 0, sizeof(adlerholder));
							ChecksumAdler32(&adlerholder, &(tempBoxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE)); //the boxi we loaded
							newBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum

							//Calculate MD5 (this is done only on data, not the block header like the adler is
							cvs_MD5Init(&MD5context);
							cvs_MD5Update (&MD5context, &(tempBoxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE));
							cvs_MD5Final (&newBlock->md5, &MD5context);

							newBlock->flags=BSFLAG_INMEMORY;
						}
						//ROOT, CODE, KERN, NVRM, LOAD (usual) blocks - handle these the same
						if ((newBlock->typeOfBlock==BTYPE_ROOT)||(newBlock->typeOfBlock==BTYPE_CODE)||(newBlock->typeOfBlock==BTYPE_KERN)||(newBlock->typeOfBlock==BTYPE_NVRM)||(newBlock->typeOfBlock==BTYPE_LOAD)) {
							memset(&adlerholder, 0, sizeof(adlerholder)); //reset adler checksum to zero
							cvs_MD5Init(&MD5context); //init md5 context

							if (includesHeader==BST_UNCHECKED) {
								newBlock->ptrFurtherBlockDetail=ReadUsualBlock(newBlock, importFile, 0);
								AdlerOnFile(importFile, &adlerholder, 0, newBlock->dwDataLength);
								MD5OnFile(importFile, &MD5context, 0, newBlock->dwDataLength);
							}
							else { //if a header is there
								newBlock->ptrFurtherBlockDetail=ReadUsualBlock(newBlock, importFile, sizeof(struct sBlockHeader));
								AdlerOnFile(importFile, &adlerholder,  sizeof(struct sBlockHeader), newBlock->dwDataLength);
								MD5OnFile(importFile, &MD5context, sizeof(struct sBlockHeader), newBlock->dwDataLength);
							}
							newBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum
							cvs_MD5Final (&newBlock->md5, &MD5context);

							newBlock->flags=BSFLAG_INMEMORY;
						}
						//Load a VERI block
						if (newBlock->typeOfBlock==BTYPE_VERI) {
							if (includesHeader==BST_CHECKED)
								newBlock->ptrFurtherBlockDetail=ReadVeriBlock(newBlock, importFile, sizeof(struct sBlockHeader));
							else
								newBlock->ptrFurtherBlockDetail=ReadVeriBlock(newBlock, importFile, 0);
							//calculate checksum
							tempVeriStruct=newBlock->ptrFurtherBlockDetail;
							memset(&adlerholder, 0, sizeof(adlerholder));
							cvs_MD5Init(&MD5context);

							//perform adler on all verifiledata blocks
							counterVeriPart=tempVeriStruct->numberVeriObjects;
							while (counterVeriPart) {
								if (tempVeriStruct) {
									ChecksumAdler32(&adlerholder, &tempVeriStruct->veriFileData, sizeof(VERIBLOCK_STRUCTURE));
									cvs_MD5Update (&MD5context, &(tempVeriStruct->veriFileData), sizeof(VERIBLOCK_STRUCTURE));
									}
								counterVeriPart--;
								if (counterVeriPart) tempVeriStruct=tempVeriStruct->nextStructure;
							}
							newBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum
							cvs_MD5Final (&newBlock->md5, &MD5context);
							newBlock->flags=BSFLAG_INMEMORY;
						}
					fclose(importFile);
					}

					InvalidateRect(hwndMain, NULL, TRUE);
					EndDialog(hwnd,1);
					return 1;
				case IDC_BLOCKIMPORTAUTODETECT:
					GetDlgItemText(hwnd, IDC_BLOCKIMPORTFILENAME, &buffer[0], 255);
					DetectTypeOfBlock(hwnd, &buffer[0]);
					return 1;
				case IDBROWSE:
					memset(&ofnImport, 0, sizeof(ofnImport));
					ofnImport.lStructSize = sizeof(ofnImport);
					ofnImport.hInstance = GetModuleHandle(NULL);
					ofnImport.hwndOwner = GetActiveWindow();
					ofnImport.lpstrFile = buffer;
					ofnImport.nMaxFile = sizeof(buffer);
					ofnImport.lpstrTitle = "Browse";
					ofnImport.nFilterIndex = 6;
					ofnImport.lpstrDefExt = "zim";
					strcpy(buffer,"*.blok;*.sfs;*.sqsh;*.gz;*.data");
					strcpy(tmpfilter,"All files,*.*,SquashFS file (*.sfs; *.sqsh),*.sfs;*.sqsh,Whole blocks (*.blok),*.blok,Gzip file (*.gz),*.gz,Unknown data (*.data),*.data,All exported blocks,*.blok;*.sfs;*.sqsh;*.gz;*.data");
					i=0;
					while(tmpfilter[i]) {
						if (tmpfilter[i] == ',')
							tmpfilter[i] = 0;
						i++;
					}
					tmpfilter[i++] = 0; tmpfilter[i++] = 0;
					ofnImport.Flags = OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
					ofnImport.lpstrFilter = tmpfilter;
					if (GetOpenFileName(&ofnImport))	{
						SetDlgItemText(hwnd, IDC_BLOCKIMPORTFILENAME, ofnImport.lpstrFile);
						DetectTypeOfBlock(hwnd, ofnImport.lpstrFile);
					}
			}
			break;
	}
	return 0;
}

BOOL _stdcall PropertiesDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TCITEM tie;
	HANDLE hTab;
	int nSel;
	BLOCK_STRUCTURE *selectedBlock;
	char tempString[255];

	switch(msg) {
		case WM_INITDIALOG:
		    nSel = SelectedCount(&pZim);
			SelectedFirst(&pZim, &selectedBlock);

			hTab=GetDlgItem(hwnd, IDC_PROPERTIESTAB);

		    tie.mask = TCIF_TEXT | TCIF_IMAGE;
		    tie.iImage = -1;
		    tie.pszText = "General";
			TabCtrl_InsertItem(hTab, 0, &tie);

			if (nSel==1)	{
				sprintf(&tempString[0], "%s Properties", selectedBlock->name);
				SetWindowText(hwnd, tempString);

		    	tie.pszText = selectedBlock->name;
				TabCtrl_InsertItem(hTab, 1, &tie);
			}
		    else if (nSel>1)	{
				sprintf(&tempString[0], "Multiple (%i blocks selected) Properties", nSel);
				SetWindowText(hwnd, tempString);

			}
			else {
				sprintf(&tempString[0], "%s Properties", pZim.displayFilenameNoPath);
				SetWindowText(hwnd, tempString);
			}

			break;
		case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code)
            {
            case TCN_SELCHANGE:
            	//MessageBox(hwnd, "t", "t", 00);
				break;
        	default:
            	return DefWindowProc(hwnd, msg, wParam, lParam);
			}
			return 0;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
				case IDAPPLY:
					return 1;
				case IDOK:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}




BOOL _stdcall BlockExportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i;
	int selectedBlockId;
	char tempString[511]; //needs to accommodate a longish info string about each block
	BLOCK_STRUCTURE *tempBlockStruct;
	BLOCK_STRUCTURE selectedBlockStruct;
	OPENFILENAME ofnExportTo;
	FILE *filePtrExport;

	UINT includeHeader;
	USUAL_STRUCTURE *tempUsualStructure;

	switch(msg) {
		case WM_INITDIALOG:

			SetDlgItemText(hwnd, IDC_BLOCKEXPORTTYPE, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTSIZE, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTOFFSET, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTDETAIL, "");

			CheckDlgButton(hwnd, IDC_BLOCKEXPORTINCLUDEHEADER, BST_UNCHECKED);

			tempBlockStruct=pZim.first;

			selectedBlockId=-1; //we have no idea what block to export just yet
			for (i=0;i<pZim.wNumBlocks;i++) {
				SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_ADDSTRING, 0, (LPARAM)tempBlockStruct->name);
				if ((tempBlockStruct->flags & BSFLAG_ISSELECTED) && (selectedBlockId<0)) {
					selectedBlockId=i;
					if ((tempBlockStruct->typeOfBlock==BTYPE_BOXI)||(tempBlockStruct->typeOfBlock==BTYPE_VERI))
				    	CheckDlgButton(hwnd, IDC_BLOCKEXPORTINCLUDEHEADER, BST_CHECKED); //if it's a boxi or veri, include header
				}
				tempBlockStruct=tempBlockStruct->next;
			}

			if (selectedBlockId>=0) {
				BlockExportDetailsUpdate(hwnd, selectedBlockId);
				selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_SETCURSEL, selectedBlockId, 0);
			}

			return 1;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hwnd,1);
					return 1;
				case IDOK: //in this case this is the Export button

					//Get which block we want to export, and fill the selectedBlockStruct with that
					selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_GETCURSEL, 0, 0);
					if (selectedBlockId==CB_ERR) {MessageBox(hwnd, "No block has been selected. Please chose a block to export.", "Export",MB_ICONEXCLAMATION); return 1;}

					tempBlockStruct=pZim.first;
					for (i=0;i<max(pZim.wNumBlocks, selectedBlockId+1);i++) {
						if (i==selectedBlockId) {
							memcpy(&selectedBlockStruct, tempBlockStruct, sizeof(selectedBlockStruct));
						}
						tempBlockStruct=tempBlockStruct->next;
					}

					//Include header or not?
					includeHeader=IsDlgButtonChecked(hwnd, IDC_BLOCKEXPORTINCLUDEHEADER);


					memset(&ofnExportTo,0,sizeof(ofnExportTo));
					ofnExportTo.lStructSize = sizeof(ofnExportTo);
					ofnExportTo.hInstance = GetModuleHandle(NULL);
					ofnExportTo.hwndOwner = GetActiveWindow();
					ofnExportTo.lpstrFile = tempString;
					ofnExportTo.nMaxFile = sizeof(tempString);
					ofnExportTo.lpstrTitle = "Export To";
					ofnExportTo.nFilterIndex = 2;

					ofnExportTo.Flags = 0;
					if (includeHeader==BST_CHECKED) {
						ofnExportTo.lpstrFilter = "All files\0*.*\0Block file (*.blok)\0*.blok\0\0";
						ofnExportTo.lpstrDefExt = "blok";
						sprintf(tempString, "*.blok");
					}
					else {
						if ((selectedBlockStruct.typeOfBlock==BTYPE_ROOT)||(selectedBlockStruct.typeOfBlock==BTYPE_KERN)||(selectedBlockStruct.typeOfBlock==BTYPE_CODE)||(selectedBlockStruct.typeOfBlock==BTYPE_LOAD)||(selectedBlockStruct.typeOfBlock==BTYPE_NVRM)) {
							tempUsualStructure=selectedBlockStruct.ptrFurtherBlockDetail;
							if ((tempUsualStructure->magicnumber == SQUASHFS_MAGIC_LZMA)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC_LZMA_SWAP)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC_SWAP)) {
								ofnExportTo.lpstrFilter = "All files\0*.*\0Data file (*.data)\0*.data\0SquashFS file (*.sfs)\0*.sfs\0\0";
								ofnExportTo.nFilterIndex = 3;
								ofnExportTo.lpstrDefExt = "sfs";
								sprintf(tempString, "*.sfs");
							}
							else if ((tempUsualStructure->magicnumber&0xffff) == 0x8b1f){
								ofnExportTo.lpstrFilter = "All files\0*.*\0Data file (*.data)\0*.data\0Gzip file (*.gz)\0*.gz\0\0";
								ofnExportTo.nFilterIndex = 3;
								ofnExportTo.lpstrDefExt = "gz";
								sprintf(tempString, "*.gz");
							}
							else {
								ofnExportTo.lpstrFilter = "All files\0*.*\0Data file (*.data)\0*.data\0\0";
								ofnExportTo.lpstrDefExt = "data";
								sprintf(tempString, "*.data");
							}

						}
						else {
							ofnExportTo.lpstrFilter = "All files\0*.*\0Data file (*.data)\0*.data\0\0";
							ofnExportTo.lpstrDefExt = "data";
							sprintf(tempString, "*.data");
							}
					}

					if (GetSaveFileName(&ofnExportTo)) {

						filePtrExport = fopen(ofnExportTo.lpstrFile, "r");
						if (filePtrExport!=NULL) {
							if (MessageBox(hwnd, "This file alreadys exists. Are you sure you want to replace it?", "Export", MB_YESNO|MB_ICONEXCLAMATION)==IDNO)
								return 1;
							fclose(filePtrExport);
							}

						filePtrExport = fopen(ofnExportTo.lpstrFile, "wb");
						WriteBlockToFile(&pZim, &selectedBlockStruct, filePtrExport, (includeHeader==BST_CHECKED));
						fclose(filePtrExport);

						EndDialog(hwnd,1);
						return 1;
					}

					return 1;
				case IDC_BLOCKLIST:
					if (HIWORD(wParam)==CBN_SELCHANGE) {
						selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_GETCURSEL, 0, 0);
						BlockExportDetailsUpdate(hwnd, selectedBlockId);
					}

					return 1;
			}
			break;
	}
	return 0;
}

void BlockExportDetailsUpdate(HWND hwnd, int blockid)
{
	BLOCK_STRUCTURE selectedBlockStruct;
	BLOCK_STRUCTURE *tempBlockStruct;
	int i;
	char tempString[511]; //needs to accommodate a longish info string about each block

	USUAL_STRUCTURE *tempUsualStructure;


	tempBlockStruct=pZim.first;
	for (i=0;i<max(pZim.wNumBlocks, blockid+1);i++) {
		if (i==blockid) { //we need to copy the selectedblock to more than just a pointer, because we forget pointer
			memcpy(&selectedBlockStruct, tempBlockStruct, sizeof(selectedBlockStruct));
		}
		tempBlockStruct=tempBlockStruct->next;
	}
	SetDlgItemText(hwnd, IDC_BLOCKEXPORTTYPE, selectedBlockStruct.name);
	sprintf(tempString, "%i bytes", selectedBlockStruct.dwDataLength);
	SetDlgItemText(hwnd, IDC_BLOCKEXPORTSIZE,tempString);

	if (!(selectedBlockStruct.flags & BSFLAG_INMEMORY))	sprintf(tempString, "0x%x (%i)", selectedBlockStruct.dwBlockStartLoc, selectedBlockStruct.dwBlockStartLoc);
	if (selectedBlockStruct.flags & BSFLAG_INMEMORY)	sprintf(tempString, "Stored in memory");
	SetDlgItemText(hwnd, IDC_BLOCKEXPORTOFFSET,tempString);


	if (selectedBlockStruct.typeOfBlock==BTYPE_VERI)
		sprintf(tempString, "This verification block is used to check the validity of other blocks in the .zim file. In this case it contains verification data for %i other block%s. It is recommended to export with a header, although the block can be regenerated.",selectedBlockStruct.dwDataLength/32,(selectedBlockStruct.dwDataLength>32) ? "s":"");
	else if (selectedBlockStruct.typeOfBlock==BTYPE_BOXI)
		sprintf(tempString, "This block contains information about the set-top box, including the version and model numbers. It is recommended to export including the header to enable simple transfer to other .zim files.");
	else if ((selectedBlockStruct.typeOfBlock==BTYPE_ROOT)||(selectedBlockStruct.typeOfBlock==BTYPE_KERN)||(selectedBlockStruct.typeOfBlock==BTYPE_CODE)||(selectedBlockStruct.typeOfBlock==BTYPE_LOAD)||(selectedBlockStruct.typeOfBlock==BTYPE_NVRM)) {
		tempUsualStructure=selectedBlockStruct.ptrFurtherBlockDetail;
		if (tempUsualStructure) {
			sprintf(tempString,
			"These blocks generally contain a filesystem that is copied to the firmware. In this case it contains %s%s. It is written to %s of the firmware. To open it with other software you should export the block without a header.",
			((tempUsualStructure->magicnumber == SQUASHFS_MAGIC_LZMA)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC_LZMA_SWAP)||(tempUsualStructure->magicnumber == SQUASHFS_MAGIC_SWAP)) ? "a Squashfs filesystem":(((tempUsualStructure->magicnumber&0xffff) == 0x8b1f)? "a gzip file":"an unknown filesystem"),
			(tempUsualStructure->magicnumber == SQUASHFS_MAGIC_LZMA)?" compressed with the LZMA algorithm":((tempUsualStructure->magicnumber == SQUASHFS_MAGIC)?" compressed with the ZLIB algorithm":(((tempUsualStructure->magicnumber&0xffffff)==0x088b1f)?" compressed with the DEFLATE algorithm":" stored in an unknown algorithm")), //zlib or lzma
			(selectedBlockStruct.typeOfBlock==BTYPE_ROOT) ? "flash0.rootfs":((selectedBlockStruct.typeOfBlock==BTYPE_CODE)?"flash0.app":((selectedBlockStruct.typeOfBlock==BTYPE_KERN)?"flash0.kernel":"an unknown part")));
			}
		} //where this is written to
	else if (selectedBlockStruct.typeOfBlock==BTYPE_NVRM)
		sprintf(tempString, "The contents of this block are copied to the non-volatile RAM (flash0.nvram) of the set-top box. Probably best to export without header.");
	else if (selectedBlockStruct.typeOfBlock==BTYPE_LOAD)
		sprintf(tempString, "The contents of this block are copied to the Common Firmware Environment (flash0.cfe) of the set-top box.");
	else sprintf(tempString, "");

	SetDlgItemText(hwnd, IDC_BLOCKEXPORTDETAIL, tempString);

return;
}

void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	char filename[MAX_PATH];
	int i;

	switch(id) {
		case IDM_ABOUT:
			DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),	hwndMain, AboutDlg);
			break;
		case IDM_PROPERTIES:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_PROPERTIES), hwndMain, PropertiesDlg);
			break;
		case IDM_BLOCKEXPORT:
			if (pZim.displayFilename[0]) {
				DialogBox(hInst, MAKEINTRESOURCE(IDD_BLOCKEXPORT), hwndMain, BlockExportDlg);
			}
			break;
		case IDM_BLOCKIMPORT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_BLOCKIMPORT), hwndMain, BlockImportDlg);
			break;
		case IDM_CUSTOMERNUMBER:
			if (pZim.displayFilename[0]) {
				DialogBox(hInst, MAKEINTRESOURCE(IDD_CHANGECUSTOMERNUMBER), hwndMain, ChangeCustomerNumberDlg);
				InvalidateRect(hwndMain, NULL, TRUE);
			}
			break;
		case IDM_BLOCKFIXCHECKSUMS:
			if (pZim.displayFilename[0]) {
				CalculateOffsetForWriting(&pZim);
				InvalidateRect(hwndMain, NULL, TRUE);
			}
			break;
		case IDM_BLOCKCREATEBOXIBLOCK:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_BOXIBLOCK), hwndMain, BlockCreateBoxiDlg);
			break;
		case IDM_BLOCKCREATEVERIBLOCK:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_VERIBLOCK), hwndMain, BlockCreateVeriDlg);
			break;
		case IDM_OPEN:
		if (GetFilename(filename,sizeof(filename))) {
			if (pZim.displayFilename[0]) {
				if (MessageBox(hwnd, "You already have a file open. Are you sure you want to open a new one?", "ZimView", MB_OKCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2)==IDCANCEL)
					break;
				else {CloseZimFile(&pZim);	SetWindowText(hwnd, "ZimView");}
			}
		OpenZimFile(hwnd, &pZim, filename); //the opens the file, and loads the zim while checking for errors
		}
		break;
		case IDM_SAVEAS:
			SaveAsZim(&pZim);
			break;
		case IDM_REVERT:
			if (pZim.displayFilename[0]) {
				sprintf(&filename[0], "%s", pZim.displayFilename);
				CloseZimFile(&pZim);
				OpenZimFile(hwnd, &pZim, filename);
				InvalidateRect(hwnd, NULL, TRUE);
				UpdateWindow(hwnd);
			}
			break;
		case IDM_CLOSE:
			CloseZimFile(&pZim);
			SetWindowText(hwnd, "ZimView");
			InvalidateRect (hwnd, NULL, TRUE );
			break;
		case IDM_NEW:
			if (pZim.displayFilename[0]==0)	{
				ActivateZimFile(&pZim);
				InvalidateRect(hwnd, NULL, TRUE );
			} else	{
				if(MessageBox(hwnd, "Starting a new file will lose all changes on your open file. Do you want to start a new file?", "New", MB_YESNO|MB_ICONQUESTION)==IDYES)	{
					CloseZimFile(&pZim);
					ActivateZimFile(&pZim);
					InvalidateRect(hwnd, NULL, TRUE );
				}
			}
			break;
		case IDM_SELECTALL:
			SelectAllBlocks(hwnd, &pZim);
			break;
		case IDM_EDITCOPY:
			EditCopySelected(hwnd, &pZim, FALSE);
			break;
		case IDM_EDITCUT:
			EditCopySelected(hwnd, &pZim, TRUE);
			break;
		case IDM_EDITPASTE:
			EditPaste(hwnd, &pZim);
			InvalidateRect(hwnd, NULL, TRUE);
			break;
		case IDM_EDITCLEAR:
			DisableSelected(&pZim);
			RedrawSelectedBlocks(hwnd, &pZim);
			break;
		case IDM_EDITDELETE:
			i= SelectedCount(&pZim);
			if (i==0) return;
			sprintf(&filename[0], "Deleting %s block%s cannot be undone. Are you sure you want to delete %s?", i>1?"these":"this", i>1?"s":"", i>1?"them":"it");
			if (MessageBox(hwnd, filename, "Confirm block delete", MB_YESNO|MB_ICONQUESTION)==IDYES) {
				i=DeleteSelectedBlocks(&pZim); //returns the first block deleted -- so redraw this and all beneath
				if (caretedBlock>pZim.wNumBlocks-1) caretedBlock=pZim.wNumBlocks-1;
				if (i>=0) RedrawBetweenBlocks(hwnd, &pZim, i, pZim.wNumBlocks-1); //redraw from first delete block down
			}
			break;
		case IDM_MOVEUP:
			if (caretedBlock>0) {
				SwapBlocks(&pZim, caretedBlock, caretedBlock-1);
				RedrawBetweenBlocks(hwnd, &pZim, caretedBlock-1, caretedBlock);
				caretedBlock--;
			}
			break;
		case IDM_MOVEDOWN:
			if (caretedBlock<pZim.wNumBlocks-1) {
				SwapBlocks(&pZim, caretedBlock, caretedBlock+1);
				RedrawBetweenBlocks(hwnd, &pZim, caretedBlock, caretedBlock+1);
				caretedBlock++;
			}
			break;




		case IDM_EXIT:
		PostMessage(hwnd,WM_CLOSE,0,0);
		break;
	}
}


LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	POINT point;
	POINT pointClient;

	int oldSelectedBlock;
	int oldSelectedMoveBlock;

	RECT tempRect;

	HMENU contextMenu;

	switch (msg) {

	case WM_ERASEBKGND:
	    return 1;

	case WM_MOUSEACTIVATE:
    	SetFocus(hwnd);
	    return MA_ACTIVATE;
	case WM_SETFOCUS:
		isFocused=1;
		RedrawSelectedBlocks(hwnd, &pZim);
		return 0;
	case WM_KILLFOCUS:
		isFocused=0;
		showCaret=0;
		RedrawSelectedBlocks(hwnd, &pZim);
		if (caretedBlock>=0)
			RedrawBlock(hwnd, &pZim, caretedBlock);
		break;
	case WM_PAINT:
		PaintWindow(hwnd);
		break;
	case WM_SIZE:
		SendMessage(hWndStatusbar,msg,wParam,lParam);
		SendMessage(hwndToolBar,msg,wParam,lParam);
		InitializeStatusBar(hWndStatusbar,1);
		if (showCaret)	{
			if ((caretedBlock>=topDisplayBlock) && (caretedBlock<topDisplayBlock+numberDisplayedBlocks))	{
				tempRect.top=paintSelectRects[caretedBlock-topDisplayBlock].top;
				tempRect.bottom=tempRect.top+1;
				tempRect.left=paintSelectRects[caretedBlock-topDisplayBlock].left;
				tempRect.right=paintSelectRects[caretedBlock-topDisplayBlock].right;
				InvalidateRect(hwnd, &tempRect, FALSE);

				tempRect.bottom=paintSelectRects[caretedBlock-topDisplayBlock].bottom;
				tempRect.top=tempRect.bottom-1;
				InvalidateRect(hwnd, &tempRect, FALSE);

				tempRect.top=paintSelectRects[caretedBlock-topDisplayBlock].top;
				tempRect.left=paintSelectRects[caretedBlock-topDisplayBlock].left;
				tempRect.right=tempRect.left+1;
				InvalidateRect(hwnd, &tempRect, FALSE);

				tempRect.right=paintSelectRects[caretedBlock-topDisplayBlock].right;
				tempRect.left=tempRect.right-2;
				InvalidateRect(hwnd, &tempRect, FALSE);

				UpdateWindow(hwnd);
			}
		}
		break;
	case WM_INITMENUPOPUP:
	    InitMenu((HMENU) wParam);
	    break;
	case WM_MENUSELECT:
		return MsgMenuSelect(hwnd,msg,wParam,lParam);
	case WM_KEYDOWN:
		//Movement of the selected block
		if (showCaret==0) {
			showCaret=1;
			RedrawBlock(hwnd, &pZim, caretedBlock);
		}
		oldSelectedBlock=caretedBlock;
		if (wParam==VK_DOWN) caretedBlock++;
		if (wParam==VK_UP) caretedBlock--;
		if (wParam==VK_HOME) caretedBlock=0;
		if (wParam==VK_END) caretedBlock=pZim.wNumBlocks-1;

		if (wParam==VK_SPACE)	{
			if (GetKeyState(VK_CONTROL)<0)	{
				SelectToggleBlock(hwnd, &pZim, caretedBlock);
			}
			else if (GetKeyState(VK_SHIFT)<0)	{ //if shift is down, select a block
				SelectRangeBlocks(hwnd, &pZim, startRangeSelectedBlock, caretedBlock);
			}
			else {
				SelectOnlyBlock(hwnd, &pZim, caretedBlock);
			}
		}

		if (caretedBlock<0) caretedBlock=0;
		if (caretedBlock>=pZim.wNumBlocks) caretedBlock=pZim.wNumBlocks-1;

		if (caretedBlock<topDisplayBlock)
			OnMouseWheel(hwnd, &pZim, 1);

		if (caretedBlock>=topDisplayBlock+numberFullyDisplayedBlocks)	//we need another variable with FULLY displayed block
			OnMouseWheel(hwnd, &pZim, -1);


		//If the selected block has changed
		if (caretedBlock!=oldSelectedBlock) {
			if (GetKeyState(VK_CONTROL)<0)	{ //if control is down, we want to move caret but not change selection

			}
			else if (GetKeyState(VK_SHIFT)<0)	{ //if shift is down, select a block
				SelectRangeBlocks(hwnd, &pZim, startRangeSelectedBlock, caretedBlock);
			}
			else { //if control not down
				//now select the block which we have moved the caret to
				SelectOnlyBlock(hwnd, &pZim, caretedBlock);
				startRangeSelectedBlock=caretedBlock; 				//this should now be where we start selection from
			}
				RedrawBlock(hwnd, &pZim, caretedBlock);
				RedrawBlock(hwnd, &pZim, oldSelectedBlock);
		}
		break;

	case WM_LBUTTONUP:
    	if (draggingBlock) { //if we've been dragging
			ReleaseCapture();
			draggingBlock=0;
			mouseBeenPushed=0;
			MoveBlockAfter(&pZim, caretedBlock, moveIndicatorLocation);
			RedrawBetweenBlocks(hwnd, &pZim, caretedBlock, moveIndicatorLocation+1);
			RedrawMoveIndicator(hwnd, moveIndicatorLocation);

			LoadCursor(NULL, IDC_ARROW);
        }
		if (mouseBeenPushed) { //if the mouse was pushed, but not moved, we'll release capture
			mouseBeenPushed=0;
			ReleaseCapture();
		}
        break;

	case WM_CAPTURECHANGED:
        if (draggingBlock) {
			draggingBlock = 0;
			mouseBeenPushed=0;
           // clean up (e.g. free memory)
        }
        return(0);

	case WM_MOUSEMOVE:
		if ((mouseBeenPushed)&&(draggingBlock==0)) {
			draggingBlock=1;
			SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		}
		if (draggingBlock)	{
			oldSelectedMoveBlock=moveIndicatorLocation;
			MouseBlockMoveSelect(&moveIndicatorLocation, hwnd, &pZim, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

			if (!(oldSelectedMoveBlock==moveIndicatorLocation))	{
				RedrawMoveIndicator(hwnd, moveIndicatorLocation);
				RedrawMoveIndicator(hwnd, oldSelectedMoveBlock);
			}

		}
		break;


	case WM_LBUTTONDOWN:
		mouseBeenPushed=1;
		SetCapture(hwnd);

		oldSelectedBlock=caretedBlock;
		caretedBlock=-2;
		MouseBlockSelect(&caretedBlock, hwnd, &pZim, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (caretedBlock==-2) {//there was no block associated with the click
			ReleaseCapture();
			mouseBeenPushed=0;
			caretedBlock=oldSelectedBlock;
		}

		moveIndicatorLocation=caretedBlock; //so we don't want to move it to the new position

		if (GetKeyState(VK_CONTROL)<0)	{ //if control is down, we want to toggle that position
			SelectToggleBlock(hwnd, &pZim, caretedBlock);
			startRangeSelectedBlock=caretedBlock;
		}
		else if (GetKeyState(VK_SHIFT)<0)	{ //if shift is down, select a block
			SelectRangeBlocks(hwnd, &pZim, startRangeSelectedBlock, caretedBlock);
		}
		else { //if control and shift not down
			SelectOnlyBlock(hwnd, &pZim, caretedBlock);
			startRangeSelectedBlock=caretedBlock;
		}

		RedrawBlock(hwnd, &pZim, caretedBlock);
		RedrawBlock(hwnd, &pZim, oldSelectedBlock);
		break;
	case WM_CONTEXTMENU:
		point.x=GET_X_LPARAM(lParam);
		point.y=GET_Y_LPARAM(lParam);

		if ((point.x==-1) && (point.y==-1)) { //the use clicked the menu key instead
			if (caretedBlock>=topDisplayBlock)	{
				point.x=(paintSelectRects[caretedBlock-topDisplayBlock].left+paintSelectRects[caretedBlock-topDisplayBlock].right)/2;
				point.y=(paintSelectRects[caretedBlock-topDisplayBlock].top+paintSelectRects[caretedBlock-topDisplayBlock].bottom)/2;
			} else	{
				point.x=(paintSelectRects[0].left+paintSelectRects[0].right)/2;
				point.y=(paintSelectRects[0].top+paintSelectRects[0].bottom)/2;
			}
			ClientToScreen(hwnd, &point);
		}
		else { //it was a right mouse click
			pointClient.x=point.x; pointClient.y=point.y;
			ScreenToClient(hwnd, &pointClient);

			oldSelectedBlock=caretedBlock;
			caretedBlock=-2;
			MouseBlockSelect(&caretedBlock, hwnd, &pZim, pointClient.x, pointClient.y);

			if (GetKeyState(VK_CONTROL)<0)	{ //if control is down, we don't want to do anything
			}
			else if (GetKeyState(VK_SHIFT)<0)	{ //if shift is down, don't do anything either
			}
			else { //if control and shift are not pressed, we select the block before loading contextmenu
				if (caretedBlock==-2)	{
					caretedBlock=oldSelectedBlock;
					SelectNoBlocks(hwnd, &pZim);
				}
				else	{
					SelectOnlyBlock(hwnd, &pZim, caretedBlock);
					startRangeSelectedBlock=caretedBlock;
				}
			}

		}

		contextMenu=CreatePopupMenu();
		PopulatePopupMenu(contextMenu); //my function to add copy, paste, etc
		TrackPopupMenuEx(contextMenu, TPM_LEFTALIGN| TPM_TOPALIGN, point.x, point.y, hwnd, NULL);
		DestroyMenu(contextMenu);
		break;
	case WM_MOUSEWHEEL:
		return OnMouseWheel(hwnd, &pZim, (short)HIWORD(wParam));
	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
/*@@3<-@@*/
	return 0;
}


#define NUM_TOOLBAR_BUTTONS		11
HWND CreateAToolBar(HWND hwndParent)
{
	HWND hwndTB;
	TBADDBITMAP tbab;
	TBBUTTON tbb[NUM_TOOLBAR_BUTTONS];

	COLORMAP colorMap;
	HBITMAP hbm;

	int standardToolbarIndex;
	int myToolbarIndex[2];

	// Ensure that the common control DLL is loaded.
	//InitCommonControls(); (we load the Ex version at initiation)

	// Create a toolbar that the user can customize and that has a
	// tooltip associated with it.
	hwndTB = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR) NULL,
	    WS_CHILD|WS_BORDER|CCS_ADJUSTABLE,
	    0, 0, 0, 0, hwndParent, (HMENU) ID_TOOLBAR, hInst, NULL);

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for
	// backward compatibility.
	SendMessage(hwndTB, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);

	// Add the bitmap containing button images to the toolbar.
	tbab.hInst = HINST_COMMCTRL;
	tbab.nID   = IDB_STD_SMALL_COLOR;
	standardToolbarIndex = SendMessage(hwndTB, TB_ADDBITMAP, 0,(LPARAM)&tbab);

	//try a custom button
	colorMap.from = RGB(255, 0, 255);
	colorMap.to = GetSysColor(COLOR_BTNFACE);
	hbm = CreateMappedBitmap(hInst, BMP_TOOLBAREXPORT, 0, &colorMap, 1);
	tbab.hInst = NULL;
	tbab.nID = (UINT_PTR)hbm;
	myToolbarIndex[0] = SendMessage(hwndTB, TB_ADDBITMAP, 1, (LPARAM)&tbab);

	hbm = CreateMappedBitmap(hInst, BMP_TOOLBARIMPORT, 0, &colorMap, 1);
	tbab.hInst = NULL;
	tbab.nID = (UINT_PTR)hbm;
	myToolbarIndex[1] = SendMessage(hwndTB, TB_ADDBITMAP, 1, (LPARAM)&tbab);



	// clean memory before using it
	memset(tbb,0,sizeof tbb);

	tbb[0].iBitmap = STD_FILENEW+standardToolbarIndex;
	tbb[0].idCommand = IDM_NEW;
	tbb[0].fsState = 0;
	tbb[0].fsStyle = TBSTYLE_BUTTON;

	tbb[1].iBitmap = STD_FILEOPEN+standardToolbarIndex;
	tbb[1].idCommand = IDM_OPEN;
	tbb[1].fsState = TBSTATE_ENABLED;
	tbb[1].fsStyle = TBSTYLE_BUTTON;

	tbb[2].iBitmap = STD_FILESAVE+standardToolbarIndex;
	tbb[2].idCommand = IDM_SAVE;
	tbb[2].fsState = 0;
	tbb[2].fsStyle = TBSTYLE_BUTTON;

	tbb[3].iBitmap = 0;
	tbb[3].idCommand = 0;
	tbb[3].fsState = 0;
	tbb[3].fsStyle = BTNS_SEP;

	tbb[4].iBitmap = STD_CUT+standardToolbarIndex;
	tbb[4].idCommand = IDM_EDITCUT;
	tbb[4].fsState = 0;
	tbb[4].fsStyle = TBSTYLE_BUTTON;

	tbb[5].iBitmap = STD_COPY+standardToolbarIndex;
	tbb[5].idCommand = IDM_EDITCOPY;
	tbb[5].fsState = 0;
	tbb[5].fsStyle = TBSTYLE_BUTTON;

	tbb[6].iBitmap = STD_PASTE+standardToolbarIndex;
	tbb[6].idCommand = IDM_EDITPASTE;
	tbb[6].fsState = 0;
	tbb[6].fsStyle = TBSTYLE_BUTTON;

	tbb[7].iBitmap = 0;
	tbb[7].idCommand = 0;
	tbb[7].fsState = 0;
	tbb[7].fsStyle = BTNS_SEP;

	tbb[9].iBitmap = myToolbarIndex[0];
	tbb[9].idCommand = IDM_BLOCKEXPORT;
	tbb[9].fsState = TBSTATE_ENABLED;
	tbb[9].fsStyle = TBSTYLE_BUTTON;

	tbb[8].iBitmap = myToolbarIndex[1];
	tbb[8].idCommand = IDM_BLOCKIMPORT;
	tbb[8].fsState = TBSTATE_ENABLED;
	tbb[8].fsStyle = TBSTYLE_BUTTON;

	tbb[10].iBitmap = STD_PROPERTIES+standardToolbarIndex;
	tbb[10].idCommand = IDM_PROPERTIES;
	tbb[10].fsState = TBSTATE_ENABLED;
	tbb[10].fsStyle = TBSTYLE_BUTTON;

	SendMessage(hwndTB, TB_ADDBUTTONS, NUM_TOOLBAR_BUTTONS, (LPARAM)&tbb);


	ShowWindow(hwndTB, SW_SHOW);
	return hwndTB;
}

/*<---------------------------------------------------------------------->*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;

	hInst = hInstance;
	if (!InitApplication())
		return 0;
	hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));

	hwndMain = CreateWindow("ZimViewWndClass","ZimView",
		WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME|WS_VSCROLL,
		CW_USEDEFAULT,0,CW_USEDEFAULT,0, NULL, NULL, hInst, NULL);

	CreateWindow("ZimViewWndClass","ZimView1", WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_BORDER,
		CW_USEDEFAULT,0,CW_USEDEFAULT,0, hwndMain, NULL, hInst, NULL);

	if (hwndMain == (HWND)0)
		return 0;

	CreateStatusBar(hwndMain,"Ready",1);
	hwndToolBar = CreateAToolBar(hwndMain);
	ShowWindow(hwndMain,SW_SHOW);

	if (strlen(lpCmdLine)>4) 		OpenZimFile(hwndMain, &pZim, lpCmdLine);

	while (GetMessage(&msg,NULL,0,0)) {
		if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

// 4-byte number
DWORD DWORD_swap_endian(DWORD i)
{
    return((i&0xff)<<24)+((i&0xff00)<<8)+((i&0xff0000)>>8)+((i>>24)&0xff);
}

// 2-byte number
WORD WORD_swap_endian(WORD i)
{
    return ((i>>8)&0xff)+((i << 8)&0xff00);
}

int LoadZimFile(ZIM_STRUCTURE * LoadedZim) {
	WORD tempWord;

	struct sZimHeader zimHeader; //load the zim header as is
	struct sBlockHeader blockHeader; //load the zim header as is

	BLOCK_STRUCTURE * tempBlockStruct;
	BLOCK_STRUCTURE * oldBlockStruct;

	ADLER_STRUCTURE adlerhold; //memset to zero to start, then can be used for multipart checksums

	struct cvs_MD5Context MD5context;

	DWORD blockidDword;

			//Find the real file length, and rewind file
			fseek(LoadedZim->pZimFile, 0, SEEK_END);
			LoadedZim->dwRealFileLen=ftell(LoadedZim->pZimFile);
			rewind(LoadedZim->pZimFile);

			//If the file is too small for even a header, don't pursue it
			if (LoadedZim->dwRealFileLen<sizeof(zimHeader))
				return LOADZIM_ERR_SIZEIMPOSSIBLE;

			memset(&adlerhold, 0, sizeof(ADLER_STRUCTURE)); //set a=0, b=0 (broken adler)
			LoadedZim->dwRealChecksum=	AdlerOnFile(LoadedZim->pZimFile, &adlerhold, 4, LoadedZim->dwRealFileLen-4);
			if ((adlerhold.a==0xDE) && (adlerhold.b==0xAD) && (LoadedZim->dwRealChecksum==0)) return LOADZIM_ERR_MEMORYPROBLEM;


			rewind(LoadedZim->pZimFile);

			//Load the header (as is) into zimHeader
			fread(&zimHeader, sizeof(zimHeader), 1, LoadedZim->pZimFile);

			//Transfer data from zimHeader to LoadedZim while fixing endianess
			LoadedZim->wNumBlocks=WORD_swap_endian(zimHeader.wNumBlocks);
			LoadedZim->dwTotalFileLen=DWORD_swap_endian(zimHeader.dwTotalFileLen);
			LoadedZim->dwChecksum=DWORD_swap_endian(zimHeader.dwChecksum); //?endianess
			LoadedZim->wCustomerNumber=zimHeader.wCustomerNumber; //?big endian

			//If the number of blocks is less than zero, then something is wrong
			if (LoadedZim->wNumBlocks<0) {
				return LOADZIM_ERR_BLOCKNUMBERIMPOSSIBLE;
			}

			//If there are too many blocks for the number of block locations (DWORDs), then return
			if ((sizeof(zimHeader) + sizeof(DWORD) * LoadedZim->wNumBlocks)>LoadedZim->dwRealFileLen)
				return LOADZIM_ERR_BLOCKNUMBERIMPOSSIBLE;

			LoadedZim->dwBlockStartLocArray=malloc(LoadedZim->wNumBlocks * sizeof(DWORD));
			if (LoadedZim->dwBlockStartLocArray==NULL)
				return LOADZIM_ERR_MEMORYPROBLEM;
			memset(LoadedZim->dwBlockStartLocArray, 0, LoadedZim->wNumBlocks * sizeof(DWORD));

			LoadedZim->first=NULL;

			//Read where the blocks are located, and load them into internal structure, check for obvious errors
			for (tempWord=0;tempWord<LoadedZim->wNumBlocks; tempWord++) {
				fread(&LoadedZim->dwBlockStartLocArray[tempWord],sizeof(DWORD),1,LoadedZim->pZimFile);
				//Convert to big endian
				LoadedZim->dwBlockStartLocArray[tempWord]=DWORD_swap_endian(LoadedZim->dwBlockStartLocArray[tempWord]);

				//if the block is located before the header, it's not going to be a block (probably) OR if the block is meant to be at a position greater than the real file size, then stop
				if ( (LoadedZim->dwBlockStartLocArray[tempWord]<=sizeof(zimHeader)) || (LoadedZim->dwBlockStartLocArray[tempWord]> LoadedZim->dwRealFileLen-sizeof(blockHeader)) ) {
					free(LoadedZim->dwBlockStartLocArray); //free the memory allocated for the array
					LoadedZim->wNumBlocks=0;
					return LOADZIM_ERR_BLOCKOUTOFBOUNDS; }

			}

			oldBlockStruct=LoadedZim->first;
			//Go through each block, and add it to the linked list
			for (tempWord=0;tempWord<LoadedZim->wNumBlocks; tempWord++) {
				//Read the block header
				fseek(LoadedZim->pZimFile, LoadedZim->dwBlockStartLocArray[tempWord], SEEK_SET);
				fread(&blockHeader, sizeof(blockHeader), 1, LoadedZim->pZimFile);

				/* NOTE IN BOTH THESE ERRORS, I NEED TO free the previously malloc'd structures */
				if (DWORD_swap_endian(blockHeader.dwDataLength)<sizeof(blockHeader)) { //if the length of the block too small
					LoadedZim->wNumBlocks=0;
					return LOADZIM_ERR_BLOCKSIZEIMPOSSIBLE;
				}

				if (DWORD_swap_endian(blockHeader.dwDataLength)+LoadedZim->dwBlockStartLocArray[tempWord]>LoadedZim->dwRealFileLen) { //or if bigger than remaining space in file
					LoadedZim->wNumBlocks=0;
					return LOADZIM_ERR_BLOCKSIZEIMPOSSIBLE;
				}

				tempBlockStruct=malloc(sizeof(BLOCK_STRUCTURE));
				memset(tempBlockStruct, 0, sizeof(BLOCK_STRUCTURE));

				if (tempWord==0) LoadedZim->first=tempBlockStruct;
				else oldBlockStruct->next=tempBlockStruct;

				tempBlockStruct->next=NULL;

				tempBlockStruct->dwDataLength =		DWORD_swap_endian(blockHeader.dwDataLength);
				tempBlockStruct->dwBlockStartLoc =	LoadedZim->dwBlockStartLocArray[tempWord];
				tempBlockStruct->dwDestStartLoc =	tempBlockStruct->dwBlockStartLoc;
				tempBlockStruct->dwChecksum=DWORD_swap_endian(blockHeader.dwChecksum);
				tempBlockStruct->name[0]=blockHeader.name[0];
				tempBlockStruct->name[1]=blockHeader.name[1];
				tempBlockStruct->name[2]=blockHeader.name[2];
				tempBlockStruct->name[3]=blockHeader.name[3];
				tempBlockStruct->name[4]=0;

				tempBlockStruct->blockSignature[0]=blockHeader.blockSignature[0];
				tempBlockStruct->blockSignature[1]=blockHeader.blockSignature[1];
				tempBlockStruct->blockSignature[2]=blockHeader.blockSignature[2];
				tempBlockStruct->blockSignature[3]=blockHeader.blockSignature[3];

				//Calculate the real checksum on the block in two parts
				memset(&adlerhold, 0, sizeof(ADLER_STRUCTURE));
				AdlerOnFile(LoadedZim->pZimFile, &adlerhold, tempBlockStruct->dwBlockStartLoc+sizeof(blockHeader), tempBlockStruct->dwDataLength);
				if ((adlerhold.a==0xDE) && (adlerhold.b==0xAD) && (1==0)) return LOADZIM_ERR_MEMORYPROBLEM;
				tempBlockStruct->dwRealChecksum=AdlerOnFile(LoadedZim->pZimFile, &adlerhold, tempBlockStruct->dwBlockStartLoc, sizeof(blockHeader)-sizeof(blockHeader.dwChecksum));
				if ((adlerhold.a==0xDE) && (adlerhold.b==0xAD) && (tempBlockStruct->dwRealChecksum==0)) return LOADZIM_ERR_MEMORYPROBLEM;

				//Calculate the md5 of the block
				cvs_MD5Init(&MD5context);
				MD5OnFile(LoadedZim->pZimFile, &MD5context, tempBlockStruct->dwBlockStartLoc+sizeof(blockHeader), tempBlockStruct->dwDataLength);
				cvs_MD5Final (&tempBlockStruct->md5, &MD5context);

				//get a DWORD from the block name
				blockidDword=0;
				blockidDword |= (tempBlockStruct->name[3] << 24);
				blockidDword |= (tempBlockStruct->name[2] << 16);
				blockidDword |= (tempBlockStruct->name[1] << 8);
				blockidDword |= tempBlockStruct->name[0];

				switch (blockidDword) {

				//First do what is common to root, code and kern, load, nvrm
				case BID_DWORD_ROOT:
				case BID_DWORD_CODE:
				case BID_DWORD_KERN:
				case BID_DWORD_LOAD:
				case BID_DWORD_NVRM:
					if (blockidDword==BID_DWORD_ROOT) tempBlockStruct->typeOfBlock=BTYPE_ROOT;
					if (blockidDword==BID_DWORD_CODE) tempBlockStruct->typeOfBlock=BTYPE_CODE;
					if (blockidDword==BID_DWORD_KERN) tempBlockStruct->typeOfBlock=BTYPE_KERN;
					if (blockidDword==BID_DWORD_LOAD) tempBlockStruct->typeOfBlock=BTYPE_LOAD;
					if (blockidDword==BID_DWORD_NVRM) tempBlockStruct->typeOfBlock=BTYPE_NVRM;
					tempBlockStruct->ptrFurtherBlockDetail=ReadUsualBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwBlockStartLoc + sizeof(blockHeader));
					break;
				case BID_DWORD_VERI:
					tempBlockStruct->typeOfBlock=BTYPE_VERI;
					tempBlockStruct->ptrFurtherBlockDetail=ReadVeriBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwBlockStartLoc + sizeof(blockHeader));
					break;
				case BID_DWORD_BOXI:
					tempBlockStruct->typeOfBlock=BTYPE_BOXI;
					tempBlockStruct->ptrFurtherBlockDetail=ReadBoxiBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwBlockStartLoc + sizeof(blockHeader));
					break;
				default: //?UVER ?MCUP
					tempBlockStruct->typeOfBlock=0;
					break;
				}
				oldBlockStruct=tempBlockStruct;
			}

			free(LoadedZim->dwBlockStartLocArray); //Free the memory the block start points were stored.

	if (LoadedZim->wNumBlocks > ((LoadedZim->dwRealFileLen-12)/24))
		return LOADZIM_ERR_BLOCKNUMBERUNLIKELY;

	if (LoadedZim->dwRealFileLen!=LoadedZim->dwTotalFileLen)
		return LOADZIM_ERR_SIZEDESCREP;

return LOADZIM_ERR_SUCCESS; //default
}


int CloseZimFile(ZIM_STRUCTURE *LoadedZim) {
	int i;
	WORD counterVeriPart;
	DWORD *arrayOfStructPointers;
	BLOCK_STRUCTURE *ptrBlockStruct;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;

	if (pZim.displayFilename[0]==0) {memset(LoadedZim, 0, sizeof(ZIM_STRUCTURE)); return 1;} //this should ONLY occur if the zim file has no malloc'd children

	topDisplayBlock=0;

	//We need to go backwards through linked list, freeing all the blockstructs e.g LoadedZim->first
	//Since we're deleting, we can just enumerate all the structs, then delete

	arrayOfStructPointers=NULL;
	//if (LoadedZim->wNumBlocks==0) {memset(LoadedZim, 0, sizeof(ZIM_STRUCTURE)); return 1;}
	if (LoadedZim->wNumBlocks>0)	{
		arrayOfStructPointers=malloc(LoadedZim->wNumBlocks*sizeof(DWORD));
		memset(arrayOfStructPointers, 0, LoadedZim->wNumBlocks*sizeof(DWORD));

		ptrBlockStruct=(LoadedZim->first);
		arrayOfStructPointers[0]=(DWORD)ptrBlockStruct;
	}

	//Load the block pointers in an array, at same time free memory assigned to the furtherblockdetail structs.
	for (i=1; i<LoadedZim->wNumBlocks; i++) {
		arrayOfStructPointers[i]=(DWORD)ptrBlockStruct->next;
		ptrBlockStruct=(ptrBlockStruct->next);
	}

	for (i=0; i<LoadedZim->wNumBlocks; i++) {
		ptrBlockStruct=(BLOCK_STRUCTURE*)arrayOfStructPointers[i];

		switch (ptrBlockStruct->typeOfBlock) {
		case BTYPE_VERI: /* NEED TO GET RID OF VERI LINKED LISTS */
			tempVeriStruct=ptrBlockStruct->ptrFurtherBlockDetail;
			counterVeriPart=tempVeriStruct->numberVeriObjects;
			while (counterVeriPart) {
				while (tempVeriStruct->nextStructure) {
						tempVeriStruct=tempVeriStruct->nextStructure;
				}
				if (tempVeriStruct->prevStructure) {tempVeriStruct->prevStructure->nextStructure=NULL;}
				free(tempVeriStruct);
				counterVeriPart--;
				if (counterVeriPart) tempVeriStruct=ptrBlockStruct->ptrFurtherBlockDetail;
			}
			break;
		case BID_DWORD_ROOT:
		case BID_DWORD_CODE:
		case BID_DWORD_KERN:
		case BID_DWORD_LOAD:
		case BID_DWORD_NVRM:
			tempUsualStruct = ptrBlockStruct->ptrFurtherBlockDetail;
			if (tempUsualStruct->sqshHeader) {free(tempUsualStruct->sqshHeader);}
			if (tempUsualStruct->gzipHeader) {free(tempUsualStruct->gzipHeader);}
			free(ptrBlockStruct->ptrFurtherBlockDetail); //free the further boxi block detail
			break;
		case BID_DWORD_BOXI:
		default:
			free(ptrBlockStruct->ptrFurtherBlockDetail); //free the further block detail
			break;
	}
		free(ptrBlockStruct); //free the block itself
	}

	if (arrayOfStructPointers)
		free(arrayOfStructPointers); //free the array that held all of those pointers

	if (LoadedZim->pZimFile)
		fclose(LoadedZim->pZimFile); //Close the associated file.

    memset(LoadedZim, 0, sizeof(ZIM_STRUCTURE));
	caretedBlock=0;

	return 0;
}


int PaintWindow(HWND hwnd) {

	HDC hdc;
	PAINTSTRUCT psPaint;
	RECT clientRect;
	HRGN clipRgn;

	RECT headerRect; //the rectangle the zim header information is displayed in
	RECT footerRect; //the white space under all the blocks
	RECT lineRect;   //a rect for each line when using exttextout
	int heightHeader;
	int heightBlock;

	int intIconResource;
	HICON hIcon;

	LOGBRUSH logBrush;
	HPEN hPenCaret;
	HBRUSH hBrush;
	COLORREF backgroundColour;

	int y;
	int i;

	char tempString[255];
	WORD counterVeriPart;

	BLOCK_STRUCTURE *tempBlockStruct;
	BOXI_STRUCTURE *tempBoxiStruct;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;

	int flaglzma;
	int flagendian;

	hdc = BeginPaint(hwnd, &psPaint);
	GetClientRect(hwnd, &clientRect);			//the size of the whole window draw area


	//First of all we need to calculate the space the header and each block will occupy
	heightHeader=180; //includes all padding from the top of the client rect

	headerRect.left=clientRect.left;
	headerRect.right=clientRect.right;
	headerRect.top=clientRect.top;
	headerRect.bottom=headerRect.top+heightHeader;

	y=headerRect.bottom;
	tempBlockStruct=GetBlockNumber(&pZim, topDisplayBlock); //pZim.first;

	numberFullyDisplayedBlocks=0;
	for (i=0;(i<pZim.wNumBlocks-topDisplayBlock)&&(i<255);i++) {
		paintSelectRects[i].top=y;
		paintSelectRects[i].left=clientRect.left;
		paintSelectRects[i].right=clientRect.right;
		y+=68; //each block can be 64, plus two 2px margins for now
		paintSelectRects[i].bottom=y;

		if (y<clientRect.bottom) numberFullyDisplayedBlocks++;
		tempBlockStruct=tempBlockStruct->next;
	}

	//Set the footer rect
	footerRect.top=y;
	footerRect.bottom=clientRect.bottom;
	footerRect.left=clientRect.left;
	footerRect.right=clientRect.right;


	//Displayed regardless
	y=64;
	if (!pZim.displayFilename[0])
		ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &clientRect, "Open a .zim file above, or start adding blocks.", 47, NULL);

	if (pZim.displayFilename[0]) {

		//Display the header information
		if (RectVisible(hdc, &headerRect)) {
			//draw line by line, the left and right edges don't change
			lineRect.left=headerRect.left;
			lineRect.right=headerRect.right;

			y+=16;
			sprintf(tempString, "%s", pZim.displayFilenameNoPath);

			lineRect.top=headerRect.top; //this is the first line of text, so should include top margin
			lineRect.bottom=y+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;
			sprintf(tempString, "%s", pZim.displayFilename);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;
			sprintf(tempString, "Checksum: %08X (correct)", pZim.dwChecksum);
			if (pZim.dwChecksum!=pZim.dwRealChecksum)
				sprintf(tempString, "Checksum: %08X (differs from calculated checksum %08X)", pZim.dwChecksum, pZim.dwRealChecksum);

			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;
			sprintf(tempString, "Customer number: %i", pZim.wCustomerNumber);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;
			sprintf(tempString, "Number of blocks: %i", pZim.wNumBlocks);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;
			sprintf(tempString, "File length: %i bytes", pZim.dwTotalFileLen);
			if (pZim.dwTotalFileLen!=pZim.dwRealFileLen)
	 			sprintf(tempString, "File length: %i bytes (NB: Actual file length is %i bytes)", pZim.dwTotalFileLen, pZim.dwRealFileLen);
			lineRect.top=y;
			lineRect.bottom=headerRect.bottom; //this is the last line, so should include the small margin between header and blocks
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);
			}

//List the blocks
			tempBlockStruct=GetBlockNumber(&pZim, topDisplayBlock); //pZim.first;
			numberDisplayedBlocks=0;
			for (i=0;(i<pZim.wNumBlocks-topDisplayBlock)&&(paintSelectRects[i].top<clientRect.bottom);i++) {
				numberDisplayedBlocks++;
				if (RectVisible(hdc, &paintSelectRects[i])) {
					y=paintSelectRects[i].top+2;
					//Decide the background colour to draw (i.e. different if selected)
					if (tempBlockStruct->flags & BSFLAG_ISSELECTED) {
						if (isFocused)
							backgroundColour= RGB(255, 255, 128);
						else
							backgroundColour= RGB(220, 220, 160);
					}
					else {
						backgroundColour=RGB(255, 255, 255);
					}
					SetBkColor(hdc, backgroundColour);

					//Decide the text colour to draw
					if (tempBlockStruct->flags & BSFLAG_DONTWRITE)
							SetTextColor(hdc, RGB(128, 128, 128));
					else if (tempBlockStruct->flags & BSFLAG_INMEMORY)
						{
							SetTextColor(hdc, RGB(0, 0, 128));
						}
					else
						SetTextColor(hdc, RGB(0, 0, 0));


					//Exclude from overwriting the icon.
					ExcludeClipRect(hdc, paintSelectRects[i].left+EDGE_MARGIN, paintSelectRects[i].top+1, paintSelectRects[i].left+EDGE_MARGIN+ICON_SIZE, paintSelectRects[i].top+ICON_SIZE);

					//Don't overwrite where the caret is, otherwise we get flicker
					if ((showCaret) && (i==(caretedBlock-topDisplayBlock)))	{
						paintSelectRects[i].top++;
						paintSelectRects[i].left++;
						paintSelectRects[i].bottom--;
						paintSelectRects[i].right--;
					}

					lineRect.top=paintSelectRects[i].top;
					lineRect.bottom=paintSelectRects[i].bottom;
					lineRect.left=paintSelectRects[i].left;
					lineRect.right=paintSelectRects[i].left + EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE;
					ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &lineRect, "", 0, NULL); //blank the area behind the icon with bg colour

					//Display for every block
					lineRect.left=paintSelectRects[i].left;
					lineRect.right=paintSelectRects[i].right;

					lineRect.top=paintSelectRects[i].top; //this is first line, so it starts where the block does
					lineRect.bottom=y+16;

					sprintf(tempString, "%s",tempBlockStruct->name);
					ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

					y+=16;
					sprintf(tempString, "Offset: 0x%x, Length: 0x%x, Checksum: %08X", tempBlockStruct->dwDestStartLoc, tempBlockStruct->dwDataLength, tempBlockStruct->dwRealChecksum);
					lineRect.top=y;
					lineRect.bottom=y+16;
					if (tempBlockStruct->ptrFurtherBlockDetail==NULL)
						lineRect.bottom=paintSelectRects[i].bottom; //as this is last line
					ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

					/*
					y+=16;
					sprintf(tempString, "md5: ");
					MD5HexString(tempString+5, tempBlockStruct->md5);
					TextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, tempString, strlen(tempString));
					*/

					//Before we decide which icon to use
					intIconResource=IDI_UNKNOWNBLOCK;

					//Block specifics
					switch (tempBlockStruct->typeOfBlock) {
						//root->flash0.rootfs, kern->kernel, code->?app, ?nvrm->nvram (non-volatile ram)
						case BTYPE_ROOT:
						case BTYPE_KERN:
						case BTYPE_CODE:
						case BTYPE_LOAD:
						case BTYPE_NVRM:
							//First get the icon resource decided
							intIconResource=IDI_NVRM;
							if (tempBlockStruct->typeOfBlock==BTYPE_ROOT)
								intIconResource=IDI_ROOT;
							if (tempBlockStruct->typeOfBlock==BTYPE_KERN)
								intIconResource=IDI_KERN;
							if (tempBlockStruct->typeOfBlock==BTYPE_CODE)
								intIconResource=IDI_CODE;
							if (tempBlockStruct->typeOfBlock==BTYPE_LOAD)
								intIconResource=IDI_LOAD;

							y+=16;
							tempUsualStruct=tempBlockStruct->ptrFurtherBlockDetail;

							if (tempUsualStruct) {
								sprintf(tempString, "Magic: %s (0x%08x)",tempUsualStruct->displaymagicnumber, tempUsualStruct->magicnumber);
								lineRect.top=y;
								lineRect.bottom=y+16;
								if (!((tempUsualStruct->gzipHeader) ||(tempUsualStruct->sqshHeader)))
									lineRect.bottom=paintSelectRects[i].bottom; //as this is last line
								ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

								if (tempUsualStruct->sqshHeader) {
									y+=16;
									if (tempUsualStruct->sqshHeader->s_magic==SQUASHFS_MAGIC_LZMA) {flaglzma=1; flagendian=1;}
									if (tempUsualStruct->sqshHeader->s_magic==SQUASHFS_MAGIC) {flaglzma=0; flagendian=1;}
									if (tempUsualStruct->sqshHeader->s_magic==SQUASHFS_MAGIC_LZMA_SWAP) {flaglzma=1; flagendian=0;}
									if (tempUsualStruct->sqshHeader->s_magic==SQUASHFS_MAGIC_SWAP) {flaglzma=0; flagendian=0;}
									sprintf(tempString, "SquashFS-%s. Ver: %d.%d. Inodes: %i.", flaglzma ? "lzma":"zlib", tempUsualStruct->sqshHeader->s_major, tempUsualStruct->sqshHeader->s_minor, tempUsualStruct->sqshHeader->inodes);
									lineRect.top=y;
									lineRect.bottom=paintSelectRects[i].bottom; //as this is last line
									ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);
								}
								if (tempUsualStruct->gzipHeader) {
									y+=16;
									sprintf(tempString, "GZIP. Flags: %i. Encoding OS: %i", tempUsualStruct->gzipHeader->flg, tempUsualStruct->gzipHeader->os);
									lineRect.top=y;
									lineRect.bottom=paintSelectRects[i].bottom; //as this is last line
									ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);
								}
							}
						break;
						case BTYPE_VERI:
							intIconResource=IDI_VERI;
							tempVeriStruct=tempBlockStruct->ptrFurtherBlockDetail;
							if (tempVeriStruct) {
								counterVeriPart=tempVeriStruct->numberVeriObjects;
								while (counterVeriPart) {
									y+=16;
									sprintf(tempString, "Id: %s, Ver: %i.%i.(%i).%i, md5: ",tempVeriStruct->displayblockname, tempVeriStruct->veriFileData.version_a,tempVeriStruct->veriFileData.version_b,tempVeriStruct->veriFileData.version_c,tempVeriStruct->veriFileData.version_d);
									MD5HexString(tempString+strlen(tempString), tempVeriStruct->veriFileData.md5Digest);
									lineRect.top=y;
									lineRect.bottom=y+16;
									if (tempVeriStruct->nextStructure==NULL)
										lineRect.bottom=paintSelectRects[i].bottom; //if this is last veri structure, draw to bottom of block
									ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

									counterVeriPart--;
									if (counterVeriPart) tempVeriStruct=tempVeriStruct->nextStructure;
								}
							}
							break;
						case BTYPE_BOXI:
							intIconResource=IDI_BOXI;
							tempBoxiStruct=tempBlockStruct->ptrFurtherBlockDetail;
							if (tempBoxiStruct) {
								y+=16;
								sprintf(tempString, "OUI: 0x%x, Ver/Mod: %i/%i/%i/%i, StarterSize: %i", tempBoxiStruct->boxiFileData.uiOUI&0xFFFF, tempBoxiStruct->boxiFileData.wHwVersion, tempBoxiStruct->boxiFileData.wSwVersion, tempBoxiStruct->boxiFileData.wHwModel, tempBoxiStruct->boxiFileData.wSwModel, tempBoxiStruct->boxiFileData.uiStarterImageSize);
								lineRect.top=y;
								lineRect.bottom=y+16;
								ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

								y+=16;
								MD5HexString(tempString, tempBoxiStruct->boxiFileData.abStarterMD5Digest);
								lineRect.top=y;
								lineRect.bottom=paintSelectRects[i].bottom; //as this is last line
								ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);
							}
							break;
						}

					//Draw Caret
					if ((showCaret) && (i==(caretedBlock-topDisplayBlock)))	{

						paintSelectRects[caretedBlock-topDisplayBlock].top--;
						paintSelectRects[caretedBlock-topDisplayBlock].left--;
						paintSelectRects[caretedBlock-topDisplayBlock].bottom++;
						paintSelectRects[caretedBlock-topDisplayBlock].right++;

						DrawCaret(hdc, &paintSelectRects[caretedBlock-topDisplayBlock], RGB(0,0,0), backgroundColour);
					}

					//load and display the icon
					clipRgn = CreateRectRgn(psPaint.rcPaint.left, psPaint.rcPaint.top, psPaint.rcPaint.right, psPaint.rcPaint.bottom);
					SelectClipRgn(hdc, clipRgn);

					hBrush=CreateSolidBrush(backgroundColour); //draw the icon first onto a back buffer of this brush
					hIcon=LoadImage(hInst, MAKEINTRESOURCE(intIconResource), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
					DrawIconEx(hdc, paintSelectRects[i].left+EDGE_MARGIN, paintSelectRects[i].top+1, hIcon, 0, 0, 0, hBrush, DI_NORMAL);
					DestroyIcon(hIcon);
					DeleteObject(hBrush);
					DeleteObject(clipRgn);
				}


				tempBlockStruct=tempBlockStruct->next;
			}

			//Draw the footer
			if (RectVisible(hdc, &footerRect))
				FillRect(hdc, &footerRect,(HBRUSH) (COLOR_WINDOW+1));

			//draw the moveindicator bar
			if (draggingBlock)	{
				logBrush.lbColor=RGB(255, 255, 0);
				logBrush.lbStyle=BS_SOLID;
				hPenCaret=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND, MOVEINDICATOR_WIDTH, &logBrush, 0, NULL);

				SelectObject(hdc, hPenCaret);
				if (moveIndicatorLocation>=topDisplayBlock)	{
					MoveToEx(hdc, paintSelectRects[moveIndicatorLocation-topDisplayBlock].left, paintSelectRects[moveIndicatorLocation-topDisplayBlock].bottom, NULL);
					LineTo(hdc, paintSelectRects[moveIndicatorLocation-topDisplayBlock].right, paintSelectRects[moveIndicatorLocation-topDisplayBlock].bottom);
				}
				else if (moveIndicatorLocation<topDisplayBlock) {
					MoveToEx(hdc, paintSelectRects[0].left, paintSelectRects[0].top, NULL);
					LineTo(hdc, paintSelectRects[0].right, paintSelectRects[0].top);
				}

				DeleteObject(hPenCaret);
			}
		}
		else {
			//Displayed if no file loaded
			caretedBlock=0;
			topDisplayBlock=0;
		}

		EndPaint (hwnd, &psPaint);
return 0;
}

int DrawCaret(HDC hdc, RECT *lpRect, COLORREF colour1, COLORREF colour2)
{
	LOGBRUSH logBrush;
	HPEN hPenCaret;
	HPEN hPenCaretBackground;

	logBrush.lbColor=colour1;
	logBrush.lbStyle=BS_SOLID;

	hPenCaret=ExtCreatePen(PS_COSMETIC|PS_ALTERNATE, 1,&logBrush,0,NULL);

	logBrush.lbColor=colour2;
	logBrush.lbStyle=BS_SOLID;
	hPenCaretBackground=ExtCreatePen(PS_COSMETIC|PS_ALTERNATE, 1,&logBrush,0,NULL);

	SelectObject(hdc, hPenCaret);
	SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)); //inside of the rectangle
	MoveToEx(hdc, lpRect->left, lpRect->top, NULL);
	LineTo(hdc,lpRect->right, lpRect->top);

	MoveToEx(hdc, lpRect->left, lpRect->top, NULL);
	LineTo(hdc,lpRect->left, lpRect->bottom);

	MoveToEx(hdc, lpRect->right-1, lpRect->top, NULL);
	LineTo(hdc,lpRect->right-1, lpRect->bottom);

	MoveToEx(hdc, lpRect->left, lpRect->bottom-1, NULL);
	LineTo(hdc,lpRect->right, lpRect->bottom-1);


	SelectObject(hdc, hPenCaretBackground);
	MoveToEx(hdc, lpRect->left+1, lpRect->top, NULL);
	LineTo(hdc,lpRect->right, lpRect->top);

	MoveToEx(hdc, lpRect->left, lpRect->top+1, NULL);
	LineTo(hdc,lpRect->left, lpRect->bottom);

	MoveToEx(hdc, lpRect->right-1, lpRect->top+1, NULL);
	LineTo(hdc,lpRect->right-1, lpRect->bottom);

	MoveToEx(hdc, lpRect->left+1, lpRect->bottom-1, NULL);
	LineTo(hdc,lpRect->right, lpRect->bottom-1);

	DeleteObject(hPenCaretBackground);
	DeleteObject(hPenCaret);

	return 0;
}


int MD5HexString(char * outputString, char * MD5array)
{
sprintf(outputString,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", MD5array[0]&0xFF,MD5array[1]&0xFF,MD5array[2]&0xFF,MD5array[3]&0xFF,MD5array[4]&0xFF,MD5array[5]&0xFF,MD5array[6]&0xFF,MD5array[7]&0xFF,MD5array[8]&0xFF,MD5array[9]&0xFF,MD5array[10]&0xFF,MD5array[11]&0xFF,MD5array[12]&0xFF,MD5array[13]&0xFF,MD5array[14]&0xFF,MD5array[15]&0xFF);
return 0;
}

int MD5StringToArray(char * MD5array, char * inputString)
{
	char hexnumber[3];
	int i;

	hexnumber[2]=0;
	i=0;
	while (inputString[i] && i<32) {
		hexnumber[0]=inputString[i];
		hexnumber[1]=inputString[i+1];

		MD5array[i/2]=strtoul(&hexnumber[0], NULL, 16);
		i+=2;
	}
return 0;

}

int ActivateZimFile(ZIM_STRUCTURE *ZimToActivate)
{

	sprintf(&ZimToActivate->displayFilename[0], "Untitled");
	ZimToActivate->displayFilenameNoPath=&ZimToActivate->displayFilename[0];
	ZimToActivate->pZimFile=NULL;

	ZimToActivate->wNumBlocks=0;
	ZimToActivate->first=NULL;

	SetWindowText(hwndMain, "ZimView - Untitled");

	return 0;

}

int OpenZimFile(HWND hwnd, ZIM_STRUCTURE *ZimToOpen, char * filename)
{
int i;
char tempString[255];

if (filename[0]=='\"') {
	filename++;
	i=strlen(filename);
	filename[i-1]='\0';
}

caretedBlock=0;
memset(ZimToOpen, 0, sizeof(pZim));
ZimToOpen->pZimFile =fopen(filename,"rb");
if (ZimToOpen->pZimFile!=NULL) {
	//Clear then set the pZim filename
	sprintf(ZimToOpen->displayFilename,"%s",filename);
	ZimToOpen->displayFilenameNoPath=strrchr(ZimToOpen->displayFilename,92)+1;
    if (ZimToOpen->displayFilenameNoPath==NULL) ZimToOpen->displayFilenameNoPath=ZimToOpen->displayFilename;
	i=LoadZimFile(ZimToOpen);
	if (i==0) {
		sprintf(tempString, "ZimView - %s",ZimToOpen->displayFilenameNoPath);
		SetWindowText(hwnd, tempString);
		InvalidateRect (hwnd, NULL, TRUE);
	}
	else {
		CloseZimFile(ZimToOpen);
		SetWindowText(hwnd, "ZimView");
		InvalidateRect (hwnd, NULL, TRUE);
		MessageBox(hwnd, "This program is not yet robust enough to handle broken .zim files. File not loaded.", "ZimView", MB_OK);
		return 1;
		}
}
else {
	sprintf(tempString, "Unable to open file: %s.", filename);
	MessageBox(hwnd, tempString, "ZimView", MB_OK|MB_ICONEXCLAMATION);
	return 1;
}

return 0;
}

int DetectTypeOfBlock(HWND hwnd, char *filename)
{
FILE *impBlock;
DWORD magic;

SQUASHFS_SUPER_BLOCK sqshHeader;
DWORD fileLength;
DWORD blockFileLengthData;

impBlock= fopen(filename, "rb");
if (impBlock==NULL) return 0;

fseek(impBlock, 0, SEEK_END);
fileLength=ftell(impBlock);

if (fileLength<8) {fclose(impBlock); return 0;}
rewind(impBlock);

fread(&magic, 4, 1, impBlock);
fread(&blockFileLengthData, 4, 1, impBlock);

if (fileLength==DWORD_swap_endian(blockFileLengthData)+sizeof(struct sBlockHeader)) {
	CheckDlgButton(hwnd, IDC_BLOCKIMPORTINCLUDEHEADER, BST_CHECKED); //the block probably contains a header
	if (magic==BID_DWORD_LOAD) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTLOAD);}
	if (magic==BID_DWORD_ROOT) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTROOT);}
	if (magic==BID_DWORD_CODE) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTCODE);}
	if (magic==BID_DWORD_KERN) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTKERN);}
	if (magic==BID_DWORD_NVRM) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTNVRM);}
	if (magic==BID_DWORD_BOXI) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTBOXI);}
	if (magic==BID_DWORD_VERI) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTVERI);}

	fclose(impBlock);
	return 0;
}

CheckDlgButton(hwnd, IDC_BLOCKIMPORTINCLUDEHEADER, BST_UNCHECKED);

if (((magic==SQUASHFS_MAGIC_LZMA)||(magic==SQUASHFS_MAGIC))&&(fileLength>=sizeof(SQUASHFS_SUPER_BLOCK))) {
	fseek(impBlock, 0, SEEK_SET);
	fread(&sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK), 1, impBlock);
	if ((sqshHeader.inodes>=3)&&(sqshHeader.inodes<=10)) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTCODE);}
	if ((sqshHeader.inodes>=250)&&(sqshHeader.inodes<=350)) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTROOT);}

	fclose(impBlock);
	return 0;
	}

if ((magic & 0xffff)==0x8b1f) { //KERNs i have seen are the only GZIP files i've seen
	CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTKERN);
	fclose(impBlock);
	return 0;
}


if (fileLength==36) {//BOXI without header is 36 bytes
	CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTBOXI);
	fclose(impBlock);
	return 0;
}

if (!(fileLength & 0x1F)) { //if divisible by 32, it _could_ be a VERI block

	if ((magic==BID_DWORD_LOAD)||(magic==BID_DWORD_ROOT)||(magic==BID_DWORD_CODE)||(magic==BID_DWORD_KERN)||(magic==BID_DWORD_NVRM)||(magic==BID_DWORD_BOXI)||(magic==BID_DWORD_VERI)) {
		CheckRadioButton(hwnd,IDC_BLOCKIMPORTROOT, IDC_BLOCKIMPORTNVRM, IDC_BLOCKIMPORTVERI);
		fclose(impBlock);
		return 0;
	}
}

fclose(impBlock);
return 0;
}

int WriteBlockToFile(ZIM_STRUCTURE *LoadedZim, BLOCK_STRUCTURE *Block, FILE *exportFile, int includeHeader)
{
	DWORD exportOffset;
	DWORD exportLength;
	char *exportData;
	struct sBlockHeader blockHeader;
	BOXI_STRUCTURE *tempBoxiStruct;
	VERI_STRUCTURE *tempVeriStruct;
	FILE *usualSourceFile;
	char *usualData;
	int counterVeriPart;

	exportOffset=Block->dwBlockStartLoc;
	exportLength=Block->dwDataLength;
	if (!includeHeader) {
		exportOffset=exportOffset+sizeof(struct sBlockHeader);
	} else
		exportLength=exportLength+sizeof(struct sBlockHeader);

	if (!(Block->flags & BSFLAG_INMEMORY)) {
		//Reserve some memory for the block
		exportData=malloc(exportLength);
		if (exportData==NULL) return EXPORTBLOCK_ERR_MEMORYPROBLEM;
		fseek(LoadedZim->pZimFile, exportOffset, SEEK_SET);
		fread(exportData, exportLength, 1,LoadedZim->pZimFile);

		fwrite(exportData, exportLength, 1, exportFile);
		free(exportData);
	}

	if (Block->flags & BSFLAG_INMEMORY) {
		//Write the header
		if (includeHeader) {
			GenerateBlockHeader(&blockHeader, Block->dwDataLength, Block->dwRealChecksum, Block->name);
			fwrite(&blockHeader, sizeof(struct sBlockHeader), 1, exportFile);
		}
		//If it's a BOXI type
		if (Block->typeOfBlock==BTYPE_BOXI) {
			tempBoxiStruct=Block->ptrFurtherBlockDetail;
			fwrite(&tempBoxiStruct->boxiFileData, sizeof(BOXIBLOCK_STRUCTURE), 1, exportFile);
			return 0;
		}
		//If it's a VERI type
		if (Block->typeOfBlock==BTYPE_VERI) {
			tempVeriStruct=Block->ptrFurtherBlockDetail;
			counterVeriPart=tempVeriStruct->numberVeriObjects;
			while (counterVeriPart) {
				if (tempVeriStruct) {
					fwrite(&tempVeriStruct->veriFileData, sizeof(VERIBLOCK_STRUCTURE), 1, exportFile);
				}
				counterVeriPart--;
				if (counterVeriPart) tempVeriStruct=tempVeriStruct->nextStructure;
			}
		return 0;
		}
		//If it's a usual type, the file data is contained in another one.
		if ((Block->typeOfBlock==BTYPE_CODE)||(Block->typeOfBlock==BTYPE_KERN)||(Block->typeOfBlock==BTYPE_ROOT)||(Block->typeOfBlock==BTYPE_LOAD)||(Block->typeOfBlock==BTYPE_NVRM)) {
			if (Block->sourceFilename) {
				usualSourceFile=fopen(Block->sourceFilename, "rb");
				if (usualSourceFile)	{
					usualData=malloc(Block->dwDataLength);
					fseek(usualSourceFile, Block->dwSourceOffset, SEEK_SET);
					fread(usualData, Block->dwDataLength, 1, usualSourceFile);
					fclose(usualSourceFile);
					fwrite(usualData, Block->dwDataLength, 1, exportFile);
					free(usualData);
				}
			}
			return 0;
		}
	}

return 0;
}


void *NewBlock(ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *newBlock;
	BLOCK_STRUCTURE *tempBlock;
	WORD tempWord;

	newBlock= malloc(sizeof(BLOCK_STRUCTURE));
	if (newBlock==NULL) return NULL;

	if (LoadedZim->displayFilename[0]==0)
		ActivateZimFile(LoadedZim);

	memset(newBlock, 0, sizeof(BLOCK_STRUCTURE));
	if (LoadedZim->wNumBlocks==0) { //if there's no blocks, then the block goes to first
		LoadedZim->first=newBlock;
		newBlock->flags=BSFLAG_INMEMORY; //we'll set that its in memory
		LoadedZim->wNumBlocks++;
		return newBlock;
	}

	//If there are already blocks, add the next block to the end of the linked list
	tempBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (tempBlock->next==NULL) {
				tempBlock->next=newBlock;
		}
		tempBlock=tempBlock->next;
	}

	newBlock->flags=BSFLAG_INMEMORY; //we'll set that its in memory
	LoadedZim->wNumBlocks++;

	return newBlock;
}

void *ReadUsualBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start)
{
	USUAL_STRUCTURE *tempUsualStruct;

	Block->ptrFurtherBlockDetail=malloc(sizeof(USUAL_STRUCTURE));
	memset(Block->ptrFurtherBlockDetail, 0, sizeof(USUAL_STRUCTURE));
	tempUsualStruct=Block->ptrFurtherBlockDetail;

	if (tempUsualStruct==NULL) return NULL;

	fseek(fileToRead, start, SEEK_SET);
	fread(&tempUsualStruct->magicnumber, sizeof(DWORD), 1, fileToRead);
	memset(&tempUsualStruct->displaymagicnumber,0,5);
	memcpy(&tempUsualStruct->displaymagicnumber, &tempUsualStruct->magicnumber, sizeof(DWORD));
	if ((tempUsualStruct->displaymagicnumber[0]<32)||(tempUsualStruct->displaymagicnumber[0]>126)) tempUsualStruct->displaymagicnumber[0]='?';
	if ((tempUsualStruct->displaymagicnumber[1]<32)||(tempUsualStruct->displaymagicnumber[1]>126)) tempUsualStruct->displaymagicnumber[1]='?';
	if ((tempUsualStruct->displaymagicnumber[2]<32)||(tempUsualStruct->displaymagicnumber[2]>126)) tempUsualStruct->displaymagicnumber[2]='?';
	if ((tempUsualStruct->displaymagicnumber[3]<32)||(tempUsualStruct->displaymagicnumber[3]>126)) tempUsualStruct->displaymagicnumber[3]='?';

	tempUsualStruct->sqshHeader=NULL;
	tempUsualStruct->gzipHeader=NULL;
	if (	(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_LZMA)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_LZMA_SWAP)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_SWAP))
		{ //if there's a squashfs file here
	 		tempUsualStruct->sqshHeader=malloc(sizeof(SQUASHFS_SUPER_BLOCK));
			fseek(fileToRead, start, SEEK_SET);
			fread(tempUsualStruct->sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK), 1, fileToRead);
		}

	if ((tempUsualStruct->magicnumber & 0xFFFF)==0x8b1f) {
		tempUsualStruct->gzipHeader=malloc(sizeof(GZIP_HEADER_BLOCK));
		fseek(fileToRead, start, SEEK_SET);
		fread(tempUsualStruct->gzipHeader, sizeof(GZIP_HEADER_BLOCK), 1, fileToRead);
	}

return tempUsualStruct;
}


void *ReadBoxiBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start)
{
	BOXI_STRUCTURE *tempBoxiStruct;

	Block->ptrFurtherBlockDetail=malloc(sizeof(BOXI_STRUCTURE));
	tempBoxiStruct=Block->ptrFurtherBlockDetail;
	if (tempBoxiStruct==NULL) return NULL;

	fseek(fileToRead, start, SEEK_SET);
	fread(&tempBoxiStruct->boxiFileData, sizeof(BOXIBLOCK_STRUCTURE), 1, fileToRead);
	return tempBoxiStruct;
}

void *ReadVeriBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start)
{
VERI_STRUCTURE *tempVeriStruct;
WORD counterVeriPart;

	Block->ptrFurtherBlockDetail=malloc(sizeof(VERI_STRUCTURE)); //the internal structure
	tempVeriStruct=Block->ptrFurtherBlockDetail;
	if (tempVeriStruct==NULL) return NULL;

	if (!(Block->dwDataLength & 0x1F))	{ //should be multiple of 32
		tempVeriStruct->numberVeriObjects=Block->dwDataLength/32;
		fseek(fileToRead, start, SEEK_SET);
		counterVeriPart=tempVeriStruct->numberVeriObjects;
		tempVeriStruct->prevStructure=NULL;
		while (counterVeriPart) {
			fread(&(tempVeriStruct->veriFileData), sizeof(VERIBLOCK_STRUCTURE), 1, fileToRead);
			memset(&tempVeriStruct->displayblockname, 0, 5); //the display name
			memcpy(&tempVeriStruct->displayblockname, &tempVeriStruct->veriFileData.blockName , sizeof(DWORD));
			counterVeriPart--;
			tempVeriStruct->nextStructure=NULL;
			if (counterVeriPart) {
				tempVeriStruct->nextStructure=malloc(sizeof(VERI_STRUCTURE));
				tempVeriStruct->nextStructure->numberVeriObjects=tempVeriStruct->numberVeriObjects;
				tempVeriStruct->nextStructure->prevStructure=tempVeriStruct; //the next structure's previous structure is this structure
				tempVeriStruct=tempVeriStruct->nextStructure; //move onto the next structure
			}
		}
	return Block->ptrFurtherBlockDetail;
	}

return NULL;
}


int GenerateBlockHeader(struct sBlockHeader *blockHeader, DWORD dwDataLength, DWORD dwChecksum, char *name) {
	memset(blockHeader, 0, sizeof(struct sBlockHeader));
  	blockHeader->reserved[0]=0;
	blockHeader->reserved[1]=0;

	blockHeader->blockSignature[0]='B';
	blockHeader->blockSignature[1]='S';
	blockHeader->blockSignature[2]=0;
	blockHeader->blockSignature[3]=0;

	blockHeader->name[0]=name[0];
	blockHeader->name[1]=name[1];
	blockHeader->name[2]=name[2];
	blockHeader->name[3]=name[3];

	blockHeader->dwDataLength=DWORD_swap_endian(dwDataLength);
 	blockHeader->dwChecksum=DWORD_swap_endian(dwChecksum);

	return 0;
}

WORD CalculateOffsetForWriting(ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *tempBlock;

	WORD tempWord;
	WORD countOfWritableBlocks;
	DWORD blockOffset;

	countOfWritableBlocks=0;
	tempBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (!(tempBlock->flags & BSFLAG_DONTWRITE)) countOfWritableBlocks++; //don't count block we won't write
		tempBlock=tempBlock->next;
	}
	blockOffset=12+countOfWritableBlocks*sizeof(DWORD); //after the block and array of offsets

	tempBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (!(tempBlock->flags & BSFLAG_DONTWRITE)) {
			tempBlock->dwDestStartLoc=blockOffset;
			tempBlock->dwChecksum=tempBlock->dwRealChecksum;
			blockOffset+=(tempBlock->dwDataLength+sizeof(struct sBlockHeader)); //the next block offset is after this block
		}
		tempBlock=tempBlock->next;
	}
	LoadedZim->dwTotalFileLen=blockOffset;

return countOfWritableBlocks;
}

int SaveAsZim(ZIM_STRUCTURE *LoadedZim)
{
	FILE *SaveFile;

	OPENFILENAME ofnExportTo;
	char filename[MAX_PATH];
	char temporarypath[MAX_PATH];
	char temporaryfilename[MAX_PATH];
	char tempString[255];
	BLOCK_STRUCTURE *tempBlock;
	WORD tempWord;

	memset(&ofnExportTo,0,sizeof(ofnExportTo));
	ofnExportTo.lStructSize = sizeof(ofnExportTo);
	ofnExportTo.hInstance = GetModuleHandle(NULL);
	ofnExportTo.hwndOwner = GetActiveWindow();
	ofnExportTo.lpstrFile = filename;
	ofnExportTo.nMaxFile = sizeof(tempString);
	ofnExportTo.lpstrTitle = "Save As";
	ofnExportTo.nFilterIndex = 2;
	ofnExportTo.Flags = 0;
	ofnExportTo.lpstrFilter = "All files\0*.*\0Zinwell firmware file (*.zim)\0*.zim\0\0";
	ofnExportTo.lpstrDefExt = "zim";

	sprintf(filename, "*.zim");

	if (GetSaveFileName(&ofnExportTo)) {

		SaveFile=fopen(filename, "r");
		if (!(SaveFile==NULL)) {
			if (MessageBox(hwndMain, "This file already exists. Are you sure you want to replace it?", "Saves As", MB_YESNO|MB_ICONEXCLAMATION)==IDNO)	{
				fclose(SaveFile);
				return 0;
			}
			fclose(SaveFile);
		}


		//we need to write to temp file (so we're not trying to read and write to same file)
		GetTempPath(MAX_PATH, &temporarypath[0]);

		sprintf(&temporaryfilename[0], "%s%s%s", &temporarypath[0], strrchr(filename,92)+1 ,".tmp");

		SaveFile=fopen(temporaryfilename, "w+b");
		WriteZimFile(LoadedZim, SaveFile);
		fclose(SaveFile);
		if (LoadedZim->pZimFile)
			fclose(LoadedZim->pZimFile);

		//then move temp file to filename
		MoveFileEx(&temporaryfilename[0], &filename[0], MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING);


		//Update the window
		sprintf(LoadedZim->displayFilename, "%s", ofnExportTo.lpstrFile);
		LoadedZim->displayFilenameNoPath=strrchr(LoadedZim->displayFilename,92)+1;

		sprintf(tempString, "ZimView - %s",LoadedZim->displayFilenameNoPath);
		SetWindowText(hwndMain, tempString);
		InvalidateRect (hwndMain, NULL, TRUE);

		tempBlock=LoadedZim->first;
		for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {

			//if it's referenced in the old version, then we really can't keep it.
			if ((tempBlock->flags & BSFLAG_DONTWRITE) && (!(tempBlock->flags & BSFLAG_INMEMORY))) {
				DeleteBlock(LoadedZim, tempWord);
				tempWord--;
			}

			tempBlock->flags&=0xFE;


			tempBlock->dwBlockStartLoc=tempBlock->dwDestStartLoc;
			tempBlock=tempBlock->next;
		}


		//Get the new/saved filename to be the one opened.
		LoadedZim->pZimFile = fopen(filename, "rb");
		fseek(LoadedZim->pZimFile, 0, SEEK_END);
		LoadedZim->dwRealFileLen=ftell(LoadedZim->pZimFile);
		LoadedZim->dwChecksum=LoadedZim->dwRealChecksum;

	}


return 0;
}

int WriteZimFile(ZIM_STRUCTURE *LoadedZim, FILE *outputZim)
{
	BLOCK_STRUCTURE *tempBlock;
	struct sZimHeader outputZimHeader;

	WORD tempWord;
	DWORD startLoc;
	DWORD adler;
	WORD wNumBlocks;

	ADLER_STRUCTURE adlerholder;

	wNumBlocks=CalculateOffsetForWriting(LoadedZim);

	outputZimHeader.wNumBlocks= WORD_swap_endian(wNumBlocks); //first calculate the offsets to write
	outputZimHeader.dwTotalFileLen=DWORD_swap_endian(LoadedZim->dwTotalFileLen);
	outputZimHeader.dwChecksum=0; //leave blank for now
	outputZimHeader.wCustomerNumber=LoadedZim->wCustomerNumber;
	//Write the zim header
	fwrite(&outputZimHeader, sizeof(struct sZimHeader), 1, outputZim);

	//Write the start offsets of all the blocks
	tempBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (!(tempBlock->flags & BSFLAG_DONTWRITE)) { //don't write the start loc for unwanted blocks
			startLoc=DWORD_swap_endian(tempBlock->dwDestStartLoc);
			fwrite(&startLoc, sizeof(DWORD), 1, outputZim);
		}
		tempBlock=tempBlock->next;
	}

	//Write each block
	tempBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (!(tempBlock->flags & BSFLAG_DONTWRITE)) { //don't write blocks we don't want to write
			WriteBlockToFile(LoadedZim, tempBlock, outputZim, 1);
		}
		tempBlock=tempBlock->next;
	}

	//Calculate Adler-32
	fflush(outputZim);
	memset(&adlerholder, 0, sizeof(adlerholder)); //reset adler checksum to zero
	adler=AdlerOnFile(outputZim, &adlerholder, sizeof(DWORD), LoadedZim->dwTotalFileLen-4);

	LoadedZim->dwRealChecksum=adler;
	adler=DWORD_swap_endian(adler);
	fflush(outputZim);
	fseek(outputZim, 0, SEEK_SET);
	fwrite(&adler, sizeof(DWORD), 1, outputZim);

	return 0;
}

int DisableSelected(ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *tempBlockStruct;
	WORD tempWord;

	tempBlockStruct=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (tempBlockStruct->flags & BSFLAG_ISSELECTED)
			tempBlockStruct->flags =tempBlockStruct->flags ^ BSFLAG_DONTWRITE;
		tempBlockStruct=tempBlockStruct->next;
	}
return 0;
}

int DeleteSelectedBlocks(ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *tempBlockStruct;
	WORD tempWord;
	int firstBlockDeleted=-1;

	tempBlockStruct=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; 1) {
		if (tempBlockStruct->flags & BSFLAG_ISSELECTED)	{
			if (firstBlockDeleted==-1)
				firstBlockDeleted=tempWord;
			DeleteBlock(LoadedZim, tempWord);
			tempWord=0; //because we've deleted a block it's safest (but definitely not most efficient) to start back at the start
			tempBlockStruct=LoadedZim->first;
		}
		else {
			tempBlockStruct=tempBlockStruct->next;
			tempWord++;
		}
	}

	return firstBlockDeleted;
}


int DeleteBlock(ZIM_STRUCTURE *LoadedZim, WORD blocknumber)
{
	BLOCK_STRUCTURE *ptrBlockStruct;
	BLOCK_STRUCTURE *prevBlockStruct;

	//BLOCK_STRUCTURE
	WORD tempWord;

	if (blocknumber>=LoadedZim->wNumBlocks) return 0; //if we don't have a block that high, don't bother

	//this can delete middle or end blocks, not the first one
	if (blocknumber>0) {
		ptrBlockStruct=LoadedZim->first;
		for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
			if ((tempWord+1)==blocknumber)
				prevBlockStruct=ptrBlockStruct; //save a pointer to the structure before the deleted one

			if (tempWord==blocknumber) {
				prevBlockStruct->next=ptrBlockStruct->next; //the previous pointer points to what the deleted one did
				FreeBlockMemory(ptrBlockStruct); 				//Free memory used by this structure
				LoadedZim->wNumBlocks--; //decrease the number of blocks;
				return 1;
			}
			ptrBlockStruct=ptrBlockStruct->next;
		}
	}

	if (blocknumber==0) {
		ptrBlockStruct= LoadedZim->first; //get the first block
		LoadedZim->first=ptrBlockStruct->next; //move the start of the list to the second (or null)
		FreeBlockMemory(ptrBlockStruct);
		LoadedZim->wNumBlocks--;
	}


return 1;
}


int SwapBlocks(ZIM_STRUCTURE *LoadedZim, WORD blockA, WORD blockB)
{
	BLOCK_STRUCTURE *ptrBlockStructA;
	BLOCK_STRUCTURE *ptrBlockStructB;
	BLOCK_STRUCTURE *ptrCounterBlock;

	BLOCK_STRUCTURE tempBlockHolder;

	WORD tempWord;

	if (blockA>=LoadedZim->wNumBlocks) return 0;
	if (blockB>=LoadedZim->wNumBlocks) return 0;

	ptrBlockStructA=NULL;
	ptrBlockStructB=NULL;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {

		if (tempWord==blockA) ptrBlockStructA=ptrCounterBlock;
		if (tempWord==blockB) ptrBlockStructB=ptrCounterBlock;

		ptrCounterBlock=ptrCounterBlock->next;
	}


	//Swap structures (note that in a linked list, this also swaps the 'next' pointer, which we don't want
	if ((ptrBlockStructA) && (ptrBlockStructB)) {
		memcpy(&tempBlockHolder, ptrBlockStructA, sizeof(BLOCK_STRUCTURE)); //copy A to temp
		memcpy(ptrBlockStructA, ptrBlockStructB, sizeof(BLOCK_STRUCTURE)); //copy B to A
		memcpy(ptrBlockStructB, &tempBlockHolder, sizeof(BLOCK_STRUCTURE)); //copy temp to B

		//Swap the pointers back (to original places) so that only the
	    //data of each block is swapped without affecting linked list
		tempBlockHolder.next=ptrBlockStructA->next;
		ptrBlockStructA->next=ptrBlockStructB->next;
		ptrBlockStructB->next=tempBlockHolder.next;
	}
return 1;
}

int MoveBlockAfter(ZIM_STRUCTURE *LoadedZim, int blockSource, int blockDest)
{

	BLOCK_STRUCTURE sourceBlockCopy;

	BLOCK_STRUCTURE *ptrSourceBlock;
	BLOCK_STRUCTURE *ptrDestBlock;
	BLOCK_STRUCTURE *ptrBeforeSourceBlock;

	BLOCK_STRUCTURE *ptrCounterBlock;
	int i;

	if (blockSource==blockDest) return 0;
	if (blockSource==blockDest+1) return 0;

	if (blockSource>=LoadedZim->wNumBlocks) return 0;
	if (blockDest>=LoadedZim->wNumBlocks) return 0;

	if (blockSource>blockDest) caretedBlock=blockDest+1;
	else caretedBlock=blockDest;

	ptrCounterBlock=LoadedZim->first;
	for (i=0; i<LoadedZim->wNumBlocks; i++) {
		if (i==blockSource) ptrSourceBlock=ptrCounterBlock;
		if (i==blockDest) ptrDestBlock=ptrCounterBlock;
		if ((i+1)==blockSource) ptrBeforeSourceBlock=ptrCounterBlock;

		ptrCounterBlock=ptrCounterBlock->next;
	}

	if (blockSource==0) {
		memcpy(&sourceBlockCopy, ptrSourceBlock, sizeof(BLOCK_STRUCTURE)); //make copy of source block
		ptrSourceBlock->next=ptrDestBlock->next;
		ptrDestBlock->next=ptrSourceBlock;
		LoadedZim->first=sourceBlockCopy.next;
		return 1;
	}

	if (blockDest==-1) { //move it "after header" i.e. make it first block
		memcpy(&sourceBlockCopy, ptrSourceBlock, sizeof(BLOCK_STRUCTURE)); //make copy of source block
		ptrSourceBlock->next=LoadedZim->first;
		LoadedZim->first=ptrSourceBlock;
		ptrBeforeSourceBlock->next=sourceBlockCopy.next;
		return 1;
	}

	memcpy(&sourceBlockCopy, ptrSourceBlock, sizeof(BLOCK_STRUCTURE)); //make copy of source block

	ptrSourceBlock->next=ptrDestBlock->next;
	ptrDestBlock->next=ptrSourceBlock;
	ptrBeforeSourceBlock->next=sourceBlockCopy.next;


	return 1;
}



int FreeBlockMemory(BLOCK_STRUCTURE *Block)
{
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;

	WORD counterVeriPart;

	switch (Block->typeOfBlock) {
		case BTYPE_VERI: /* NEED TO GET RID OF VERI LINKED LISTS */
			tempVeriStruct=Block->ptrFurtherBlockDetail;
			counterVeriPart=tempVeriStruct->numberVeriObjects;
			while (counterVeriPart) {
				while (tempVeriStruct->nextStructure) {
						tempVeriStruct=tempVeriStruct->nextStructure;
				}
				if (tempVeriStruct->prevStructure) {tempVeriStruct->prevStructure->nextStructure=NULL;}
				free(tempVeriStruct);
				counterVeriPart--;
				if (counterVeriPart) tempVeriStruct=Block->ptrFurtherBlockDetail;
			}
			break;
		case BID_DWORD_ROOT:
		case BID_DWORD_CODE:
		case BID_DWORD_KERN:
		case BID_DWORD_LOAD:
		case BID_DWORD_NVRM:
			tempUsualStruct = Block->ptrFurtherBlockDetail;
			if (tempUsualStruct->sqshHeader) {free(tempUsualStruct->sqshHeader);}
			if (tempUsualStruct->gzipHeader) {free(tempUsualStruct->gzipHeader);}
			free(Block->ptrFurtherBlockDetail); //free the further boxi block detail
			break;
		case BID_DWORD_BOXI:
		default:
			free(Block->ptrFurtherBlockDetail); //free the further block detail
			break;
	}

	free(Block); //free the block itself

return 1;
}


BLOCK_STRUCTURE *GetBlockNumber(ZIM_STRUCTURE *LoadedZim, WORD blocknumber)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (tempWord==blocknumber) return ptrCounterBlock;

		ptrCounterBlock=ptrCounterBlock->next;
	}

return NULL;

}

int SelectToggleBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD blockToSelect)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (tempWord==blockToSelect) {
			ptrCounterBlock->flags^=BSFLAG_ISSELECTED;
			RedrawBlock(hwnd, LoadedZim, tempWord);
			return 0;
		}
		ptrCounterBlock=ptrCounterBlock->next;
	}

	UpdateWindow(hwnd);
	return 0;
}

int	SelectOnlyBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD blockToSelect)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;


	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (tempWord==blockToSelect) {
			ptrCounterBlock->flags|=BSFLAG_ISSELECTED;
			RedrawBlock(hwnd, LoadedZim, tempWord);
		}
		else {
			if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
				ptrCounterBlock->flags^=(BSFLAG_ISSELECTED);
				RedrawBlock(hwnd, LoadedZim, tempWord);
			}
		}

		ptrCounterBlock=ptrCounterBlock->next;
	}

	UpdateWindow(hwnd);
	return 0;
}


int SelectRangeBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WORD startblock, WORD endblock)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	WORD start;
	WORD end;

	if (startblock>endblock) {start=endblock; end=startblock;}
	else {start=startblock; end=endblock;}

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if ((tempWord>=start)&&(tempWord<=end)) {
			ptrCounterBlock->flags|=BSFLAG_ISSELECTED;
			RedrawBlock(hwnd, LoadedZim, tempWord);
		}
		else {
			if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
				ptrCounterBlock->flags^=(BSFLAG_ISSELECTED);
				RedrawBlock(hwnd, LoadedZim, tempWord);
			}
		}

		ptrCounterBlock=ptrCounterBlock->next;
	}

	UpdateWindow(hwnd);
	return 0;
}


int SelectAllBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (!(ptrCounterBlock->flags & BSFLAG_ISSELECTED)) {
			ptrCounterBlock->flags ^= BSFLAG_ISSELECTED;
			RedrawBlock(hwnd, LoadedZim, tempWord);
		}
		ptrCounterBlock=ptrCounterBlock->next;
	}

	UpdateWindow(hwnd);
	return 0;
}

int SelectNoBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
			ptrCounterBlock->flags ^= BSFLAG_ISSELECTED;
			RedrawBlock(hwnd, LoadedZim, tempWord);
		}
		ptrCounterBlock=ptrCounterBlock->next;
	}

	return 0;
}


int SelectedCount(ZIM_STRUCTURE *LoadedZim)
{
	int n;

	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	n=0;
	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (ptrCounterBlock->flags&BSFLAG_ISSELECTED)
			n++;
		ptrCounterBlock=ptrCounterBlock->next;
	}
	return n;
}

int SelectedFirst(ZIM_STRUCTURE *LoadedZim, BLOCK_STRUCTURE **ptrBlock)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED)	{
			*ptrBlock=ptrCounterBlock;
			return tempWord;
		}
		ptrCounterBlock=ptrCounterBlock->next;
	}
	ptrBlock=NULL;
	return -1;
}



void MouseBlockSelect(int *ptrBlockNumber, HWND hwnd, ZIM_STRUCTURE *LoadedZim, int x, int y)
{
	int i;

	for (i=0;i<numberDisplayedBlocks; i++) {
		if ((y>=paintSelectRects[i].top)&&(y<=paintSelectRects[i].bottom)) {
			*ptrBlockNumber=i+topDisplayBlock;
		}
	}
return;
}

void MouseBlockMoveSelect(int *ptrBlockNumber, HWND hwnd, ZIM_STRUCTURE *LoadedZim, int x, int y)
{
	int i;

	for (i=0;i<numberDisplayedBlocks; i++) {
		if ((y<(paintSelectRects[i].top+paintSelectRects[i].bottom)/2)) {
			*ptrBlockNumber=i+topDisplayBlock-1;
			return;
		}
	}

	if (y>=paintSelectRects[numberDisplayedBlocks-1].top)
		*ptrBlockNumber=topDisplayBlock+numberDisplayedBlocks-1;

return;
}

int EditCopySelected(HWND hwnd, ZIM_STRUCTURE *LoadedZim, BOOL bCut)
{
#define CLIPBOARD_BUFFER_SIZE 0xFFFF

	LPTSTR  lptstrCopy;
	HGLOBAL hglbCopy;
	HANDLE hSuccess;

	int i;
	WORD n;
	int cch;
	char buffer[CLIPBOARD_BUFFER_SIZE];

	BLOCK_STRUCTURE *ptrCounterBlock;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;
	WORD counterVeriPart;

	n=SelectedCount(LoadedZim);

	if (n==0) return 0;

	if (!OpenClipboard(hwnd)) return 0;
	EmptyClipboard();

	//This writes a stream of data to the buffer, then places a copy in the clipboard
	//The data stream is the internal blockdata, followed by the additional data
	memset(&buffer, 0, CLIPBOARD_BUFFER_SIZE);
	ptrCounterBlock=LoadedZim->first;
	cch=0;

	memcpy(&buffer[cch], &n, sizeof(WORD));
	cch+=sizeof(WORD);

	for (i=0; i<LoadedZim->wNumBlocks; i++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
			memcpy(&buffer[cch], ptrCounterBlock, sizeof(BLOCK_STRUCTURE));
			cch+=sizeof(BLOCK_STRUCTURE);

			if (ptrCounterBlock->ptrFurtherBlockDetail)	{
				switch (ptrCounterBlock->typeOfBlock)	{
					case BTYPE_BOXI:
						memcpy(&buffer[cch], ptrCounterBlock->ptrFurtherBlockDetail, sizeof(BOXI_STRUCTURE));
						cch+=sizeof(BOXI_STRUCTURE);
						break;
					case BTYPE_VERI:
						tempVeriStruct=ptrCounterBlock->ptrFurtherBlockDetail;
						counterVeriPart=tempVeriStruct->numberVeriObjects;
						while (counterVeriPart) {
							memcpy(&buffer[cch], tempVeriStruct, sizeof(VERI_STRUCTURE));
							cch+=sizeof(VERI_STRUCTURE);
							counterVeriPart--;
							if (counterVeriPart) tempVeriStruct=tempVeriStruct->nextStructure;
						}
						break;
					case BTYPE_ROOT:
					case BTYPE_CODE:
					case BTYPE_KERN:
					case BTYPE_LOAD:
					case BTYPE_NVRM:
						tempUsualStruct=ptrCounterBlock->ptrFurtherBlockDetail;
						memcpy(&buffer[cch], tempUsualStruct, sizeof(USUAL_STRUCTURE));
						cch+=sizeof(USUAL_STRUCTURE);
						if (tempUsualStruct->gzipHeader)	{
							memcpy(&buffer[cch], tempUsualStruct->gzipHeader, sizeof(GZIP_HEADER_BLOCK));
							cch+=sizeof(GZIP_HEADER_BLOCK);
						}
						if (tempUsualStruct->sqshHeader)	{
							memcpy(&buffer[cch], tempUsualStruct->sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK));
							cch+=sizeof(SQUASHFS_SUPER_BLOCK);
						}
						break;
				}
			}

		}
		ptrCounterBlock=ptrCounterBlock->next;
	}

	//Allocate some global memory for the binary buffer
	hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (cch));
    if (hglbCopy == NULL) {
        CloseClipboard();
        return 0;
	}

	// Lock the handle and copy the binary data to the buffer.
    lptstrCopy = GlobalLock(hglbCopy);
    memcpy(lptstrCopy, &buffer,  cch);
    GlobalUnlock(hglbCopy);
	SetClipboardData(uZimBlockFormat, hglbCopy);


	/* TEXT */
	//This writes text to the buffer, then places a copy of it the clipboard
	memset(&buffer, 0, CLIPBOARD_BUFFER_SIZE);
	ptrCounterBlock=LoadedZim->first;
	cch=0;

	for (i=0; i<LoadedZim->wNumBlocks; i++) {

		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
			sprintf(buffer+cch, "%s\r\nStart: 0x%x\r\nLength: 0x%x\r\nChecksum: %08x\r\nMD5: ", ptrCounterBlock->name,  ptrCounterBlock->dwDestStartLoc, ptrCounterBlock->dwDataLength, ptrCounterBlock->dwRealChecksum);
			cch=strlen(buffer);

			MD5HexString(buffer+cch, ptrCounterBlock->md5);
			cch=strlen(buffer);

			sprintf(buffer+cch, "\r\n\r\n");
			cch=strlen(buffer);

			if (bCut) {
				ptrCounterBlock->flags|=BSFLAG_DONTWRITE;
				RedrawBlock(hwnd, LoadedZim, i);
			}

			if (cch>1536) i=LoadedZim->wNumBlocks;
		}

		ptrCounterBlock=ptrCounterBlock->next;
	}

	// Allocate a global memory object for the text.
    hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (cch + 1));
    if (hglbCopy == NULL) {
        CloseClipboard();
        return 0;
	}

    // Lock the handle and copy the text to the buffer.
    lptstrCopy = GlobalLock(hglbCopy);
    memcpy(lptstrCopy, &buffer,  cch);
    lptstrCopy[cch] = 0;    // null character
    GlobalUnlock(hglbCopy);

    // Place the handle on the clipboard.
	hSuccess = SetClipboardData(CF_TEXT, hglbCopy);
	if (!hSuccess) {
		GlobalFree(hglbCopy); //we only need to free if the SetClipboardData didn't work
		CloseClipboard();
		return 0;
	}

    CloseClipboard();
	return 1;
}

int EditPaste(HWND hwnd, ZIM_STRUCTURE *LoadedZim) {
	char *ptrCbData;
	HANDLE hglb;
	int cch;
	WORD n;
	WORD count;

	BLOCK_STRUCTURE *newBlock;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;
	WORD counterVeriPart;

	if (OpenClipboard(hwnd))  {
	if (IsClipboardFormatAvailable(uZimBlockFormat))	{ //only proceed if they have my clipboard format
	   	hglb = GetClipboardData(uZimBlockFormat);
		cch=0;

		ptrCbData = GlobalLock(hglb);

		memcpy(&n, ptrCbData+cch, sizeof(WORD));
		cch+=sizeof(WORD);

		for (count=0; count<n; count++) {
			newBlock = NewBlock(LoadedZim);
			memcpy(newBlock, ptrCbData+cch, sizeof(BLOCK_STRUCTURE));
			cch+=sizeof(BLOCK_STRUCTURE);

			newBlock->next=NULL;
			newBlock->flags=BSFLAG_INMEMORY;

			if (newBlock->ptrFurtherBlockDetail) {
				switch(newBlock->typeOfBlock)	{
					case BTYPE_ROOT:
					case BTYPE_CODE:
					case BTYPE_KERN:
					case BTYPE_LOAD:
					case BTYPE_NVRM:
						newBlock->ptrFurtherBlockDetail=malloc(sizeof(USUAL_STRUCTURE));
						memcpy(newBlock->ptrFurtherBlockDetail, ptrCbData+cch, sizeof(USUAL_STRUCTURE));
						cch+=sizeof(USUAL_STRUCTURE);
						tempUsualStruct=newBlock->ptrFurtherBlockDetail;
						if (tempUsualStruct->gzipHeader)	{
							tempUsualStruct->gzipHeader=malloc(sizeof(GZIP_HEADER_BLOCK));
							memcpy(tempUsualStruct->gzipHeader, ptrCbData+cch, sizeof(GZIP_HEADER_BLOCK));
							cch+=sizeof(GZIP_HEADER_BLOCK);
						}
						if (tempUsualStruct->sqshHeader)	{
							tempUsualStruct->sqshHeader=malloc(sizeof(SQUASHFS_SUPER_BLOCK));
							memcpy(tempUsualStruct->sqshHeader, ptrCbData+cch, sizeof(SQUASHFS_SUPER_BLOCK));
							cch+=sizeof(SQUASHFS_SUPER_BLOCK);
						}
						break;
					case BTYPE_BOXI:
						newBlock->ptrFurtherBlockDetail=malloc(sizeof(BOXI_STRUCTURE));
						memcpy(newBlock->ptrFurtherBlockDetail, ptrCbData+cch, sizeof(BOXI_STRUCTURE));
						cch+=sizeof(BOXI_STRUCTURE);
						break;
					case BTYPE_VERI:
						newBlock->ptrFurtherBlockDetail=malloc(sizeof(VERI_STRUCTURE));
						tempVeriStruct=newBlock->ptrFurtherBlockDetail;

						memcpy(tempVeriStruct, ptrCbData+cch, sizeof(VERI_STRUCTURE));
						cch+=sizeof(VERI_STRUCTURE);

						counterVeriPart=tempVeriStruct->numberVeriObjects;
						tempVeriStruct->prevStructure=NULL;
						while (counterVeriPart) {
							counterVeriPart--;
							tempVeriStruct->nextStructure=NULL;
							if (counterVeriPart) {
								tempVeriStruct->nextStructure=malloc(sizeof(VERI_STRUCTURE));
								memcpy(tempVeriStruct->nextStructure, ptrCbData+cch, sizeof(VERI_STRUCTURE));
								cch+=sizeof(VERI_STRUCTURE);

								tempVeriStruct->nextStructure->numberVeriObjects=tempVeriStruct->numberVeriObjects;
								tempVeriStruct->nextStructure->prevStructure=tempVeriStruct; //the next structure's previous structure is this structure
								tempVeriStruct=tempVeriStruct->nextStructure; //move onto the next structure
							}
						}

						break;
					default:
						newBlock->ptrFurtherBlockDetail=NULL;
						break;
				}
			}
		}

		GlobalUnlock(hglb);
	}
		CloseClipboard();
	}

	return 1;
}


int RedrawSelectedBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED)
			RedrawBlock(hwnd, LoadedZim, tempWord);

		ptrCounterBlock=ptrCounterBlock->next;
	}

	return 0;
}

int RedrawBlock(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int block)
{
	if (block<topDisplayBlock)
		return 0; //the block is before any to be displayed
	if (block>=(topDisplayBlock+numberDisplayedBlocks))
		return 0;	//the block is too far down to display

	InvalidateRect(hwnd, &paintSelectRects[block-topDisplayBlock], FALSE);
	return 1;
}


int RedrawBetweenBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim, int startblock, int endblock)
{
	WORD tempWord;

	if (startblock>endblock) { //make startblock smaller than endblock
		tempWord = startblock;
	    startblock = endblock;
		endblock = tempWord;
	}

	for (tempWord=0; tempWord<numberDisplayedBlocks; tempWord++)	{
		if (((tempWord+topDisplayBlock)>=startblock)||((tempWord+topDisplayBlock)<=endblock))
			InvalidateRect(hwnd, &paintSelectRects[tempWord], FALSE);
	}

	return 0;
}

int RedrawMoveIndicator(HWND hwnd, int loc)
{
	RECT tempRect;

	if (loc>=topDisplayBlock) {
		tempRect.top=paintSelectRects[loc-topDisplayBlock].bottom-MOVEINDICATOR_WIDTH;
		tempRect.bottom=paintSelectRects[loc-topDisplayBlock].bottom+MOVEINDICATOR_WIDTH;
		tempRect.left=paintSelectRects[loc-topDisplayBlock].left;
		tempRect.right=paintSelectRects[loc-topDisplayBlock].right;
	} else {
		tempRect.top=paintSelectRects[0].top-MOVEINDICATOR_WIDTH;
		tempRect.bottom=paintSelectRects[0].top+MOVEINDICATOR_WIDTH;
		tempRect.right=paintSelectRects[0].right;
		tempRect.left=paintSelectRects[0].left;
	}
	InvalidateRect (hwnd, &tempRect, TRUE);
	UpdateWindow(hwnd);

return 0;
}

void WINAPI InitMenu(HMENU hmenu)
{
    int  cMenuItems;
    int  nPos;
    UINT id;
    UINT fuFlags;

	cMenuItems = GetMenuItemCount(hmenu);

	for (nPos = 0; nPos < cMenuItems; nPos++)
    {
        id = GetMenuItemID(hmenu, nPos);

        switch (id)
        {
			case IDM_CLOSE:		//there must be an edited file open
				if (!pZim.displayFilename[0]) //if the zim isn't loaded
					fuFlags = MF_BYCOMMAND | MF_GRAYED;
				else
					fuFlags = MF_BYCOMMAND | MF_ENABLED;
    	        EnableMenuItem(hmenu, id, fuFlags);
				break;

			case IDM_EDITCUT:	//at least one block needs to be selected
			case IDM_EDITCOPY:
			case IDM_EDITDELETE:
			case IDM_EDITCLEAR:
			case IDM_MOVEUP:
			case IDM_MOVEDOWN:
				if (!pZim.displayFilename[0]) { //if the zim isn't loaded
					fuFlags = MF_BYCOMMAND | MF_GRAYED;
				} else {
					if (SelectedCount(&pZim)>0)
						fuFlags = MF_BYCOMMAND | MF_ENABLED;
					else
						fuFlags = MF_BYCOMMAND | MF_GRAYED;
				}
    	        EnableMenuItem(hmenu, id, fuFlags);
				break;
			case IDM_SELECTALL:	//we need at least one block to be able to do these
			case IDM_BLOCKEXPORT:
			case IDM_BLOCKCREATEVERIBLOCK:
				if (pZim.displayFilename[0] && (pZim.wNumBlocks>0))
					fuFlags = MF_BYCOMMAND | MF_ENABLED;
				else
					fuFlags = MF_BYCOMMAND | MF_GRAYED;
    	        EnableMenuItem(hmenu, id, fuFlags);
				break;
			case IDM_EDITPASTE:
				if (IsClipboardFormatAvailable(uZimBlockFormat))
					fuFlags = MF_BYCOMMAND | MF_ENABLED;
				else
					fuFlags = MF_BYCOMMAND | MF_GRAYED;
    	        EnableMenuItem(hmenu, id, fuFlags);
				break;
		}
	}

	return;
}

int PopulatePopupMenu(HMENU hMenu)
{
	MENUITEMINFO menuItemInfo;
	int index;

	memset(&menuItemInfo, 0, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize=sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_STRING|MIIM_STATE|MIIM_ID;
	menuItemInfo.fType = MFT_STRING;

	//Cut and copy
	menuItemInfo.fState = 0;
	if (SelectedCount(&pZim)==0)
		menuItemInfo.fState = MFS_GRAYED;

	index=0;
	menuItemInfo.dwTypeData="Cu&t";
	menuItemInfo.cch = 4;
	menuItemInfo.wID =IDM_EDITCUT;
	InsertMenuItem(hMenu, index, TRUE, &menuItemInfo);
	index++;

	menuItemInfo.dwTypeData="&Copy";
	menuItemInfo.cch = 5;
	menuItemInfo.wID =IDM_EDITCOPY;
	InsertMenuItem(hMenu, index, TRUE, &menuItemInfo);


	menuItemInfo.fState = MFS_GRAYED;
	if (IsClipboardFormatAvailable(uZimBlockFormat))
		menuItemInfo.fState = 0;

	menuItemInfo.dwTypeData="&Paste";
	menuItemInfo.cch = 6;
	menuItemInfo.wID =IDM_EDITPASTE;
	InsertMenuItem(hMenu, 2, TRUE, &menuItemInfo);

	menuItemInfo.dwTypeData="&Delete";
	menuItemInfo.cch = 7;
	menuItemInfo.wID =IDM_EDITDELETE;
	InsertMenuItem(hMenu, 3, TRUE, &menuItemInfo);

	menuItemInfo.fMask = MIIM_TYPE;
	menuItemInfo.fType= MFT_SEPARATOR;
	InsertMenuItem(hMenu, 4, TRUE, &menuItemInfo);

	menuItemInfo.fMask = MIIM_STRING|MIIM_ID;
	menuItemInfo.fType = MFT_STRING;
	menuItemInfo.dwTypeData="&Export block...";
	menuItemInfo.wID =IDM_BLOCKEXPORT;
	menuItemInfo.cch = 16;
	InsertMenuItem(hMenu, 5, TRUE, &menuItemInfo);

	menuItemInfo.dwTypeData="P&roperties";
	menuItemInfo.wID =IDM_PROPERTIES;
	menuItemInfo.cch = 11;
	InsertMenuItem(hMenu, 6, TRUE, &menuItemInfo);

return 1;
}

long OnMouseWheel(HWND hwnd, ZIM_STRUCTURE *LoadedZim, short nDelta)
{
int oldTopDisplayBlock;

oldTopDisplayBlock = topDisplayBlock;

if (nDelta<0) {
	topDisplayBlock++;
	if (topDisplayBlock>=LoadedZim->wNumBlocks) topDisplayBlock=LoadedZim->wNumBlocks-1;
}
if (nDelta>0) {
	topDisplayBlock--;
	if (topDisplayBlock<0) topDisplayBlock=0;
}

if (oldTopDisplayBlock!=topDisplayBlock)
	InvalidateRect(hwnd, NULL, FALSE);


    SCROLLINFO si = { sizeof(si) };

    si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;

    si.nPos  = topDisplayBlock;         // scrollbar thumb position
    si.nPage = numberDisplayedBlocks;        // number of lines in a page (i.e. rows of text in window)
    si.nMin  = 0;
    si.nMax  = LoadedZim->wNumBlocks - 1;      // total number of lines in file (i.e. total scroll range)

    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);



return 0;
}
