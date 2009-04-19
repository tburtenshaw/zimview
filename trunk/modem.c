/*@@ Wedit generated application. Written Tue Nov 08 12:09:54 2005
 @@header: e:\programming\lcc\projects\default\modemres.h
 @@resources: e:\programming\lcc\projects\default\modem.rc
 Do not edit outside the indicated areas */
/*<---------------------------------------------------------------------->*/
/*<---------------------------------------------------------------------->*/
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <stdio.h>
#include "modemres.h"
/*<---------------------------------------------------------------------->*/
HINSTANCE hInst;		// Instance handle
HWND hwndMain;		//Main window handle
BOOL Connection;	//Are we connected to the modem

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
int TestComm(HWND hwnd);

/*<---------------------------------------------------------------------->*/
/*@@0->@@*/
static BOOL InitApplication(void)
{
	WNDCLASS wc;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "modemWndClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	if (!RegisterClass(&wc))
		return 0;
/*@@0<-@@*/
	// ---TODO--- Call module specific initialization routines here

	return 1;
}

/*<---------------------------------------------------------------------->*/
/*@@1->@@*/
HWND CreatemodemWndClassWnd(void)
{
	return CreateWindow("modemWndClass","modem",
		WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,
		CW_USEDEFAULT,0,CW_USEDEFAULT,0,
		NULL,
		NULL,
		hInst,
		NULL);
}
/*@@1<-@@*/
/*<---------------------------------------------------------------------->*/
/* --- The following code comes from e:\programming\lcc\lib\wizard\defOnCmd.tpl. */
void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch(id) {
		// ---TODO--- Add new menu commands here
		/*@@NEWCOMMANDS@@*/
		case IDM_EXIT:
		PostMessage(hwnd,WM_CLOSE,0,0);
		break;
	}
}

/*<---------------------------------------------------------------------->*/
/*@@2->@@*/
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{


	switch (msg) {
/*@@3->@@*/
	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);
		break;
	case WM_TIMER:
		{
		TestComm(hwnd);
		}
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
/*@@2<-@@*/



int TestComm(HWND hwnd)
{
HDC hdc;
HDC hSerialPort;
RECT rect;
CHAR Buffer[255];
CHAR ScreenBuffer[255];
DWORD BytesWritten;
DWORD BytesRead;
INT result;
INT e;
DCB dcbConfig;
COMMTIMEOUTS timeouts;


hdc = GetDC(hwnd);

hSerialPort = CreateFile("COM4", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);


/* Setup states etc*/

timeouts.ReadIntervalTimeout = 20;
timeouts.ReadTotalTimeoutMultiplier = 10;
timeouts.ReadTotalTimeoutConstant = 100;
timeouts.WriteTotalTimeoutMultiplier = 10;
timeouts.WriteTotalTimeoutConstant = 100;

if (!SetCommTimeouts(hSerialPort, &timeouts))
{
    printf("Problem setting timeouts");
	}



if(GetCommState(hSerialPort, &dcbConfig))
{
    dcbConfig.BaudRate = 19200;
    dcbConfig.ByteSize = 8;
    dcbConfig.Parity = NOPARITY;
    dcbConfig.StopBits = ONESTOPBIT;
    dcbConfig.fBinary = TRUE;
    dcbConfig.fParity = TRUE;
}

else {
    printf("Couldn't get\r\n");
	}

if(!SetCommState(hSerialPort, &dcbConfig))
	{
	printf("Couldn't set\r\n");
	}



		SetCommMask(hSerialPort, EV_RXCHAR);



		sprintf(Buffer, "AT+CSQ\r\n");
		printf("Sending code: %s", Buffer);

		result = SetupComm(hSerialPort,128,128);

		result=WriteFile(hSerialPort, &Buffer, strlen(Buffer), &BytesWritten, NULL);
		sprintf(ScreenBuffer, "WriteResult %i, BytesWritten: %i",result,BytesWritten);
		TextOut(hdc, 0, 32, ScreenBuffer, strlen(ScreenBuffer));
		printf("%s", ScreenBuffer);


	for (e=0;e<3;e++)
		{
		memset(&Buffer,0,255);
		result=ReadFile(hSerialPort, &Buffer, 32, &BytesRead, NULL);

		if (!BytesRead)
			sprintf(ScreenBuffer, "Read %i %i %s",result,BytesRead, Buffer);
		else {
			sprintf(ScreenBuffer, "%s", Buffer);
			if (strpbrk(Buffer, "OK")) e=4;
			printf("%s", ScreenBuffer);
		}

		//TextOut(hdc, e*5, 48+16*(e%20), ScreenBuffer, strlen(ScreenBuffer));

	}


	/* again */
		sprintf(Buffer, "AT+CGATT?\r\n");
		sprintf(ScreenBuffer, "Sending code: %s", Buffer);
		TextOut(hdc, 0, 16, ScreenBuffer, strlen(ScreenBuffer));

		result=WriteFile(hSerialPort, &Buffer, strlen(Buffer), &BytesWritten, NULL);
		sprintf(ScreenBuffer, "WriteResult %i, BytesWritten: %i",result,BytesWritten);
		TextOut(hdc, 0, 32, ScreenBuffer, strlen(ScreenBuffer));
		printf("%s", ScreenBuffer);


	for (e=0;e<3;e++)
		{
		memset(&Buffer,0,255);
		result=ReadFile(hSerialPort, &Buffer, 32, &BytesRead, NULL);

		if (!BytesRead)
			sprintf(ScreenBuffer, "Read %i %i %s",result,BytesRead, Buffer);
		else {
			sprintf(ScreenBuffer, "%s", Buffer);
			if (strpbrk(Buffer, "OK")) e=4;
			printf("%s", ScreenBuffer);
		}

		//TextOut(hdc, e*5, 48+16*(e%20), ScreenBuffer, strlen(ScreenBuffer));

	}






		CloseHandle(hSerialPort);


  			// Draw string to screen.

		ReleaseDC(hwnd, hdc);



	return 0;
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
	if ((hwndMain = CreatemodemWndClassWnd()) == (HWND)0)
		return 0;
	ShowWindow(hwndMain,SW_SHOW);
	SetTimer(hwndMain, 1, 1000*1, NULL);
	while (GetMessage(&msg,NULL,0,0)) {
		if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}
