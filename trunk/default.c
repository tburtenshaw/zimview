/*@@ Wedit generated application. Written Fri May 18 08:00:53 2001
 @@header: d:\lcc\examples\defaultproject\defaultres.h
 @@resources: d:\lcc\examples\defaultproject\default.rc
 Do not edit outside the indicated areas */
/*<---------------------------------------------------------------------->*/
/*<---------------------------------------------------------------------->*/
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "defaultres.h"
/* --- The following code comes from i:\program files\lcc\lib\wizard\dlgbased.tpl. */
/*<---------------------------------------------------------------------->*/

/*
Template for a dialog based application. The main procedure for this
template is the DialogFunc below. Modify it to suit your needs.
*/
/* prototype for the dialog box function. */
static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
/*
Win main just registers a class of the same type that the dialog class, and
then calls DialogBox. Then it exits. The return value is the return value of
the dialog procedure.
*/

int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS wc;

	memset(&wc,0,sizeof(wc));
	wc.lpfnWndProc = DefDlgProc;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hinst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = "default";
	RegisterClass(&wc);

	return DialogBox(hinst, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, (DLGPROC) DialogFunc);

}

/*
You should add your initialization code here. This function will be called
when the dialog box receives the WM_INITDIALOG message.
*/
static int InitializeApp(HWND hDlg,WPARAM wParam, LPARAM lParam)
{
	return 1;
}

/*
This is the main function for the dialog. It handles all messages. Do what your
application needs to do here.
*/
static BOOL CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	/* This message means the dialog is started but not yet visible.
	   Do All initializations here 
        */
	case WM_INITDIALOG:
		InitializeApp(hwndDlg,wParam,lParam);
		return TRUE;
	/* By default, IDOK means close this dialog returning 1, IDCANCEL means
           close this dialog returning zero
        */
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
				EndDialog(hwndDlg,1);
				return 1;
			case IDCANCEL:
				EndDialog(hwndDlg,0);
				return 1;
		}
		break;
        /* By default, WM_CLOSE is equivalent to CANCEL */
	case WM_CLOSE:
		EndDialog(hwndDlg,0);
		return TRUE;
	
	}
	return FALSE;
}
