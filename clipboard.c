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
#include "clipboard.h"

int EditCopySelected(HWND hwnd, ZIM_STRUCTURE *LoadedZim, BOOL bCut)
{
	LPTSTR  lptstrCopy;
	HGLOBAL hglbCopy;
	HANDLE hSuccess;

	int i;
	WORD n;
	int bufferSize;
	int cch;
	char *buffer;

	BLOCK_STRUCTURE *ptrCounterBlock;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;
	WORD counterVeriPart;

	n=SelectedCount(LoadedZim);
	if (n==0) return 0;

	if (!OpenClipboard(hwnd)) return 0;
	EmptyClipboard();

	bufferSize=CalculateClipboardSize(LoadedZim);

	buffer=malloc(bufferSize);

	if (!buffer)	{
		MessageBox(hwnd, "Not enough memory to copy all these blocks.", "Copy", MB_OK|MB_ICONEXCLAMATION);
		return 0;
	}

	//This writes a stream of data to the buffer, then places a copy in the clipboard
	//The data stream is the internal blockdata, followed by the additional data
	memset(buffer, 0, bufferSize);
	ptrCounterBlock=LoadedZim->first;
	cch=0;

	memcpy(buffer+cch, &n, sizeof(WORD));
	cch+=sizeof(WORD);

	for (i=0; i<LoadedZim->wNumBlocks; i++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
			memcpy(buffer+cch, ptrCounterBlock, sizeof(BLOCK_STRUCTURE));
			cch+=sizeof(BLOCK_STRUCTURE);

			if (ptrCounterBlock->ptrFurtherBlockDetail)	{
				switch (ptrCounterBlock->typeOfBlock)	{
					case BTYPE_BOXI:
						memcpy(buffer+cch, ptrCounterBlock->ptrFurtherBlockDetail, sizeof(BOXI_STRUCTURE));
						cch+=sizeof(BOXI_STRUCTURE);
						break;
					case BTYPE_VERI:
						tempVeriStruct=ptrCounterBlock->ptrFurtherBlockDetail;
						counterVeriPart=tempVeriStruct->numberVeriObjects;
						while (counterVeriPart) {
							memcpy(buffer+cch, tempVeriStruct, sizeof(VERI_STRUCTURE));
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
					case BTYPE_MACA:
						tempUsualStruct=ptrCounterBlock->ptrFurtherBlockDetail;
						memcpy(buffer+cch, tempUsualStruct, sizeof(USUAL_STRUCTURE));
						cch+=sizeof(USUAL_STRUCTURE);
						if (tempUsualStruct->gzipHeader)	{
							memcpy(buffer+cch, tempUsualStruct->gzipHeader, sizeof(GZIP_HEADER_BLOCK));
							cch+=sizeof(GZIP_HEADER_BLOCK);
						}
						if (tempUsualStruct->sqshHeader)	{
							memcpy(buffer+cch, tempUsualStruct->sqshHeader, sizeof(SQUASHFS_SUPER_BLOCK));
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
        free(buffer);
		return 0;
	}

	// Lock the handle and copy the binary data to the buffer.
    lptstrCopy = GlobalLock(hglbCopy);
    memcpy(lptstrCopy, buffer,  cch);
    GlobalUnlock(hglbCopy);
	SetClipboardData(uZimBlockFormat, hglbCopy);


	/* TEXT */
	//This writes text to the buffer, then places a copy of it the clipboard
	memset(buffer, 0, bufferSize);
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

			if (cch>bufferSize) i=LoadedZim->wNumBlocks;
		}

		ptrCounterBlock=ptrCounterBlock->next;
	}

	// Allocate a global memory object for the text.
    hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (cch + 1));
    if (hglbCopy == NULL) {
        CloseClipboard();
		free(buffer);
        return 0;
	}

    // Lock the handle and copy the text to the buffer.
    lptstrCopy = GlobalLock(hglbCopy);
    memcpy(lptstrCopy, buffer,  cch);
    lptstrCopy[cch] = 0;    // null character
    GlobalUnlock(hglbCopy);

    // Place the handle on the clipboard.
	hSuccess = SetClipboardData(CF_TEXT, hglbCopy);
	if (!hSuccess) {
		GlobalFree(hglbCopy); //we only need to free if the SetClipboardData didn't work
		CloseClipboard();
		free(buffer);
		return 0;
	}

    CloseClipboard();
	free(buffer);
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

		newBlock=NULL;
		for (count=0; count<n; count++) {
			if (newBlock==NULL)
				newBlock = NewBlock(LoadedZim);
			else	{
				newBlock->next=malloc(sizeof(BLOCK_STRUCTURE));
				newBlock=newBlock->next;
				if (newBlock)	{
					memset(newBlock, 0, sizeof(BLOCK_STRUCTURE));
					LoadedZim->wNumBlocks++;
				}
			}
			memcpy(newBlock, ptrCbData+cch, sizeof(BLOCK_STRUCTURE));
			cch+=sizeof(BLOCK_STRUCTURE);

			newBlock->next=NULL;
			newBlock->flags|=BSFLAG_HASCHANGED;
			newBlock->flags&=(0xffff^BSFLAG_ISSELECTED);

			if (newBlock->ptrFurtherBlockDetail) {
				switch(newBlock->typeOfBlock)	{
					case BTYPE_ROOT:
					case BTYPE_CODE:
					case BTYPE_KERN:
					case BTYPE_LOAD:
					case BTYPE_NVRM:
					case BTYPE_MACA:
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
						newBlock->flags|=BSFLAG_EXTERNFILE;
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

long CalculateClipboardSize(ZIM_STRUCTURE *LoadedZim)
{
	long maxClipboardSize=0;
	int i;
	BLOCK_STRUCTURE *ptrCounterBlock;
	VERI_STRUCTURE *tempVeriStruct;
	USUAL_STRUCTURE *tempUsualStruct;


	maxClipboardSize+=sizeof(WORD);
	ptrCounterBlock=LoadedZim->first;
	//the usual drill. Go through all the blocks, finding the size of them all
	for (i=0; i<LoadedZim->wNumBlocks; i++) {
		if (ptrCounterBlock->flags & BSFLAG_ISSELECTED) {
			maxClipboardSize+=sizeof(BLOCK_STRUCTURE);
			switch(ptrCounterBlock->typeOfBlock)	{
				case BTYPE_BOXI:
					maxClipboardSize+=sizeof(BOXI_STRUCTURE);
					break;
				case BTYPE_VERI:
					tempVeriStruct=ptrCounterBlock->ptrFurtherBlockDetail;
					maxClipboardSize+=sizeof(VERI_STRUCTURE) * tempVeriStruct->numberVeriObjects;
					break;
				case BTYPE_ROOT:
				case BTYPE_CODE:
				case BTYPE_KERN:
				case BTYPE_LOAD:
				case BTYPE_NVRM:
				case BTYPE_MACA:
					tempUsualStruct=ptrCounterBlock->ptrFurtherBlockDetail;
					maxClipboardSize+=sizeof(USUAL_STRUCTURE);
					if (tempUsualStruct->gzipHeader)	{
						maxClipboardSize+=sizeof(GZIP_HEADER_BLOCK);
					}
					if (tempUsualStruct->sqshHeader)	{
						maxClipboardSize+=sizeof(SQUASHFS_SUPER_BLOCK);
					}
					break;
			}
		}
		ptrCounterBlock=ptrCounterBlock->next;
	}

return 	maxClipboardSize;
}
