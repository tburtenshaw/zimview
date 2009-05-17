//External variables defined in main file.
extern HINSTANCE hInst;

//Constructs, typedefs, etc.
#define C_PAGES 2
typedef struct tag_dlghdr {
    HWND hwndTab;       // tab control
    HWND hwndDisplay;   // current child dialog box
    RECT rcDisplay;     // display rectangle for the tab control
    DLGTEMPLATE *apRes[C_PAGES];
	ZIM_STRUCTURE *LoadedZim;
	BLOCK_STRUCTURE *selectedBlock;
	BOXI_STRUCTURE dlgBoxiStruct;
	int oldiSel;
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
void FillBoxiStructFromBoxiDlg(HWND hwnd, BOXI_STRUCTURE *boxiStruct);
void FillBoxiDlgFromBoxiStruct(HWND hwnd, BOXI_STRUCTURE *boxiStruct, int base);
void CreateValidBoxiBlockFromBoxiStruct(BLOCK_STRUCTURE *boxiBlock, BOXI_STRUCTURE *boxiStruct);
void PropertiesApplyChanges(HWND hwnd);
int DetectTypeOfBlock(HWND hwnd, char *filename);
