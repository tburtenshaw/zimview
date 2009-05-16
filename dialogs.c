//This function contains the dialog boxes and their associated functions
//In general if EndDialog nResult is 1 the main window needs to be redrawn



#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include "zinwellres.h"
#include "sqsh.h"
#include "md5.h"
#include "adler.h"
#include "zimview.h"
#include "dialogs.h"


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
	USUAL_STRUCTURE *tempUsualStruct;

    pHdr = (DLGHDR_PROPERTIES *) GetWindowLong(hwnd, GWL_USERDATA);
	iSel = TabCtrl_GetCurSel(pHdr->hwndTab);

    if (pHdr->hwndDisplay != NULL)	{
		if (pHdr->oldiSel==1)	{ //if we're moving from the info tab, potentially save the data
			if (pHdr->selectedBlock->typeOfBlock==BTYPE_BOXI)
				FillBoxiStructFromBoxiDlg(pHdr->hwndDisplay, &pHdr->dlgBoxiStruct);
		}
		DestroyWindow(pHdr->hwndDisplay);
	}

	pHdr->hwndDisplay = CreateDialogIndirect(hInst, pHdr->apRes[iSel], hwnd, ChildDlg);
	if (iSel>0)	{	//if we're switching to the extra info tab
		switch (pHdr->selectedBlock->typeOfBlock)	{
			case BTYPE_BOXI:
				FillBoxiDlgFromBoxiStruct(pHdr->hwndDisplay, &pHdr->dlgBoxiStruct);
				break;
			case BTYPE_VERI:
				break;
			default:
				tempUsualStruct = pHdr->selectedBlock->ptrFurtherBlockDetail;
				if (tempUsualStruct)	{
					if (tempUsualStruct->sqshHeader)	{
						FillDlgItemWithSquashFSData(pHdr->hwndDisplay, IDC_PROPERTIESLONGINFO, tempUsualStruct->sqshHeader);
					}
				}
				break;
		}
	}

	InvalidateRect(pHdr->hwndDisplay, NULL, TRUE);
	pHdr->oldiSel=iSel;
	return;
}

BOOL _stdcall PropertiesDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DLGHDR_PROPERTIES *pHdr;
	TCITEM tie;
	HANDLE hTab;
	int nSel;
	BLOCK_STRUCTURE *selectedBlock;
	BOXI_STRUCTURE *tempBoxiStruct;
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
			pHdr->oldiSel=0;

			if (nSel==1)
		    	pHdr->apRes[0] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_MAIN));
			else if (nSel>1)
		    	pHdr->apRes[0] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_MULTIPLE));
			else //nSel<1
		    	pHdr->apRes[0] = DoLockDlgRes(MAKEINTRESOURCE(IDD_PROPERTIES_ZIMFILE));

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
						tempBoxiStruct=selectedBlock->ptrFurtherBlockDetail;
						memcpy(&pHdr->dlgBoxiStruct, tempBoxiStruct, sizeof(BOXI_STRUCTURE));
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
					PropertiesApplyChanges(hwnd);
					return 1;
				case IDOK:
					PropertiesApplyChanges(hwnd);
					LocalFree(pHdr);
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}

void PropertiesApplyChanges(HWND hwnd)
{
	DLGHDR_PROPERTIES *pHdr;
	BLOCK_STRUCTURE *selectedBlock;

    pHdr = (DLGHDR_PROPERTIES *) GetWindowLong(hwnd, GWL_USERDATA);

	if (pHdr->selectedBlock)	{
		selectedBlock=pHdr->selectedBlock;

		switch (selectedBlock->typeOfBlock)	{
			case BTYPE_BOXI:

				if (pHdr->oldiSel==1)
					FillBoxiStructFromBoxiDlg(pHdr->hwndDisplay, &pHdr->dlgBoxiStruct);

				CreateValidBoxiBlockFromBoxiStruct(selectedBlock, &pHdr->dlgBoxiStruct);
				break;
		}

	}

	return;
}


BOOL _stdcall ChangeCustomerNumberDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ZIM_STRUCTURE *ZimToUse;
	char customerNumber[16];

	switch(msg) {
		case WM_INITDIALOG:
		    SetWindowLong(hwnd, GWL_USERDATA, lParam);
			ZimToUse=(ZIM_STRUCTURE *)lParam;
			sprintf(&customerNumber[0], "%i", ZimToUse->wCustomerNumber);
			SetDlgItemText(hwnd, IDC_CUSTOMERNUMBER, &customerNumber[0]);
			break;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hwnd,0);
					return 1;
				case IDOK:
					ZimToUse=(ZIM_STRUCTURE *)GetWindowLong(hwnd, GWL_USERDATA);
					GetDlgItemText(hwnd, IDC_CUSTOMERNUMBER, &customerNumber[0], 16);
					ZimToUse->wCustomerNumber=strtol(&customerNumber[0], NULL, 0);
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
					EndDialog(hwnd,0);
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
					FillBoxiStructFromBoxiDlg(hwnd, newBlock->ptrFurtherBlockDetail);
					CreateValidBoxiBlockFromBoxiStruct(newBlock, newBlock->ptrFurtherBlockDetail);
					EndDialog(hwnd,1);
					return 1;
				case IDCANCEL:
					EndDialog(hwnd,0);
					return 1;
			}
			break;
	}
	return 0;
}

void FillBoxiDlgFromBoxiStruct(HWND hwnd, BOXI_STRUCTURE *boxiStruct)
{
	char buffer[255];

	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.uiOUI);
	SetDlgItemText(hwnd, IDC_BOXIOUI, &buffer[0]);
	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.uiStarterImageSize);
	SetDlgItemText(hwnd, IDC_BOXISTARTERIMAGESIZE, &buffer[0]);
	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.wHwVersion);
	SetDlgItemText(hwnd, IDC_BOXIHWV, &buffer[0]);
	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.wSwVersion);
	SetDlgItemText(hwnd, IDC_BOXISWV, &buffer[0]);
	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.wHwModel);
	SetDlgItemText(hwnd, IDC_BOXIHWM, &buffer[0]);
	sprintf(buffer, "0x%x", boxiStruct->boxiFileData.wSwModel);
	SetDlgItemText(hwnd, IDC_BOXISWM, &buffer[0]);
	MD5HexString(&buffer[0], boxiStruct->boxiFileData.abStarterMD5Digest);
	SetDlgItemText(hwnd, IDC_BOXISTARTERMD5, &buffer[0]);
	return;
}

void FillBoxiStructFromBoxiDlg(HWND hwnd, BOXI_STRUCTURE *boxiStruct)
{
	char buffer[255];

	memset(&boxiStruct->boxiFileData, 0, sizeof(BOXIBLOCK_STRUCTURE));

	GetDlgItemText(hwnd, IDC_BOXIOUI, &buffer[0], 255);
	boxiStruct->boxiFileData.uiOUI=strtoul(&buffer[0], NULL, 0);
	GetDlgItemText(hwnd, IDC_BOXISTARTERIMAGESIZE, &buffer[0], 255);
	boxiStruct->boxiFileData.uiStarterImageSize=strtoul(&buffer[0], NULL, 0);
	GetDlgItemText(hwnd, IDC_BOXIHWV, &buffer[0], 255);
	boxiStruct->boxiFileData.wHwVersion=strtoul(&buffer[0], NULL, 0);
	GetDlgItemText(hwnd, IDC_BOXISWV, &buffer[0], 255);
	boxiStruct->boxiFileData.wSwVersion=strtoul(&buffer[0], NULL, 0);
	GetDlgItemText(hwnd, IDC_BOXIHWM, &buffer[0], 255);
	boxiStruct->boxiFileData.wHwModel=strtoul(&buffer[0], NULL, 0);
	GetDlgItemText(hwnd, IDC_BOXISWM, &buffer[0], 255);
	boxiStruct->boxiFileData.wSwModel=strtoul(&buffer[0], NULL, 0);

	GetDlgItemText(hwnd, IDC_BOXISTARTERMD5, &buffer[0], 255);
	MD5StringToArray(boxiStruct->boxiFileData.abStarterMD5Digest, &buffer[0]);

	return;
}


void CreateValidBoxiBlockFromBoxiStruct(BLOCK_STRUCTURE *boxiBlock, BOXI_STRUCTURE *boxiStruct)
{
	ADLER_STRUCTURE adlerholder;
	struct cvs_MD5Context MD5context;
	struct sBlockHeader blockHeader;

	memcpy(boxiBlock->ptrFurtherBlockDetail, boxiStruct, sizeof(BOXIBLOCK_STRUCTURE));
	//Now calculate MD5 and adler on this block
	//MD5 just on the data
	cvs_MD5Init(&MD5context);
	cvs_MD5Update (&MD5context, &(boxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE));
	cvs_MD5Final (&boxiBlock->md5, &MD5context);
	//Adler32 on the data then block header
	memset(&adlerholder, 0, sizeof(adlerholder));
	ChecksumAdler32(&adlerholder, &(boxiStruct->boxiFileData), sizeof(BOXIBLOCK_STRUCTURE)); //the boxi we loaded
	GenerateBlockHeader(&blockHeader, boxiBlock->dwDataLength, 0, "BOXI");
	boxiBlock->blockSignature[0]=blockHeader.blockSignature[0];
	boxiBlock->blockSignature[1]=blockHeader.blockSignature[1];
	boxiBlock->blockSignature[2]=blockHeader.blockSignature[2];
	boxiBlock->blockSignature[3]=blockHeader.blockSignature[3];

	boxiBlock->dwRealChecksum = ChecksumAdler32(&adlerholder, &blockHeader, sizeof(struct sBlockHeader)-sizeof(DWORD)); //the start of the header without checksum

	return;
}

void FillDlgItemWithSquashFSData(HWND hDlg, int nIDDlgItem, SQUASHFS_SUPER_BLOCK *sqshHeader)
{
	char buffer[1024];
	int bufferOffset=0;
	int i=0;

	i = sprintf(buffer+bufferOffset, "SquashFS information:\r\n\r\n");
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "SquashFS version: %i.%i\r\n", sqshHeader->s_major, sqshHeader->s_minor);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Filesystem size: %i bytes\r\n", (long)sqshHeader->bytes_used);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Block size: %i\r\n", sqshHeader->block_size);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Number of fragments: %i\r\n", sqshHeader->fragments);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Number of inodes: %i\r\n", sqshHeader->inodes);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Number of uids: %i\r\n", sqshHeader->no_uids);
	bufferOffset+=i;
	i = sprintf(buffer+bufferOffset, "Number of gids: %i\r\n", sqshHeader->no_guids);

	SetDlgItemText(hDlg, nIDDlgItem, &buffer[0]);

	return;
}

BOOL _stdcall BlockImportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ZIM_STRUCTURE *ZimToUse;
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
		case WM_INITDIALOG:
		   	SetWindowLong(hwnd, GWL_USERDATA, lParam);
			break;
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			ZimToUse=(ZIM_STRUCTURE *)GetWindowLong(hwnd, GWL_USERDATA);
			switch (LOWORD(wParam)) {
				case IDC_BLOCKIMPORTROOT:
				case IDC_BLOCKIMPORTCODE:
				case IDC_BLOCKIMPORTKERN:
				case IDC_BLOCKIMPORTBOXI:
				case IDC_BLOCKIMPORTVERI:
				case IDC_BLOCKIMPORTLOAD:
				case IDC_BLOCKIMPORTNVRM:
                    CheckRadioButton(hwnd, IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, LOWORD(wParam));
					return 1;
				case IDCANCEL:
					EndDialog(hwnd,0);
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
						newBlock=NewBlock(ZimToUse);
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
									ZimToUse->wNumBlocks--;
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



BOOL _stdcall BlockExportDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ZIM_STRUCTURE *ZimToUse;
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
		    SetWindowLong(hwnd, GWL_USERDATA, lParam);
			ZimToUse=(ZIM_STRUCTURE *)lParam;

			SetDlgItemText(hwnd, IDC_BLOCKEXPORTTYPE, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTSIZE, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTOFFSET, "");
			SetDlgItemText(hwnd, IDC_BLOCKEXPORTDETAIL, "");

			CheckDlgButton(hwnd, IDC_BLOCKEXPORTINCLUDEHEADER, BST_UNCHECKED);

			tempBlockStruct=ZimToUse->first;

			selectedBlockId=-1; //we have no idea what block to export just yet
			for (i=0;i<ZimToUse->wNumBlocks;i++) {
				SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_ADDSTRING, 0, (LPARAM)tempBlockStruct->name);
				if ((tempBlockStruct->flags & BSFLAG_ISSELECTED) && (selectedBlockId<0)) {
					selectedBlockId=i;
					if ((tempBlockStruct->typeOfBlock==BTYPE_BOXI)||(tempBlockStruct->typeOfBlock==BTYPE_VERI))
				    	CheckDlgButton(hwnd, IDC_BLOCKEXPORTINCLUDEHEADER, BST_CHECKED); //if it's a boxi or veri, include header
				}
				tempBlockStruct=tempBlockStruct->next;
			}

			if (selectedBlockId>=0) {
				BlockExportDetailsUpdate(hwnd, ZimToUse, selectedBlockId);
				selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_SETCURSEL, selectedBlockId, 0);
			}

			return 1;
		case WM_CLOSE:
			EndDialog(hwnd, 0);
			return 1;
		case WM_COMMAND:
			ZimToUse=(ZIM_STRUCTURE *)GetWindowLong(hwnd, GWL_USERDATA);
			switch (LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hwnd, 0);
					return 1;
				case IDOK: //in this case this is the Export button
					//Get which block we want to export, and fill the selectedBlockStruct with that
					selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_GETCURSEL, 0, 0);
					if (selectedBlockId==CB_ERR) {MessageBox(hwnd, "No block has been selected. Please chose a block to export.", "Export",MB_ICONEXCLAMATION); return 1;}

					tempBlockStruct=ZimToUse->first;
					for (i=0;i<max(ZimToUse->wNumBlocks, selectedBlockId+1);i++) {
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
						WriteBlockToFile(ZimToUse, &selectedBlockStruct, filePtrExport, (includeHeader==BST_CHECKED));
						fclose(filePtrExport);

						EndDialog(hwnd, 1);
						return 1;
					}

					return 1;
				case IDC_BLOCKLIST:
					if (HIWORD(wParam)==CBN_SELCHANGE) {
						selectedBlockId = SendDlgItemMessage(hwnd, IDC_BLOCKLIST, CB_GETCURSEL, 0, 0);
						BlockExportDetailsUpdate(hwnd, ZimToUse, selectedBlockId);
					}

					return 1;
			}
			break;
	}
	return 0;
}

void BlockExportDetailsUpdate(HWND hwnd, ZIM_STRUCTURE *ZimToUse, int blockid)
{
	BLOCK_STRUCTURE selectedBlockStruct;
	BLOCK_STRUCTURE *tempBlockStruct;
	int i;
	char tempString[511]; //needs to accommodate a longish info string about each block

	USUAL_STRUCTURE *tempUsualStructure;


	tempBlockStruct=ZimToUse->first;
	for (i=0;i<max(ZimToUse->wNumBlocks, blockid+1);i++) {
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

BOOL _stdcall BlockCreateVeriDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ZIM_STRUCTURE *ZimToUse;
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
		   	SetWindowLong(hwnd, GWL_USERDATA, lParam);
			ZimToUse=(ZIM_STRUCTURE *)lParam;

			SetDlgItemText(hwnd, IDC_VERIVERA, "");
			SetDlgItemText(hwnd, IDC_VERIVERB, "");
			SetDlgItemText(hwnd, IDC_VERIVERC, "0");
			SetDlgItemText(hwnd, IDC_VERIVERD, "");

			tempBlockStruct=ZimToUse->first;

			for (i=0;i<ZimToUse->wNumBlocks;i++) {
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
			ZimToUse=(ZIM_STRUCTURE *)GetWindowLong(hwnd, GWL_USERDATA);
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

						tempBlockStruct=GetBlockNumber(ZimToUse, arraySelectedLB[i]);

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
					newBlock=NewBlock(ZimToUse);
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

					EndDialog(hwnd, 1);
					return 1;
				case IDCANCEL:
					EndDialog(hwnd, 0);
					return 1;
			}
			break;
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
		if (magic==BID_DWORD_LOAD) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTLOAD);}
		if (magic==BID_DWORD_ROOT) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTROOT);}
		if (magic==BID_DWORD_CODE) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTCODE);}
		if (magic==BID_DWORD_KERN) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTKERN);}
		if (magic==BID_DWORD_NVRM) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTNVRM);}
		if (magic==BID_DWORD_BOXI) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTBOXI);}
		if (magic==BID_DWORD_VERI) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTVERI);}

		fclose(impBlock);
		return 0;
	}

	CheckDlgButton(hwnd, IDC_BLOCKIMPORTINCLUDEHEADER, BST_UNCHECKED);

	if (((magic==SQUASHFS_MAGIC_LZMA)||(magic==SQUASHFS_MAGIC))&&(fileLength>=sizeof(SQUASHFS_SUPER_BLOCK))) {
		fseek(impBlock, 0, SEEK_SET);
		fread(&sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK), 1, impBlock);
		if ((sqshHeader.inodes>=3)&&(sqshHeader.inodes<=10)) {CheckRadioButton(hwnd, IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTCODE);}
		if ((sqshHeader.inodes>=250)&&(sqshHeader.inodes<=350)) {CheckRadioButton(hwnd,IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTROOT);}

		fclose(impBlock);
		return 0;
	}

	if ((magic & 0xffff)==0x8b1f) { //KERNs i have seen are the only GZIP files i've seen
		CheckRadioButton(hwnd, IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTKERN);
		fclose(impBlock);
		return 0;
	}


	if (fileLength==36) {//BOXI without header is 36 bytes
		CheckRadioButton(hwnd, IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTBOXI);
		fclose(impBlock);
		return 0;
	}

	if (!(fileLength & 0x1F)) { //if divisible by 32, it _could_ be a VERI block

		if ((magic==BID_DWORD_LOAD)||(magic==BID_DWORD_ROOT)||(magic==BID_DWORD_CODE)||(magic==BID_DWORD_KERN)||(magic==BID_DWORD_NVRM)||(magic==BID_DWORD_BOXI)||(magic==BID_DWORD_VERI)) {
			CheckRadioButton(hwnd, IDC_BLOCKIMPORTFIRSTBLOCK, IDC_BLOCKIMPORTLASTBLOCK, IDC_BLOCKIMPORTVERI);
			fclose(impBlock);
			return 0;
		}
	}

	fclose(impBlock);
	return 0;
}
