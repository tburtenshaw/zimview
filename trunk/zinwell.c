#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "zinwellres.h"
#include "sqsh.h"
#include "adler.h"
#include "md5.h"
#include "zimview.h"
#include "clipboard.h"
#include "dialogs.h"

int isFocused=0;
int showCaret=0;
int mouseBeenPushed=0;
int draggingBlock=0;
int moveIndicatorLocation=-1;

int caretedBlock=0; //the block that has caret
int startRangeSelectedBlock=0; //where to start a range selection from (i.e. when shift is held)
int topDisplayBlock=0;
int numberDisplayedBlocks=0;		//used for calculating what to draw
int numberFullyDisplayedBlocks=0;	//used for calculating scrolling (a half block displayed should not be counted, and it should include the highest number that fits)
RECT paintSelectRects[255];

struct internalZimStructure pZim;	//The main zim file structure

HINSTANCE hInst;		// Instance handle
HWND hwndMain;		//Main window handle

HWND hwndToolBar;
HWND hwndStatusbar;
HWND hwndScrollbar;
int scrollbarWidth;

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
		//ChecksumAdler32B(adlerhold, buffer, lengthtoread);
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

void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
    SendMessage(hwndStatusbar, SB_SETTEXT, partNumber | displayFlags, (LPARAM)lpszStatusString);
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
    SendMessage(hwndStatusbar, SB_SETPARTS, nrOfParts, (LPARAM)(LPINT)ptArray);

    UpdateStatusBar("Ready", 0, 0);
	return;
}


static BOOL CreateStatusBar(HWND hwndParent,char *initialText, int nrOfParts)
{
	hwndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER|SBARS_SIZEGRIP, initialText, hwndParent, IDM_STATUSBAR);
    if(hwndStatusbar)	{
        InitializeStatusBar(hwndParent,nrOfParts);
        return TRUE;
    }

    return FALSE;
}

int GetFilename(char *buffer,int buflen)
{
	char tmpfilter[255];
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


void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	INT_PTR nResult;

	char filename[MAX_PATH];
	int i;
	int result;

	switch(id) {
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hwndMain, AboutDlg);
			break;
		case IDM_PROPERTIES:
			if (pZim.displayFilename[0]) {
				DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PROPERTIES), hwndMain, PropertiesDlg, (long)&pZim);
			}
			else {
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hwndMain, AboutDlg);
			}
			break;
		case IDM_BLOCKEXPORT:
			if (pZim.displayFilename[0]) {
				DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BLOCKEXPORT), hwndMain, BlockExportDlg, (long)&pZim);
			}
			break;
		case IDM_BLOCKIMPORT:
			nResult=DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BLOCKIMPORT), hwndMain, BlockImportDlg, (long)&pZim);
			if (nResult)
				InvalidateRect(hwndMain, NULL, FALSE);
			EnableToolbarButtons(hwndToolBar, &pZim);
			break;
		case IDM_CUSTOMERNUMBER:
			if (pZim.displayFilename[0]) {
				nResult=DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CHANGECUSTOMERNUMBER), hwndMain, ChangeCustomerNumberDlg, (long)&pZim);
				if (nResult)
					InvalidateRect(hwndMain, NULL, FALSE);
			}
			break;
		case IDM_BLOCKFIXCHECKSUMS:
			if (pZim.displayFilename[0]) {
				CalculateOffsetForWriting(&pZim);
				InvalidateRect(hwndMain, NULL, FALSE);
			}
			break;
		case IDM_BLOCKCREATEBOXIBLOCK:
			nResult=DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BOXIBLOCK), hwndMain, BlockCreateBoxiDlg, (long)&pZim);
			if (nResult)
				InvalidateRect(hwndMain, NULL, FALSE);
			EnableToolbarButtons(hwndToolBar, &pZim);
			break;
		case IDM_BLOCKCREATEVERIBLOCK:
			nResult=DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_VERIBLOCK), hwndMain, BlockCreateVeriDlg, (long)&pZim);
			if (nResult)
				InvalidateRect(hwndMain, NULL, FALSE);
			EnableToolbarButtons(hwndToolBar, &pZim);
			break;
		case IDM_OPEN:
			if (GetFilename(filename,sizeof(filename))) {
				if (pZim.displayFilename[0]) {
					if (MessageBox(hwnd, "You already have a file open. Are you sure you want to open a new one?", "ZimView", MB_OKCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2)==IDCANCEL)	{
						return;
					}
					else {
						CloseZimFile(&pZim);	SetWindowText(hwnd, "ZimView");
					}
				}
			OpenZimFile(hwnd, &pZim, filename); //the opens the file, and loads the zim while checking for errors
			UpdateWindow(hwnd);
			ScrollUpdate(hwnd, &pZim);
			EnableToolbarButtons(hwndToolBar, &pZim);
			}
			break;
		case IDM_SAVE:
			MessageBox(hwnd, "At the moment you have to Save As. I don't want anyone overwriting their original files.", "File not saved", MB_OK|MB_ICONEXCLAMATION);
			break;
		case IDM_SAVEAS:
			result = SaveAsZim(hwnd, &pZim);
			if (result==WRITESAVEZIM_ERR_NOTEMP)
				MessageBox(hwnd, "Could not open temporary file.", "File not saved", MB_OK|MB_ICONEXCLAMATION);
			if (result==WRITESAVEZIM_ERR_BLOCKERR)
				MessageBox(hwnd, "Error writing block to file.", "File not saved", MB_OK|MB_ICONEXCLAMATION);
			break;
		case IDM_REVERT:
			if (pZim.displayFilename[0]) {
				sprintf(&filename[0], "%s", pZim.displayFilename);
				CloseZimFile(&pZim);
				OpenZimFile(hwnd, &pZim, filename);
				InvalidateRect(hwnd, NULL, FALSE);
				UpdateWindow(hwnd);
			}
			break;
		case IDM_CLOSE:
			if	(HasFileBeenAltered(&pZim))	{
				if (MessageBox(hwnd, "Closing will lose all changes. Are you sure you want to close this file?", "Close", MB_YESNO|MB_ICONQUESTION)==IDNO)
					return;
			}
			CloseZimFile(&pZim);
			SetWindowText(hwnd, "ZimView");
			EnableToolbarButtons(hwndToolBar, &pZim);
			ScrollUpdate(hwnd, &pZim);
			InvalidateRect (hwnd, NULL, FALSE);
			break;
		case IDM_NEW:
			if (pZim.displayFilename[0]==0)	{
				ActivateZimFile(&pZim);
				EnableToolbarButtons(hwndToolBar, &pZim);
				InvalidateRect(hwnd, NULL, FALSE);
			} else	{
				if(MessageBox(hwnd, "Starting a new file will lose all changes on your open file. Do you want to start a new file?", "New", MB_YESNO|MB_ICONQUESTION)==IDYES)	{
					CloseZimFile(&pZim);
					ActivateZimFile(&pZim);
					EnableToolbarButtons(hwndToolBar, &pZim);
					ScrollUpdate(hwnd, &pZim);
					InvalidateRect(hwnd, NULL, FALSE);
				}
			}
			break;
		case IDM_SELECTALL:
			SelectAllBlocks(hwnd, &pZim);
			break;
		case IDM_EDITCOPY:
			EditCopySelected(hwnd, &pZim, FALSE);
			EnableToolbarButtons(hwndToolBar, &pZim);
			break;
		case IDM_EDITCUT:
			EditCopySelected(hwnd, &pZim, TRUE);
			RedrawSelectedBlocks(hwnd, &pZim);
			EnableToolbarButtons(hwndToolBar, &pZim);
			break;
		case IDM_EDITPASTE:
			EditPaste(hwnd, &pZim);
			EnableToolbarButtons(hwndToolBar, &pZim);
			ScrollUpdate(hwnd, &pZim);
			InvalidateRect(hwnd, NULL, FALSE);
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
			ScrollUpdate(hwnd, &pZim);
			EnableToolbarButtons(hwndToolBar, &pZim);
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
		SendMessage(hwndStatusbar,msg,wParam,lParam);
		SendMessage(hwndToolBar,msg,wParam,lParam);
		SendMessage(hwndScrollbar,msg,wParam,lParam);
		InitializeStatusBar(hwndStatusbar,1);
		ScrollUpdate(hwnd, &pZim);
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
		EnableToolbarButtons(hwndToolBar, &pZim);
	    InitMenu((HMENU) wParam, &pZim);
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
		if (wParam==VK_PRIOR) caretedBlock-=numberFullyDisplayedBlocks; //page up
		if (wParam==VK_NEXT) caretedBlock+=numberFullyDisplayedBlocks; //page down

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

		if (caretedBlock<topDisplayBlock)	{
			topDisplayBlock=caretedBlock;
			ScrollUpdate(hwnd, &pZim);
			InvalidateRect(hwnd, NULL, FALSE);
		}

		if (caretedBlock>=topDisplayBlock+numberFullyDisplayedBlocks)	{	//we need another variable with FULLY displayed block
			topDisplayBlock=max(0, caretedBlock-numberFullyDisplayedBlocks+1);
			ScrollUpdate(hwnd, &pZim);
			InvalidateRect(hwnd, NULL, FALSE);
		}


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
		EnableToolbarButtons(hwndToolBar, &pZim);
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
		EnableToolbarButtons(hwndToolBar, &pZim);
		break;
	case WM_LBUTTONDBLCLK:
		if (pZim.displayFilename[0]) {
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PROPERTIES), hwndMain, PropertiesDlg, (long)&pZim);
		}
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
	case WM_VSCROLL:
		return HandleVScroll(hwnd, &pZim, wParam);
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
	return 0;
}

HWND CreateScrollbar(HWND hwndParent)
{
	HWND hwndSB;
	RECT scrollbarRect;

	hwndSB = CreateWindowEx(0, WC_SCROLLBAR, (LPSTR) NULL,
    WS_CHILD | SBS_VERT|SBS_LEFTALIGN,         // scroll bar styles
    20,                           // horizontal position
    70,                           // vertical position
    CW_USEDEFAULT,                         // width of the scroll bar
    100,               // default height
    hwndParent,                   // handle to main window
    (HMENU) NULL, hInst, (LPVOID) NULL);


	GetClientRect(hwndSB, &scrollbarRect);
	scrollbarWidth=scrollbarRect.right;

	ShowWindow(hwndSB, SW_SHOW);

	return hwndSB;
}


#define NUM_TOOLBAR_BUTTONS		11
HWND CreateToolbar(HWND hwndParent)
{
	HWND hwndTB;
	TBADDBITMAP tbab;
	TBBUTTON tbb[NUM_TOOLBAR_BUTTONS];

	COLORMAP colorMap;
	HBITMAP hbm;

	int standardToolbarIndex;
	int myToolbarIndex[2];

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;

	hInst = hInstance;
	if (!InitApplication())
		return 0;
	hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));

	hwndMain = CreateWindow("ZimViewWndClass","ZimView",
		WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,
		CW_USEDEFAULT,0,CW_USEDEFAULT,0, NULL, NULL, hInst, NULL);

	if (hwndMain == (HWND)0)
		return 0;

	CreateStatusBar(hwndMain,"Ready",1);
	hwndToolBar = CreateToolbar(hwndMain);
	ShowWindow(hwndMain,SW_SHOW);

	hwndScrollbar=CreateScrollbar(hwndMain);

	if (strlen(lpCmdLine)>4)	{
 		OpenZimFile(hwndMain, &pZim, lpCmdLine);
	}
	EnableToolbarButtons(hwndToolBar, &pZim);
	ScrollUpdate(hwndMain, &pZim);

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

	//Check that the number of blocks isn't higher than the defined maximum number of blocks
	if (LoadedZim->wNumBlocks>MAXBLOCKS)	{
		return LOADZIM_ERR_NUMBERABOVEMAX;
	}

	//Allocate some space for the array
	LoadedZim->dwBlockStartLocArray=malloc(LoadedZim->wNumBlocks * sizeof(DWORD));
	if (LoadedZim->dwBlockStartLocArray==NULL)
		return LOADZIM_ERR_MEMORYPROBLEM;
	memset(LoadedZim->dwBlockStartLocArray, 0, LoadedZim->wNumBlocks * sizeof(DWORD)); //blank the array

	//Read block offsets and load them into internal structure, check for obvious errors
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

	LoadedZim->first=NULL;
	oldBlockStruct=LoadedZim->first;
	//Go through each block, and add it to the linked list
	for (tempWord=0;tempWord<LoadedZim->wNumBlocks; tempWord++) {
		//Read the block header
		fseek(LoadedZim->pZimFile, LoadedZim->dwBlockStartLocArray[tempWord], SEEK_SET);
		fread(&blockHeader, sizeof(blockHeader), 1, LoadedZim->pZimFile);

		// NOTE IN BOTH THESE ERRORS, I NEED TO free the previously malloc'd structures
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
		tempBlockStruct->dwSourceOffset =	LoadedZim->dwBlockStartLocArray[tempWord];
		tempBlockStruct->dwDestStartLoc =	tempBlockStruct->dwSourceOffset;
		tempBlockStruct->flags|=BSFLAG_SOURCECONTAINSHEADER;
		sprintf(tempBlockStruct->sourceFilename, "%s", LoadedZim->displayFilename);

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
		AdlerOnFile(LoadedZim->pZimFile, &adlerhold, tempBlockStruct->dwSourceOffset+sizeof(blockHeader), tempBlockStruct->dwDataLength);
		if ((adlerhold.a==0xDE) && (adlerhold.b==0xAD) && (1==0)) return LOADZIM_ERR_MEMORYPROBLEM;
		tempBlockStruct->dwRealChecksum=AdlerOnFile(LoadedZim->pZimFile, &adlerhold, tempBlockStruct->dwSourceOffset, sizeof(blockHeader)-sizeof(blockHeader.dwChecksum));
		if ((adlerhold.a==0xDE) && (adlerhold.b==0xAD) && (tempBlockStruct->dwRealChecksum==0)) return LOADZIM_ERR_MEMORYPROBLEM;

		//Calculate the md5 of the block
		cvs_MD5Init(&MD5context);
		MD5OnFile(LoadedZim->pZimFile, &MD5context, tempBlockStruct->dwSourceOffset+sizeof(blockHeader), tempBlockStruct->dwDataLength);
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
			case BID_DWORD_MACA:
				if (blockidDword==BID_DWORD_ROOT) tempBlockStruct->typeOfBlock=BTYPE_ROOT;
				if (blockidDword==BID_DWORD_CODE) tempBlockStruct->typeOfBlock=BTYPE_CODE;
				if (blockidDword==BID_DWORD_KERN) tempBlockStruct->typeOfBlock=BTYPE_KERN;
				if (blockidDword==BID_DWORD_LOAD) tempBlockStruct->typeOfBlock=BTYPE_LOAD;
				if (blockidDword==BID_DWORD_NVRM) tempBlockStruct->typeOfBlock=BTYPE_NVRM;
				if (blockidDword==BID_DWORD_MACA) tempBlockStruct->typeOfBlock=BTYPE_MACA;
				tempBlockStruct->ptrFurtherBlockDetail=ReadUsualBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwSourceOffset + sizeof(blockHeader));
				tempBlockStruct->flags|=BSFLAG_EXTERNFILE;
				sprintf(tempBlockStruct->sourceFilename, "%s", LoadedZim->displayFilename);
				break;
			case BID_DWORD_VERI:
				tempBlockStruct->typeOfBlock=BTYPE_VERI;
				tempBlockStruct->ptrFurtherBlockDetail=ReadVeriBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwSourceOffset + sizeof(blockHeader));
				break;
			case BID_DWORD_BOXI:
				tempBlockStruct->typeOfBlock=BTYPE_BOXI;
				tempBlockStruct->ptrFurtherBlockDetail=ReadBoxiBlock(tempBlockStruct, LoadedZim->pZimFile, tempBlockStruct->dwSourceOffset + sizeof(blockHeader));
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
		case BID_DWORD_MACA:
			tempUsualStruct = ptrBlockStruct->ptrFurtherBlockDetail;
			if (tempUsualStruct->sqshHeader) {free(tempUsualStruct->sqshHeader);}
			if (tempUsualStruct->gzipHeader) {free(tempUsualStruct->gzipHeader);}
			if (tempUsualStruct->maclistHeader) {free(tempUsualStruct->maclistHeader);}
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


#define MOVEINDICATOR_WIDTH 6
#define EDGE_MARGIN 4
#define ICON_SIZE 48
#define TOP_MARGIN 16
#define HEADERBOTTOM_MARGIN 8

int PaintWindow(HWND hwnd) {
	HDC hdc;
	PAINTSTRUCT psPaint;
	RECT clientRect;
	HRGN clipRgn;

	RECT headerRect; //the rectangle the zim header information is displayed in
	RECT footerRect; //the white space under all the blocks
	RECT lineRect;   //a rect for each line when using exttextout
	RECT infobarRect;

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
	BLOCK_STRUCTURE *firstDisplayBlockStruct;
	BOXI_STRUCTURE *tempBoxiStruct;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;

	int flaglzma;
	int flagendian;

	hdc = BeginPaint(hwnd, &psPaint);
	GetClientRect(hwnd, &clientRect);			//the size of the whole window draw area
	GetClientRect(hwndToolBar, &infobarRect);	//from the bottom of the toolbar
	clientRect.top=infobarRect.bottom;

	GetClientRect(hwndStatusbar, &infobarRect);
	clientRect.bottom-=infobarRect.bottom;	//to the top of the statusbar


	GetClientRect(hwndScrollbar, &infobarRect);
	clientRect.right-=infobarRect.right;

	//First of all we need to calculate the space the header and each block will occupy
	heightHeader=112+HEADERBOTTOM_MARGIN; //includes padding above and below

	headerRect.left=clientRect.left;
	headerRect.right=clientRect.right;
	headerRect.top=clientRect.top;
	headerRect.bottom=headerRect.top+heightHeader;

	y=headerRect.bottom;
	firstDisplayBlockStruct=GetBlockNumber(&pZim, topDisplayBlock);

	tempBlockStruct=firstDisplayBlockStruct;
	numberFullyDisplayedBlocks=0;
	for (i=0;(i<pZim.wNumBlocks-topDisplayBlock)&&(i<255)&&(tempBlockStruct);i++) {
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


	y=headerRect.top+TOP_MARGIN;

	if (!pZim.displayFilename[0])	{ 	//Displayed if no file open
		ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &clientRect, "Open a .zim file above, or start adding blocks.", 47, NULL);
		caretedBlock=0;
		topDisplayBlock=0;
	}
	else {

		//Display the header information
		if (RectVisible(hdc, &headerRect)) {
			//draw line by line, the left and right edges don't change
			lineRect.left=headerRect.left;
			lineRect.right=headerRect.right;

			sprintf(tempString, "%s", pZim.displayFilenameNoPath);

			lineRect.top=headerRect.top; //this is the first line of text, so should include top margin
			lineRect.bottom=y+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;	//32
			sprintf(tempString, "%s", pZim.displayFilename);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;	//48
			sprintf(tempString, "Checksum: %08X", pZim.dwChecksum);
			if (pZim.dwChecksum!=pZim.dwRealChecksum)
				sprintf(tempString, "Checksum: %08X (differs from calculated checksum %08X)", pZim.dwChecksum, pZim.dwRealChecksum);

			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;	//64
			sprintf(tempString, "Customer number: %i (0x%x)", pZim.wCustomerNumber, pZim.wCustomerNumber);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;	//80
			sprintf(tempString, "Number of blocks: %i", pZim.wNumBlocks);
			lineRect.top=y;
			lineRect.bottom=lineRect.top+16;
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

			y+=16;	//96+16=112
			sprintf(tempString, "File length: %i bytes", pZim.dwTotalFileLen);
			if (pZim.dwTotalFileLen!=pZim.dwRealFileLen)
	 			sprintf(tempString, "File length: %i bytes (NB: Actual file length is %i bytes)", pZim.dwTotalFileLen, pZim.dwRealFileLen);
			lineRect.top=y;
			lineRect.bottom=headerRect.bottom; //this is the last line, so should include the small margin between header and blocks
			ExtTextOut(hdc, EDGE_MARGIN, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);
			}

//List the blocks
			tempBlockStruct = firstDisplayBlockStruct;
			numberDisplayedBlocks=0;
			for (i=0;(i<pZim.wNumBlocks-topDisplayBlock)&&(paintSelectRects[i].top<clientRect.bottom)&&(tempBlockStruct);i++) {
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
					else if (tempBlockStruct->flags & BSFLAG_HASCHANGED)
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
						case BTYPE_MACA:
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
							if (tempBlockStruct->typeOfBlock==BTYPE_MACA)
								intIconResource=IDI_MACA;

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
								if (counterVeriPart<3)	{
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
								else if	((counterVeriPart>2) && (counterVeriPart<5))	{
									while (counterVeriPart) {
										y+=16;
										sprintf(tempString, "Id: %s, Ver: %i.%i.(%i).%i; ",tempVeriStruct->displayblockname, tempVeriStruct->veriFileData.version_a,tempVeriStruct->veriFileData.version_b,tempVeriStruct->veriFileData.version_c,tempVeriStruct->veriFileData.version_d);
										lineRect.top=y;
										lineRect.bottom=y+16;

										counterVeriPart--;
										tempVeriStruct=tempVeriStruct->nextStructure;
										if (counterVeriPart)	{
											sprintf(tempString+strlen(tempString), "Id: %s, Ver: %i.%i.(%i).%i;", tempVeriStruct->displayblockname, tempVeriStruct->veriFileData.version_a,tempVeriStruct->veriFileData.version_b,tempVeriStruct->veriFileData.version_c,tempVeriStruct->veriFileData.version_d);
											counterVeriPart--;
											tempVeriStruct=tempVeriStruct->nextStructure;
										}
										lineRect.bottom=paintSelectRects[i].bottom; //this is last veri structure, draw to bottom of block
										ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);

									}
								}
								else if ((counterVeriPart>4))	{
									y+=16;
									sprintf(tempString, "%s; ", tempVeriStruct->displayblockname);
									lineRect.top=y;
									lineRect.bottom=y+16;
									counterVeriPart--;
									tempVeriStruct=tempVeriStruct->nextStructure;
									while (counterVeriPart)	{
										sprintf(tempString+strlen(tempString), "%s; ", tempVeriStruct->displayblockname);
										counterVeriPart--;
										tempVeriStruct=tempVeriStruct->nextStructure;
									}
										lineRect.bottom=paintSelectRects[i].bottom; //this is last veri structure, draw to bottom of block
										ExtTextOut(hdc, EDGE_MARGIN+EDGE_MARGIN+ICON_SIZE, y, ETO_OPAQUE, &lineRect, tempString, strlen(tempString), NULL);



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


void MD5HexString(char * outputString, char * MD5array)
{
	sprintf(outputString,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", MD5array[0]&0xFF,MD5array[1]&0xFF,MD5array[2]&0xFF,MD5array[3]&0xFF,MD5array[4]&0xFF,MD5array[5]&0xFF,MD5array[6]&0xFF,MD5array[7]&0xFF,MD5array[8]&0xFF,MD5array[9]&0xFF,MD5array[10]&0xFF,MD5array[11]&0xFF,MD5array[12]&0xFF,MD5array[13]&0xFF,MD5array[14]&0xFF,MD5array[15]&0xFF);
	return;
}

int MD5StringToArray(char * MD5array, char * inputString)
{
	char hexnumber[3];
	int i;

	hexnumber[2]=0;	//zero terminator
	i=0;
	while (inputString[i] && i<32) {
		hexnumber[0]=inputString[i];
		hexnumber[1]=inputString[i+1];

		MD5array[i/2]=strtoul(&hexnumber[0], NULL, 16);
		i+=2;
	}

	return i;
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

int WriteBlockToFile(ZIM_STRUCTURE *LoadedZim, BLOCK_STRUCTURE *Block, FILE *exportFile, int includeHeader)
{
	DWORD exportOffset;
	DWORD exportLength;
	//char *exportData;
	struct sBlockHeader blockHeader;
	BOXI_STRUCTURE *tempBoxiStruct;
	VERI_STRUCTURE *tempVeriStruct;
	FILE *usualSourceFile;
	char *usualData;
	int counterVeriPart;

	exportOffset=Block->dwSourceOffset;
	exportLength=Block->dwDataLength;
	if (!includeHeader)
		exportOffset=exportOffset+sizeof(struct sBlockHeader);
	else
		exportLength=exportLength+sizeof(struct sBlockHeader);

	//Write the header
	if (includeHeader) {
		GenerateBlockHeader(&blockHeader, Block->dwDataLength, Block->dwRealChecksum, Block->name);
		fwrite(&blockHeader, sizeof(struct sBlockHeader), 1, exportFile);
	}

	switch (Block->typeOfBlock)	{

		case BTYPE_BOXI:
			tempBoxiStruct=Block->ptrFurtherBlockDetail;
			fwrite(&tempBoxiStruct->boxiFileData, sizeof(BOXIBLOCK_STRUCTURE), 1, exportFile);
			break;

		case BTYPE_VERI:
			tempVeriStruct=Block->ptrFurtherBlockDetail;
			counterVeriPart=tempVeriStruct->numberVeriObjects;
			while (counterVeriPart) {
				if (tempVeriStruct) {
					fwrite(&tempVeriStruct->veriFileData, sizeof(VERIBLOCK_STRUCTURE), 1, exportFile);
				}
				counterVeriPart--;
				if (counterVeriPart) tempVeriStruct=tempVeriStruct->nextStructure;
			}
			break;

		case BTYPE_CODE:
		case BTYPE_KERN:
		case BTYPE_ROOT:
		case BTYPE_LOAD:
		case BTYPE_NVRM:
		case BTYPE_MACA: //does this need default??????
		default:
			if (!Block->sourceFilename)
				return WBTF_ERR_NOSOURCE;

			usualSourceFile=fopen(Block->sourceFilename, "rb");
			if (!usualSourceFile)
				return WBTF_ERR_FILENOTFOUND;	//error

			usualData=malloc(Block->dwDataLength);
			if (!usualData)
				return WBTF_ERR_MEMORYALLOCATION;

			fseek(usualSourceFile, (Block->flags&BSFLAG_SOURCECONTAINSHEADER) ? Block->dwSourceOffset+sizeof(struct sBlockHeader):Block->dwSourceOffset, SEEK_SET);
			fread(usualData, Block->dwDataLength, 1, usualSourceFile);
				fclose(usualSourceFile);
			fwrite(usualData, Block->dwDataLength, 1, exportFile);
			free(usualData);
			break;
	}


	return WBTF_ERR_SUCCESS;
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
		newBlock->flags=BSFLAG_HASCHANGED; //we'll set that its in memory
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

	newBlock->flags=BSFLAG_HASCHANGED; //we'll set that its in memory
	LoadedZim->wNumBlocks++;

	return newBlock;
}

void *ReadUsualBlock(BLOCK_STRUCTURE *Block, FILE *fileToRead, DWORD start)
{
	USUAL_STRUCTURE *tempUsualStruct;
	WORD tempXLEN;	//used by gzip
	char tempchar;
	int tempLengthOfString;
	int tempNumberOfCRsOrLFs;
	int tempInt;

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

	tempUsualStruct->maclistHeader=NULL;
	tempUsualStruct->sqshHeader=NULL;
	tempUsualStruct->gzipHeader=NULL;
	if (	(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_LZMA)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_LZMA_SWAP)
		  ||(tempUsualStruct->magicnumber==SQUASHFS_MAGIC_SWAP))
		{ //if there's a squashfs file here
	 		tempUsualStruct->sqshHeader=malloc(sizeof(SQUASHFS_SUPER_BLOCK));
			fseek(fileToRead, start, SEEK_SET);
			//fread(tempUsualStruct->sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK), 1, fileToRead);
			//Sadly the compiler aligns at 16 bit intervals, and the squashfs has three bytes in a row
			fread(&tempUsualStruct->sqshHeader->s_magic, sizeof(tempUsualStruct->sqshHeader->s_magic), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->inodes, sizeof(tempUsualStruct->sqshHeader->inodes), 1, fileToRead);

			fread(&tempUsualStruct->sqshHeader->bytes_used_2, sizeof(tempUsualStruct->sqshHeader->bytes_used_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->uid_start_2, sizeof(tempUsualStruct->sqshHeader->uid_start_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->guid_start_2, sizeof(tempUsualStruct->sqshHeader->guid_start_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->inode_table_start_2, sizeof(tempUsualStruct->sqshHeader->inode_table_start_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->directory_table_start_2, sizeof(tempUsualStruct->sqshHeader->directory_table_start_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->s_major, 2, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->s_minor, 2, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->block_size_1, 2, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->block_log, 2, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->flags, 1, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->no_uids, 1, 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->no_guids, 1, 1, fileToRead);

			fread(&tempUsualStruct->sqshHeader->mkfs_time, sizeof(tempUsualStruct->sqshHeader->mkfs_time), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->root_inode, sizeof(tempUsualStruct->sqshHeader->root_inode), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->block_size, sizeof(tempUsualStruct->sqshHeader->block_size), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->fragments, sizeof(tempUsualStruct->sqshHeader->fragments), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->fragment_table_start_2, sizeof(tempUsualStruct->sqshHeader->fragment_table_start_2), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->bytes_used, sizeof(tempUsualStruct->sqshHeader->bytes_used), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->uid_start, sizeof(tempUsualStruct->sqshHeader->uid_start), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->guid_start, sizeof(tempUsualStruct->sqshHeader->guid_start), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->inode_table_start, sizeof(tempUsualStruct->sqshHeader->inode_table_start), 1, fileToRead);

			fread(&tempUsualStruct->sqshHeader->directory_table_start, sizeof(tempUsualStruct->sqshHeader->directory_table_start), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->fragment_table_start, sizeof(tempUsualStruct->sqshHeader->fragment_table_start), 1, fileToRead);
			fread(&tempUsualStruct->sqshHeader->lookup_table_start, sizeof(tempUsualStruct->sqshHeader->lookup_table_start), 1, fileToRead);
		}

	else if (Block->typeOfBlock==BTYPE_MACA)	{
 		tempUsualStruct->maclistHeader=malloc(sizeof(MACLIST_HEADER_BLOCK));
		fseek(fileToRead, start, SEEK_SET);
		fread(&tempUsualStruct->maclistHeader->firstMAC,17, 1, fileToRead);
		tempUsualStruct->maclistHeader->firstMAC[17]=0; //ensure the string has null term

		tempLengthOfString=0;
		tempNumberOfCRsOrLFs=0;
		tempUsualStruct->maclistHeader->numberofMACs=0;
		tempInt=17;

		while ((tempNumberOfCRsOrLFs<3)&&(tempInt<Block->dwDataLength))	{

			fread(&tempchar,1,1,fileToRead);
			tempLengthOfString++;
			if ((tempchar==0x0a) || (tempchar==0x0d))	{
				tempNumberOfCRsOrLFs++;
			} else
				tempNumberOfCRsOrLFs=0;

			if (tempNumberOfCRsOrLFs==2)
				tempUsualStruct->maclistHeader->numberofMACs++;

			tempInt++;
		}



		}
	else if ((tempUsualStruct->magicnumber & 0xFFFF)==0x8b1f) {
		tempUsualStruct->gzipHeader=malloc(sizeof(GZIP_HEADER_BLOCK));
		fseek(fileToRead, start, SEEK_SET);

		//fread(tempUsualStruct->gzipHeader, sizeof(GZIP_HEADER_BLOCK), 1, fileToRead);
		//Same again, can't read all as a batch because of alignment issues.
		fread(&tempUsualStruct->gzipHeader->id1, sizeof(char), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->id2, sizeof(char), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->cm, sizeof(char), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->flg, sizeof(char), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->mtime, sizeof(DWORD), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->xfl, sizeof(char), 1, fileToRead);
		fread(&tempUsualStruct->gzipHeader->os, sizeof(char), 1, fileToRead);

		if (tempUsualStruct->gzipHeader->flg & GZIP_FEXTRA)	{	//skip the extra block if it exists
			fread(&tempXLEN, sizeof(WORD),1, fileToRead);
			fseek(fileToRead, tempXLEN, SEEK_CUR);
		}

		tempUsualStruct->gzipHeader->filename[0]=0;
		if (tempUsualStruct->gzipHeader->flg & GZIP_FNAME)	{	//read the filename
			//need to read null-terminated string
			auto	char c;
			auto	int i;

			i=0;
			do	{
				c=fgetc(fileToRead);
				tempUsualStruct->gzipHeader->filename[i]=c;
				i++;
			}	while (c && i<MAX_PATH-1);
			tempUsualStruct->gzipHeader->filename[i]=0;
		}
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

int SaveAsZim(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
	FILE *SaveFile;

	OPENFILENAME ofnExportTo;
	char filename[MAX_PATH];
	char temporarypath[MAX_PATH];
	char temporaryfilename[MAX_PATH];
	char tempString[255];
	BLOCK_STRUCTURE *tempBlock;
	WORD tempWord;

	int result;

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
			if (MessageBox(hwnd, "This file already exists. Are you sure you want to replace it?", "Saves As", MB_YESNO|MB_ICONEXCLAMATION)==IDNO)	{
				fclose(SaveFile);
				return 0;
			}
			fclose(SaveFile);
		}


		//we need to write to temp file (so we're not trying to read and write to same file)
		GetTempPath(MAX_PATH, &temporarypath[0]);

		sprintf(&temporaryfilename[0], "%s%s%s", &temporarypath[0], strrchr(filename,92)+1 ,".tmp");

		SaveFile=fopen(temporaryfilename, "w+b");
		if (!SaveFile)	//couldn't open temp file
			return WRITESAVEZIM_ERR_NOTEMP;
		result = WriteZimFile(LoadedZim, SaveFile);
		fclose(SaveFile);

		if (result)	{//error when writing temp file
			DeleteFile(&temporaryfilename[0]);
			return WRITESAVEZIM_ERR_BLOCKERR;
		}

		if (LoadedZim->pZimFile)
			fclose(LoadedZim->pZimFile);

		//then move temp file to filename
		MoveFileEx(&temporaryfilename[0], &filename[0], MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING);

		//Update the window
		sprintf(LoadedZim->displayFilename, "%s", ofnExportTo.lpstrFile);
		LoadedZim->displayFilenameNoPath=strrchr(LoadedZim->displayFilename,92)+1;

		sprintf(tempString, "ZimView - %s",LoadedZim->displayFilenameNoPath);
		SetWindowText(hwnd, tempString);
		InvalidateRect (hwnd, NULL, TRUE);

		tempBlock=LoadedZim->first;
		for (tempWord=0; tempWord<LoadedZim->wNumBlocks; tempWord++) {

			//if it's referenced in the old version, then we really can't keep it.
			if ((tempBlock->flags & BSFLAG_DONTWRITE) && (!(tempBlock->flags & BSFLAG_HASCHANGED))) {
				DeleteBlock(LoadedZim, tempWord);
				tempWord--;
			}

			tempBlock->flags&=0xFE;

			sprintf(tempBlock->sourceFilename, "%s",filename);
			tempBlock->dwSourceOffset=tempBlock->dwDestStartLoc;
			tempBlock=tempBlock->next;
		}


		//Get the new/saved filename to be the one opened.
		LoadedZim->pZimFile = fopen(filename, "rb");
		fseek(LoadedZim->pZimFile, 0, SEEK_END);
		LoadedZim->dwRealFileLen=ftell(LoadedZim->pZimFile);
		LoadedZim->dwChecksum=LoadedZim->dwRealChecksum;

	}

	return WRITESAVEZIM_ERR_SUCCESS;
}

int WriteZimFile(ZIM_STRUCTURE *LoadedZim, FILE *outputZim)
{
	BLOCK_STRUCTURE *tempBlock;
	struct sZimHeader outputZimHeader;

	WORD tempWord;
	DWORD startLoc;
	DWORD adler;
	WORD wNumBlocks;

	int result;

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
			result = WriteBlockToFile(LoadedZim, tempBlock, outputZim, 1);
			if (result)
				return WRITESAVEZIM_ERR_BLOCKERR;
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

	return WRITESAVEZIM_ERR_SUCCESS;
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
		case BID_DWORD_MACA:
			tempUsualStruct = Block->ptrFurtherBlockDetail;
			if (tempUsualStruct->sqshHeader) {free(tempUsualStruct->sqshHeader);}
			if (tempUsualStruct->gzipHeader) {free(tempUsualStruct->gzipHeader);}
			if (tempUsualStruct->maclistHeader) {free(tempUsualStruct->maclistHeader);}
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
	//while (ptrCounterBlock)	{
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
	//while (ptrCounterBlock)	{
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

int RedrawSelectedBlocks(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *ptrCounterBlock;
	WORD tempWord;

	ptrCounterBlock=LoadedZim->first;
	//while (ptrCounterBlock)	{
	for (tempWord=0; (tempWord<LoadedZim->wNumBlocks && ptrCounterBlock); tempWord++) {
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

void EnableToolbarButtons(HWND hTB, ZIM_STRUCTURE *LoadedZim)
{
	SendMessage(hTB, TB_ENABLEBUTTON, IDM_NEW, MAKELONG(TRUE ,0));
	SendMessage(hTB, TB_ENABLEBUTTON, IDM_OPEN, MAKELONG(TRUE ,0));

	if (!LoadedZim->displayFilename[0])	{
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_SAVE, MAKELONG(FALSE ,0));
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_PROPERTIES, MAKELONG(FALSE ,0));
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_BLOCKEXPORT, MAKELONG(FALSE ,0));
	} else {
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_SAVE, MAKELONG(TRUE ,0));
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_PROPERTIES, MAKELONG(TRUE ,0));
		if (LoadedZim->wNumBlocks>0)
			SendMessage(hTB, TB_ENABLEBUTTON, IDM_BLOCKEXPORT, MAKELONG(TRUE ,0));
		else
			SendMessage(hTB, TB_ENABLEBUTTON, IDM_BLOCKEXPORT, MAKELONG(FALSE ,0));

	}

	if (SelectedCount(LoadedZim)>0)	{
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITCOPY, MAKELONG(TRUE ,0));
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITCUT, MAKELONG(TRUE ,0));
	} else	{
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITCOPY, MAKELONG(FALSE ,0));
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITCUT, MAKELONG(FALSE ,0));
	}


	if (IsClipboardFormatAvailable(uZimBlockFormat))	{
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITPASTE, MAKELONG(TRUE ,0));
	} else {
		SendMessage(hTB, TB_ENABLEBUTTON, IDM_EDITPASTE, MAKELONG(FALSE ,0));
	}

	return;
}

void WINAPI InitMenu(HMENU hmenu, ZIM_STRUCTURE *LoadedZim)
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
			case IDM_REVERT:
			case IDM_SAVE:
			case IDM_SAVEAS:
			case IDM_CLOSE:		//there must be an edited file open
				if (!LoadedZim->displayFilename[0]) //if the zim isn't loaded
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
				if (!LoadedZim->displayFilename[0]) { //if the zim isn't loaded
					fuFlags = MF_BYCOMMAND | MF_GRAYED;
				} else {
					if (SelectedCount(LoadedZim)>0)
						fuFlags = MF_BYCOMMAND | MF_ENABLED;
					else
						fuFlags = MF_BYCOMMAND | MF_GRAYED;
				}
    	        EnableMenuItem(hmenu, id, fuFlags);
				break;
			case IDM_SELECTALL:	//we need at least one block to be able to do these
			case IDM_BLOCKEXPORT:
			case IDM_BLOCKCREATEVERIBLOCK:
				if (LoadedZim->displayFilename[0] && (LoadedZim->wNumBlocks>0))
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
		if (topDisplayBlock>=LoadedZim->wNumBlocks-numberFullyDisplayedBlocks) topDisplayBlock=LoadedZim->wNumBlocks-numberFullyDisplayedBlocks;
	}
	if (nDelta>0) {
		topDisplayBlock--;
		if (topDisplayBlock<0) topDisplayBlock=0;
	}

	if (oldTopDisplayBlock!=topDisplayBlock)
		InvalidateRect(hwnd, NULL, FALSE);

	ScrollUpdate(hwnd, LoadedZim);

	return 0;
}

void ScrollUpdate(HWND hwnd, ZIM_STRUCTURE *LoadedZim)
{
    RECT clientRect;
	RECT toolbarRect;
	RECT statusbarRect;

	SCROLLINFO si = { sizeof(si) };


	GetClientRect(hwnd, &clientRect);
	GetClientRect(hwndToolBar, &toolbarRect);
	GetClientRect(hwndStatusbar, &statusbarRect);

	MoveWindow(hwndScrollbar, clientRect.right-scrollbarWidth, toolbarRect.bottom+3, scrollbarWidth, clientRect.bottom-toolbarRect.bottom-statusbarRect.bottom-3, TRUE);


    si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;

    si.nPos  = topDisplayBlock;         // scrollbar thumb position
    si.nPage = numberFullyDisplayedBlocks;        // number of lines in a page (i.e. rows of text in window)

	if (si.nPage==0)
		si.nPage=1;

    si.nMin  = 0;
    si.nMax  = LoadedZim->wNumBlocks-1;      // total number of lines in file (i.e. total scroll range)

    SetScrollInfo(hwndScrollbar, SB_CTL, &si, TRUE);

	return;
}

long HandleVScroll(HWND hwnd, ZIM_STRUCTURE *LoadedZim, WPARAM wParam)
{
	short nSBCode;
	short nScrollPos;

	nSBCode=LOWORD(wParam);

	switch (nSBCode)	{
		case SB_LINEDOWN:
			topDisplayBlock++;
			break;
		case SB_LINEUP:
			topDisplayBlock--;
			break;
		case SB_PAGEUP:
			topDisplayBlock-=numberFullyDisplayedBlocks;
			break;
		case SB_PAGEDOWN:
			topDisplayBlock+=numberFullyDisplayedBlocks;
			break;
		case SB_TOP:
			topDisplayBlock=0;
			break;
		case SB_BOTTOM:
			break;
		case SB_THUMBTRACK:
			nScrollPos = HIWORD(wParam);
			topDisplayBlock = nScrollPos;
			break;

	}

	if (topDisplayBlock<0) topDisplayBlock=0;
	if (topDisplayBlock>=LoadedZim->wNumBlocks-numberFullyDisplayedBlocks) topDisplayBlock=LoadedZim->wNumBlocks-numberFullyDisplayedBlocks;

	ScrollUpdate(hwnd, LoadedZim);
	InvalidateRect(hwnd, NULL, FALSE);
	return 0;
}

int HasFileBeenAltered(ZIM_STRUCTURE *LoadedZim)
{
	BLOCK_STRUCTURE *ptrCounterBlock;

	if (LoadedZim->displayFilename[0]==0)
		return 0;

	ptrCounterBlock=LoadedZim->first;
	while (ptrCounterBlock)	{
		if (ptrCounterBlock->flags & BSFLAG_HASCHANGED)
			return 1;

		if (ptrCounterBlock->dwChecksum!=ptrCounterBlock->dwRealChecksum)
			return 1;

		ptrCounterBlock=ptrCounterBlock->next;
	}

	return 0;
}
