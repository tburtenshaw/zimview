//External variables defined in main file.
extern HINSTANCE hInst;

//Constructs, typedefs, etc.
#define C_PAGES 2
#define PT_ZIM 0
#define PT_BLOCK 1
#define PT_MULTI 2
typedef struct tag_dlghdr {
    HWND hwndTab;       // tab control
    HWND hwndDisplay;   // current child dialog box
	HWND hwndMain;		// the parent window
    DLGTEMPLATE *apRes[C_PAGES];
	ZIM_STRUCTURE *LoadedZim;
	BLOCK_STRUCTURE *selectedBlock;
	BOXI_STRUCTURE dlgBoxiStruct;
	VERI_STRUCTURE dlgVeriStruct;
	int propertiesType;
	int oldiSel;
	int selectedBlockNumber;	//if there is only one block
} DLGHDR_PROPERTIES;


BOOL _stdcall ChildDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); //This proc basically does nothing.
BOOL _stdcall AboutDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall PropertiesDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall BlockCreateBoxiDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall BlockCreateVeriDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall ChangeCustomerNumberDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall BlockExportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL _stdcall BlockImportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//Tool functions used by dialog boxes
DLGTEMPLATE * WINAPI DoLockDlgRes(LPCSTR lpszResName);
void BlockExportDetailsUpdate(HWND hwnd, ZIM_STRUCTURE *ZimToUse, int blockid);
void PropertiesDlg_ChangeSelection(HWND hwnd);
void FillDlgItemWithSquashFSData(HWND hDlg, int nIDDlgItem, SQUASHFS_SUPER_BLOCK *sqshHeader);
void FillDlgItemWithGzipData(HWND hDlg, int nIDDlgItem, GZIP_HEADER_BLOCK *sqshHeader);
void FillDlgItemWithMaclistData(HWND hDlg, int nIDDlgItem, MACLIST_HEADER_BLOCK *macHeader);
void FillBoxiStructFromBoxiDlg(HWND hwnd, BOXI_STRUCTURE *boxiStruct);
void FillBoxiDlgFromBoxiStruct(HWND hwnd, BOXI_STRUCTURE *boxiStruct, int base);
void CreateValidBoxiBlockFromBoxiStruct(BLOCK_STRUCTURE *boxiBlock, BOXI_STRUCTURE *boxiStruct);
void FillPropertiesMainDlg(HWND hwnd, BLOCK_STRUCTURE *selectedBlock);
void FillPropertiesZimDlg(HWND hwnd, ZIM_STRUCTURE *zimStruct);
int VeriApplyChanges(BLOCK_STRUCTURE *selectedBlock, DLGHDR_PROPERTIES *pHdr);
int VeriStructMakeCopy(VERI_STRUCTURE *destVeri, VERI_STRUCTURE *sourceVeri);
int VeriStructCopyData(VERI_STRUCTURE *destVeri, VERI_STRUCTURE *sourceVeri);
int VeriStructFree(VERI_STRUCTURE *deleteVeri);
void DisplayVeriListView(HWND hwnd, VERI_STRUCTURE *veriStruct);
void PropertiesApplyChanges(HWND hwnd);
int DetectTypeOfBlock(HWND hwnd, char *filename);

LRESULT ProcessCustomDraw (LPNMLVCUSTOMDRAW lplvcd);
