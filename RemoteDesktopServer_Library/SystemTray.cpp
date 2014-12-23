#include "stdafx.h"
#include "SystemTray.h"
#include "resource.h"
#include "..\RemoteDesktop_Library\Desktop_Monitor.h"

#define ID_TRAY_APP_ICON    1001
#define ID_TRAY_EXIT        1002
#define ID_TRAY_EXIT_REMOVE	1003
#define ID_TRAY_BALLOON		1004
#define ID_TRAY_APP_TIMER   1005
#define WM_SYSICON          (WM_USER + 1)

RemoteDesktop::SystemTray::SystemTray() {
	_SystemTrayIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0);
}
RemoteDesktop::SystemTray::~SystemTray(){
	Stop();
	_Cleanup();
}
void RemoteDesktop::SystemTray::_Cleanup(){
	if (_SystemTrayIcon != nullptr) DestroyIcon(_SystemTrayIcon);
	_SystemTrayIcon = nullptr;
	KillTimer(Hwnd, ID_TRAY_APP_TIMER);
	if (notifyIconData.hWnd != 0) {
		Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
		memset(&notifyIconData, 0, sizeof(notifyIconData));
	}
}
void RemoteDesktop::SystemTray::Start(){
	_BackGroundThread = std::thread(&SystemTray::_Run, this);
}
void RemoteDesktop::SystemTray::Stop(){
	PostMessage(Hwnd, WM_QUIT, 0, 0);
	if (std::this_thread::get_id() != _BackGroundThread.get_id()){
		if (_BackGroundThread.joinable()) _BackGroundThread.join();
	}

}

UINT s_uTaskbarRestart = 0;
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CREATE) s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
	RemoteDesktop::SystemTray *c = (RemoteDesktop::SystemTray *)GetWindowLong(hWnd, GWLP_USERDATA);
	if (c == NULL)
		return DefWindowProc(hWnd, msg, wParam, lParam);
	return c->WindowProc(hWnd, msg, wParam, lParam);
}
void RemoteDesktop::SystemTray::_CreateIcon(HWND hWnd){

	if (_TrayIconCreated) return;
	notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
	notifyIconData.hWnd = hWnd;
	notifyIconData.uID = ID_TRAY_APP_ICON;
	notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	notifyIconData.uCallbackMessage = WM_SYSICON; //Set up our invented Windows Message

	notifyIconData.hIcon = _SystemTrayIcon;
	TCHAR szTIP[64] = TEXT("Remote Desktop Process");
	wcscpy_s(notifyIconData.szTip, szTIP);

	if (Shell_NotifyIcon(NIM_ADD, &notifyIconData)){
		_TrayIconCreated = true;
	}

}


LRESULT RemoteDesktop::SystemTray::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg){
	case(WM_TIMER) :
		if (FindWindow(L"Shell_TrayWnd", 0)){
			_CreateIcon(Hwnd);
		}
				   break;
	case(WM_QUIT) :
	case(WM_CLOSE) :
	case(WM_DESTROY) :
	case(WM_QUERYENDSESSION) :
	case(WM_ENDSESSION) :
						_Cleanup();
		break;
	case(WM_SYSICON) :
		if (lParam == WM_RBUTTONUP)
		{
			// Get current mouse position.
			POINT curPoint;
			GetCursorPos(&curPoint);
			SetForegroundWindow(hWnd);
			// TrackPopupMenu blocks the app until TrackPopupMenu returns
			UINT clicked = TrackPopupMenu(Hmenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, hWnd, NULL);
			SendMessage(hWnd, WM_NULL, 0, 0); // send benign message to window to make sure the menu goes away.
			if (clicked == ID_TRAY_EXIT || clicked == ID_TRAY_EXIT_REMOVE){
				PostMessage(Hwnd, WM_QUIT, 0, 0);
			}
		}
		break;

	}
	if (msg == s_uTaskbarRestart){
		_TrayIconCreated = false;
		_CreateIcon(Hwnd);
	}
	return DefWindowProc(hWnd, msg, msg, lParam);
}
void RemoteDesktop::SystemTray::_Run(){
	DesktopMonitor dekstopmonitor;
	dekstopmonitor.Switch_to_Desktop(DesktopMonitor::DEFAULT);
	memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));

	auto myclass = L"systrayclass";
	WNDCLASSEX wndclass = {};
	memset(&wndclass, 0, sizeof(wndclass));
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.lpfnWndProc = WndProc;
	wndclass.lpszClassName = myclass;

	if (RegisterClassEx(&wndclass)) Hwnd = CreateWindowEx(0, myclass, L"systraywatcher", 0, 0, 0, 0, 0, HWND_DESKTOP, 0, GetModuleHandle(NULL), 0);
	else 	return DEBUG_MSG("Error %", GetLastError());

	Hmenu = CreatePopupMenu();
	AppendMenu(Hmenu, MF_STRING, ID_TRAY_EXIT, TEXT("Exit"));
	AppendMenu(Hmenu, MF_STRING, ID_TRAY_EXIT_REMOVE, TEXT("Exit and Remove"));

	//_CreateIcon(Hwnd);

	ShowWindow(Hwnd, SW_HIDE);
	SetWindowLongPtr(Hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SetTimer(Hwnd, ID_TRAY_APP_TIMER, 1000, NULL); //every 1000 ms windows will send a timer notice to the msg proc below. This allows the destructor to set _Running to false and the message proc to break
	MSG msg;
	while (GetMessage(&msg, Hwnd, 0, 0) != 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

}

void RemoteDesktop::SystemTray::Popup(const wchar_t* title, const wchar_t* message, unsigned int timeout){
	NOTIFYICONDATA ni;
	memset(&ni, 0, sizeof(NOTIFYICONDATA));
	ni.cbSize = sizeof(NOTIFYICONDATA);
	ni.hWnd = Hwnd;
	ni.uID = ID_TRAY_APP_ICON;
	ni.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIcon(NIM_SETVERSION, &ni);
	ni.uFlags = NIF_INFO;
	ni.uTimeout = timeout;
	wcscpy_s(ni.szInfo, message);
	wcscpy_s(ni.szInfoTitle, title);
	Shell_NotifyIcon(NIM_MODIFY, &ni);
	ni.uVersion = 0;
	Shell_NotifyIcon(NIM_SETVERSION, &ni);
}