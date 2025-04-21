#pragma once
#ifdef WIN32
#include<windows.h>
#else
using HWND = int*;
using UINT = unsigned int;
#define CALLBACK
using WPARAM = int;
using LPARAM = int;
enum {
WM_CREATE,
WM_SIZE,
WM_DESTROY,
WM_QUIT,
};
struct CREATESTRUCT{void* lpCreateParams;};
void PostQuitMessage(int){}
using LRESULT = int;
LRESULT DefWindowProcA(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return LRESULT{}; }
using ATOM = int;
struct WNDCLASS{int* hInstance; const char* lpszClassName; typeof(DefWindowProcA)* lpfnWndProc;};
HWND GetModuleHandle(void*){ return HWND{};}
using LPCSTR = char*;
ATOM RegisterClass(WNDCLASS*) { return ATOM{};}
void UnregisterClass(LPCSTR, HWND){}
struct RECT{int left,right,top,bottom;};
#define CW_USEDEFAULT 0
#define SW_SHOWNORMAL 0
void ShowWindow(HWND, int){}
#define WS_OVERLAPPEDWINDOW 0
struct MSG{int message;};
#define PM_REMOVE 0
int PeekMessage(MSG*, void*, int, int, int){ return 0; }
void TranslateMessage(MSG*){}
void DispatchMessage(MSG*){}
HWND CreateWindowA(LPCSTR,...){ return nullptr; }
void AdjustWindowRect(RECT*,...){}
#endif

#include <stdexcept>

namespace win32_helper{
class Window {
public:
	Window() : m_handle{ create_window() } {

	}
	HWND get_handle() { return m_handle; }
	void run() {
		MSG msg = {};
		while (GetMessage(&msg, NULL, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
	HWND create_window() {
		const char CLASS_NAME[] = "Sample Window Class";

		auto hInstance = GetModuleHandleA(NULL);

		WNDCLASSA wc = {};

		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;

		RegisterClass(&wc);

		HWND handle = CreateWindowExA(
			0,
			CLASS_NAME,
			"Sample",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL,
			NULL,
			hInstance,
			NULL
		);

		if (handle == NULL) {
			throw std::runtime_error{ "Create Window fail" };
		}

		ShowWindow(handle, SW_NORMAL);

		return handle;
	}
	HWND m_handle;
};
}