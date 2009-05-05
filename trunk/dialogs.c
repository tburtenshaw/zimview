#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "zinwellres.h"
#include "sqsh.h"
#include "zimview.h"
#include "md5.h"
#include "dialogs.h"
#include "adler.h"

DLGTEMPLATE * WINAPI DoLockDlgRes(LPCSTR lpszResName)
{
    HRSRC hrsrc = FindResource(NULL, lpszResName, RT_DIALOG);
    HGLOBAL hglb = LoadResource(hInst, hrsrc);
    return (DLGTEMPLATE *) LockResource(hglb);
}


void PropertiesDlg_ChangeSelection(HWND hwnd)
{
	int iSel;
	DLGHDR_PROPERTIES *pHdr;

    pHdr = (DLGHDR_PROPERTIES *) GetWindowLong(hwnd, GWL_USERDATA);
	iSel = TabCtrl_GetCurSel(pHdr->hwndTab);

    if (pHdr->hwndDisplay != NULL)
		DestroyWindow(pHdr->hwndDisplay);

	pHdr->hwndDisplay = CreateDialogIndirect(hInst, pHdr->apRes[iSel], hwnd, ChildDlg);

	switch (pHdr->selectedBlock->typeOfBlock)	{
		case BTYPE_BOXI:
			FillBoxiDlgFromBoxiStruct(pHdr->hwndDisplay, pHdr->selectedBlock);
	}

	InvalidateRect(pHdr->hwndDisplay, NULL, TRUE);

	return;
}

BOOL _stdcall PropertiesDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DLGHDR_PROPERTIES *pHdr;
	TCITEM tie;
	HANDLE hTab;
	int nSel;
	BLOCK_STRUCTURE *selectedBlock;
	char tempString[255];

	ZIM_STRUCTURE *ZimToUse;

	switch(msg) {
		case WM_INITDIALOG:
			pHdr = (DLGHDR_PROPERTIES *) LocalAlloc(LPTR, sizeof(DLGHDR_PROPERTIES));

			if (pHdr==NULL) {
				MessageBox(hwnd, "There is insufficient memory to display Properties.", "Properties", MB_OK|MB_ICONEXCLAMATION);
				return 0;
			}


			ZimToUse=(ZIM_STRUCTURE *)lParam;
		    // Save a pointer to the DLGHDR structure.
		    SetWindowLong(hwnd, GWL_USERDATA, (LONG) pHdr);

			pHdr->LoadedZim=ZimToUse;

			nSel = SelectedCount(ZimToUse);
			SelectedFirst(ZimToUse, &selectedBlock);

			hTab=GetDlgItem(hwnd, IDC_PROPERTIESTAB);
			pHdr->hwndTab=hTab;

		    pHdr->apRes[0] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_MAIN));

		    tie.mask = TCIF_TEXT | TCIF_IMAGE;
		    tie.iImage = -1;
		    tie.pszText = "General";
			TabCtrl_InsertItem(hTab, 0, &tie);

			if (nSel==1)	{
				sprintf(&tempString[0], "%s Properties", selectedBlock->name);
				SetWindowText(hwnd, tempString);

		    	tie.pszText = selectedBlock->name;
				TabCtrl_InsertItem(hTab, 1, &tie);
				pHdr->selectedBlock=selectedBlock;
				switch (selectedBlock->typeOfBlock)	{
					case BTYPE_BOXI:
						pHdr->apRes[1] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_BOXI));
						break;
					case BTYPE_VERI:
						pHdr->apRes[1] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_VERI));
						break;
					default:
						pHdr->apRes[1] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_USUAL));
						break;
				}


			}
		    else if (nSel>1)	{
				sprintf(&tempString[0], "Multiple (%i blocks selected) Properties", nSel);
				SetWindowText(hwnd, tempString);

			}
			else {
				sprintf(&tempString[0], "%s Properties", ZimToUse->displayFilenameNoPath);
				SetWindowText(hwnd, tempString);
			}
			PropertiesDlg_ChangeSelection(hwnd);
			break;
		case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code)
            {
            case TCN_SELCHANGE:
				PropertiesDlg_ChangeSelection(hwnd);
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
					LocalFree(pHdr);
					EndDialog(hwnd,1);
					return 1;
				case IDAPPLY:
					return 1;
				case IDOK:
					LocalFree(pHdr);
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
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

BOOL _stdcall ChildDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
	}
	return 0;
}


BOOL _stdcall BlockCreateBoxiDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ZIM_STRUCTURE *ZimToUse;
	BLOCK_STRUCTURE *newBlock;

	switch(msg) {
		case WM_INITDIALOG:
		    SetWindowLong(hwnd, GWL_USERDATA, lParam);
			break;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			ZimToUse=(ZIM_STRUCTURE *)GetWindowLong(hwnd, GWL_USERDATA);
			switch (LOWORD(wParam)) {
				case IDOK:
					newBlock=NewBlock(ZimToUse);

					newBlock->typeOfBlock=BTYPE_BOXI;
					sprintf(newBlock->name, "BOXI");
					newBlock->dwDataLength= sizeof(BOXIBLOCK_STRUCTURE);

					newBlock->ptrFurtherBlockDetail=malloc(sizeof(BOXI_STRUCTURE));
					if (newBlock->ptrFurtherBlockDetail==NULL) {
						MessageBox(hwnd, "There is insufficient memory to create a new BOXI block.", "Create BOXI block", MB_OK|MB_ICONEXCLAMATION);
						return 0;
					}
					FillBoxiStructFromBoxiDlg(hwnd, newBlock);
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

void FillBoxiDlgFromBoxiStruct(HWND hwnd, BLOCK_STRUCTURE *boxiBlock)
{
	BOXI_STRUCTURE *tempBoxiStruct;
	char buffer[255];

	tempBoxiStruct=boxiBlock->ptrFurtherBlockDetail;

	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.uiOUI);
	SetDlgItemText(hwnd, IDC_BOXIOUI, &buffer[0]);
	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.uiStarterImageSize);
	SetDlgItemText(hwnd, IDC_BOXISTARTERIMAGESIZE, &buffer[0]);
	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.wHwVersion);
	SetDlgItemText(hwnd, IDC_BOXIHWV, &buffer[0]);
	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.wSwVersion);
	SetDlgItemText(hwnd, IDC_BOXISWV, &buffer[0]);
	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.wHwModel);
	SetDlgItemText(hwnd, IDC_BOXIHWM, &buffer[0]);
	sprintf(buffer, "0x%x", tempBoxiStruct->boxiFileData.wSwModel);
	SetDlgItemText(hwnd, IDC_BOXISWM, &buffer[0]);
	MD5HexString(&buffer[0], tempBoxiStruct->boxiFileData.abStarterMD5Digest);
	SetDlgItemText(hwnd, IDC_BOXISTARTERMD5, &buffer[0]);
	return;
}

void FillBoxiStructFromBoxiDlg(HWND hwnd, BLOCK_STRUCTURE *boxiBlock)
{
	BOXI_STRUCTURE *tempBoxiStruct;
	char buffer[255];

	ADLER_STRUCTURE adlerholder;
	struct cvs_MD5Context MD5context;
	struct sBlockHeader blockHeader;

	tempBoxiStruct=boxiBlock->ptrFurtherBlockDetail;
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
	cvs_MD5Final (&boxiBlock->md5, &MD5context);
	//Adler32 on the data then block header
	memset(&adlerholder, 0, sizeof(adlerholder));
	ChecksumAdler32(&adlerholder, &(tempBoxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE)); //the boxi we loaded
	GenerateBlockHeader(&blockHeader, boxiBlock->dwDataLength, 0, "BOXI");
	boxiBlock->blockSignature[0]=blockHeader.blockSignature[0];
	boxiBlock->blockSignature[1]=blockHeader.blockSignature[1];
	boxiBlock->blockSignature[2]=blockHeader.blockSignature[2];
	boxiBlock->blockSignature[3]=blockHeader.blockSignature[3];

	boxiBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum

	return;
}
