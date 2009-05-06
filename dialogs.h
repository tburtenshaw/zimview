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
} DLGHDR_PROPERTIES;


BOOL _stdcall ChildDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); //for the tabs - this basically does nothing.
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
void FillBoxiStructFromBoxiDlg(HWND hwnd, BLOCK_STRUCTURE *boxiBlock);
void FillBoxiDlgFromBoxiStruct(HWND hwnd, BLOCK_STRUCTURE *boxiBlock);
