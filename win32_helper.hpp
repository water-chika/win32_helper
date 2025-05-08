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
#include <vector>
#include <variant>
#include <span>
#include <concepts>

#include <cpp_helper.hpp>

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

static inline auto query_display_config(uint32_t flags) {
	std::vector<DISPLAYCONFIG_PATH_INFO> path_infos{};
	std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos{};
	uint32_t path_info_count{ 0 }, mode_info_count{ 0 };

	LONG res;
	do
	{
		res = GetDisplayConfigBufferSizes(flags, &path_info_count, &mode_info_count);
		if (res != ERROR_SUCCESS)
		{
			throw std::runtime_error{ "GetDisplayConfigBufferSizes fail" };
		}

		path_infos.resize(path_info_count);

		mode_infos.resize(mode_info_count);

		res = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_info_count, path_infos.data(), &mode_info_count, mode_infos.data(), nullptr);
	} while (res == ERROR_INSUFFICIENT_BUFFER);

	if (res != ERROR_SUCCESS) {
		throw std::runtime_error{ "QueryDisplayConfig fail" };
	}

	return std::pair{ std::move(path_infos), std::move(mode_infos) };
}


using cpp_helper::overloads;

template<typename T>
concept string_or_wide_string = requires (T t) {
	t.data();
	t.size();
};

static inline auto registry_open_key(HKEY key, const string_or_wide_string auto& sub_key, DWORD options, REGSAM desired) {
	HKEY out;
	auto res = overloads{ RegOpenKeyExW, RegOpenKeyExA }(key, sub_key.data(), options, desired, &out);
	if (res != ERROR_SUCCESS) {
		throw std::runtime_error{ "RegOpenKeyExW fail" };
	}
	return out;
}

static inline auto registry_close_key(HKEY key) {
	auto res = RegCloseKey(key);
	if (res != ERROR_SUCCESS) {
		throw std::runtime_error{ "RegCloseKey fail" };
	}
}

static inline auto foreach_registry_sub_key_name(HKEY key, auto fun) {
	std::vector<wchar_t> name(64);
	auto count = static_cast<DWORD>(name.size());
	DWORD i = 0;
	do {
		count = name.size();
		auto res = RegEnumKeyExW(key, i, name.data(), &count, nullptr, nullptr, nullptr, nullptr);
		if (res == ERROR_MORE_DATA) {
			name.resize(count);
			res = RegEnumKeyExW(key, i, name.data(), &count, nullptr, nullptr, nullptr, nullptr);
		}
		if (res == ERROR_SUCCESS) {
			fun(name);
		}
		else if (res == ERROR_NO_MORE_ITEMS) {
			break;
		}
		else {
			throw std::runtime_error{ "RegEnumKeyExW fail" };
		}
		++i;
	} while (true);
}


static inline auto registry_query_value(HKEY key, string_or_wide_string auto& name) {
	using string = std::basic_string<std::remove_cvref_t<decltype(name[0])>>;
	std::vector<uint8_t> data_buf(64);
	DWORD data_type{}, data_size{ static_cast<DWORD>(data_buf.size()) };
	auto res = overloads{ RegQueryValueExW, RegQueryValueExA}(key, name.data(), nullptr, &data_type, data_buf.data(), &data_size);
	if (res == ERROR_MORE_DATA) {
		data_buf.resize(data_size);
		res = overloads{ RegQueryValueExW, RegQueryValueExA }(key, name.data(), nullptr, &data_type, data_buf.data(), &data_size);
	}
	if (res != ERROR_SUCCESS) {
		throw std::runtime_error{ "RegQueryValueExW fail" };
	}

	std::variant<DWORD, string, std::vector<uint8_t>> data{};
	if (data_type == REG_DWORD) {
		data = *reinterpret_cast<DWORD*>(data_buf.data());
	}
	else if (data_type == REG_SZ) {
		data = string{ reinterpret_cast<string::value_type*>(data_buf.data()) };
	}
	else if (data_type == REG_BINARY) {
		data = data_buf;
	}
	else {
		throw std::runtime_error{ "unimplemented data type" };
	}

	return data;
}

class registry_key {
public:
	registry_key(HKEY parent, const string_or_wide_string auto& key_name, DWORD options, REGSAM desired)
		:
		m_key{registry_open_key(parent, key_name, options, desired)}
	{}
	~registry_key() {
		registry_close_key(m_key);
	}

	auto get_key() {
		return m_key;
	}

	auto query_value(const string_or_wide_string auto& key_name) {
		return registry_query_value(m_key, key_name);
	}

	auto foreach_sub_key_name(auto fun) {
		foreach_registry_sub_key_name(m_key,
			std::move(fun));
	}

private:
	HKEY m_key;
};

}