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
#include <optional>
#include <cassert>
#include <filesystem>

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

class DeviceContext {
public:
	DeviceContext(HWND hwnd, HDC hdc) : m_hwnd{ hwnd }, m_hdc { hdc } {
		if (hdc == NULL) {
			throw std::runtime_error{ "hdc is null" };
		}
	}
	~DeviceContext() {
		ReleaseDC(m_hwnd, m_hdc);
	}

	auto get_handle() {
		return m_hdc;
	}
private:
	HWND m_hwnd;
	HDC m_hdc;
};

static inline auto get_display_monitors() {
	std::vector<HMONITOR> monitors{};
	MONITORENUMPROC monitor_enum_proc = [](HMONITOR hMonitor,
		HDC      hdcMonitor,
		LPRECT   lprcMonitor,
		LPARAM   dwData) -> BOOL {
			auto& vector = *reinterpret_cast<std::vector<HMONITOR>*>(dwData);
			vector.emplace_back(hMonitor);
			return true;
		};
	EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(&monitors));
	return monitors;
}

static inline auto get_monitor_info(HMONITOR monitor) {
	MONITORINFOEXA monitor_info{};
	bool success = GetMonitorInfoA(monitor, &monitor_info);
	if (!success) {
		throw std::runtime_error{ "GetMonitorInfo fail" };
	}
	return monitor_info;
}

auto enum_display_devices(char* adapter_name, DWORD dev_num, DISPLAY_DEVICE* display_device, DWORD flags) {
	return EnumDisplayDevicesA(adapter_name, dev_num, display_device, flags);
}

static inline auto get_display_adapters() {
	cpp_helper::small_vector<DISPLAY_DEVICEA, DWORD, 16> display_devices{};
	for (DISPLAY_DEVICEA display_device{ .cb = sizeof(display_device) }; enum_display_devices(nullptr, display_devices.size(), &display_device, 0); ) {
		display_devices.emplace_back(display_device);
	}
	return display_devices;
}
static inline auto get_display_monitors(std::string adapter_name) {
	cpp_helper::small_vector<DISPLAY_DEVICEA, DWORD, 16> display_devices{};
	for (DISPLAY_DEVICEA display_device{ .cb = sizeof(display_device) }; EnumDisplayDevicesA(adapter_name.c_str(), display_devices.size(), &display_device, 0); ) {
		display_devices.emplace_back(display_device);
	}
	return display_devices;
}

static inline auto get_display_device_modes(const char* device_name) {
	cpp_helper::small_vector<DEVMODEA, DWORD, 256> modes{};
	for (DEVMODEA mode{ .dmSize = sizeof(mode) }; EnumDisplaySettingsA(device_name, modes.size(), &mode);) {
		modes.emplace_back(mode);
	}
	return modes;
}

static inline auto get_display_device_current_mode(const char* device_name) {
	DEVMODEA device_mode{.dmSize = sizeof(device_mode)};
	bool success = EnumDisplaySettingsA(device_name, ENUM_CURRENT_SETTINGS, &device_mode);
	if (!success) {
		throw std::runtime_error{ "EnumDisplaySettings fail" };
	}
	return device_mode;
}

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


auto load_library(std::string name) {
	auto lib = LoadLibraryExA(name.c_str(), nullptr, 0);
	if (lib == NULL) {
		throw std::runtime_error{ "failed to open library" };
	}
	return lib;
}

class library {
public:
	library(std::string(name)) : m_module{ load_library(name) }
	{}

	void* get_procedure_address(std::string name) {
		auto addr =  GetProcAddress(m_module, name.c_str());
		if (addr == NULL) {
			throw std::runtime_error{ "failed to get procedure address" };
		}
		return addr;
	}
	struct no_exception {};
	std::optional<void*> get_procedure_address(std::string name, no_exception) {
		auto addr = GetProcAddress(m_module, name.c_str());
		if (addr != nullptr) {
			return addr;
		}
		else {
			return std::nullopt;
		}
	}
	~library() {
		FreeLibrary(m_module);
	}
private:
	HMODULE m_module;
};

class handle {
public:
	handle() : m_handle{INVALID_HANDLE_VALUE} {}
	handle(HANDLE h) : m_handle{h} {}
	~handle() {
		close();
	}
	handle(handle&& h) : handle{} {
		std::swap(m_handle, h.m_handle);
		h.close();
	}
	handle& operator=(handle&& h) {
		std::swap(m_handle, h.m_handle);
		h.close();
	}
	handle(const handle& h) = delete;
	handle& operator=(const handle& h) = delete;
	void close() {
		if (m_handle != INVALID_HANDLE_VALUE) {
			CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
	}
	void reset(HANDLE h) {
		close();
		m_handle = h;
	}
	auto get() {
		return m_handle;
	}
private:
	HANDLE m_handle;
};

handle create_file(std::filesystem::path path, uint32_t perms, uint32_t share_perms, uint32_t open_mode) {
	HANDLE h = CreateFileW(path.c_str(), perms, share_perms, NULL, open_mode, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		throw std::runtime_error{std::format("CreatFile failed: {}", path.string())};
	}
	return handle{h};
}

class file_mapping {
public:
  file_mapping(std::filesystem::path path) :
	hFile{create_file(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)}
  {
    hMapping.reset(CreateFileMapping(hFile.get(), NULL, PAGE_READONLY, 0, 0, NULL));
    mmaped_ptr = MapViewOfFile(hMapping.get(), FILE_MAP_READ, 0, 0, 0);
    m_size = GetFileSize(hFile.get(), NULL);
  }
  file_mapping(const file_mapping &file) = delete;
  file_mapping(file_mapping &&file)
 :  mmaped_ptr{file.mmaped_ptr},
	hMapping{std::move(file.hMapping)},
	hFile{std::move(file.hFile)},
	m_size{file.m_size}
  {
	file.mmaped_ptr = nullptr;
  }
  ~file_mapping() {
	if (mmaped_ptr != nullptr) {
		UnmapViewOfFile(mmaped_ptr);
		mmaped_ptr = nullptr;
	}
  }
  file_mapping &operator=(const file_mapping &file) = delete;
  file_mapping &operator=(file_mapping &&file) {
	mmaped_ptr = file.mmaped_ptr;
	hMapping = std::move(file.hMapping);
	hFile = std::move(file.hFile);

	file.mmaped_ptr = nullptr;
  }

  const void* data() const {
    return mmaped_ptr;
  }
  void* data() {
	return mmaped_ptr;
  }
  // file size
  size_t size() const { return m_size; }

private:
  handle hFile;
  handle hMapping;
  void *mmaped_ptr;
  size_t m_size; // count of byte
};

auto map_file(std::filesystem::path path) {
	return file_mapping{path};
}

}