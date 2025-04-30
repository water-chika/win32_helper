#pragma once
#ifdef _WIN32
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
#include <thread>
#include <mutex>
#include <latch>

namespace win32_helper{
class Window {
public:
	Window() : Window(CW_USEDEFAULT,CW_USEDEFAULT) {}
	Window(auto width, auto height) : m_handle{ create_window(width, height) } {

	}
	HWND get_handle() { return m_handle; }
	void run() {
		MSG msg = {};
		while (GetMessageA(&msg, NULL, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
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
	HWND create_window(auto width, auto height) {
		const char CLASS_NAME[] = "Sample Window Class";

		auto hInstance = GetModuleHandleA(NULL);

		WNDCLASSA wc = {};

		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;

		RegisterClassA(&wc);

		HWND handle = CreateWindowExA(
			0,
			CLASS_NAME,
			"Sample",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, width, height,
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
class WindowWithinThread {
public:
	WindowWithinThread() : WindowWithinThread{ create_window_thread() } {};
	WindowWithinThread(std::pair<std::thread, Window&> pair)
		: m_thread{std::move(pair.first)}, m_window{pair.second} {
	}
	~WindowWithinThread() {
		m_thread.join();
	}
	auto& get_thread() {
		return m_thread;
	}
	auto& get_window() {
		return m_window;
	}
private:
	static std::pair<std::thread, Window&> create_window_thread() {
		Window* p_window;
		std::latch window_created{ 1 };
		auto thread = std::thread{
			[&p_window, &window_created]() {
				Window window{};
				p_window = &window;
				window_created.count_down();
				window.run();
			}
		};
		window_created.wait();
		return std::pair<std::thread, Window&>{ std::move(thread), *p_window };
	}
	std::thread m_thread;
	Window& m_window;
};
}