#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::Utils
{
	inline HANDLE g_processHeap{ GetProcessHeap() };

	FORCEINLINE auto to_error_wstring(HRESULT hr)
	{
		return winrt::hresult_error{ hr }.message();
	}

	template <typename T>
	FORCEINLINE auto cast_pointer(PVOID ptr)
	{
		T target_ptr{ nullptr };
		*reinterpret_cast<PVOID*>(&target_ptr) = ptr;
		return target_ptr;
	}
	
	template <UINT id>
	FORCEINLINE const std::wstring_view GetResWStringView()
	{
		LPCWSTR buffer{ nullptr };
		auto length = LoadStringW(wil::GetModuleInstanceHandle(), id, reinterpret_cast<LPWSTR>(&buffer), 0);
		return { buffer, static_cast<size_t>(length) };
	}
	template <UINT id>
	FORCEINLINE std::wstring GetResWString()
	{
		LPCWSTR buffer{ nullptr };
		auto length = LoadStringW(wil::GetModuleInstanceHandle(), id, reinterpret_cast<LPWSTR>(&buffer), 0);
		return { buffer, static_cast<size_t>(length) };
	}

	static std::wstring make_current_folder_file_wstring(std::wstring_view baseFileName)
	{
		static const auto s_current_module_path = []() -> std::wstring
		{
			WCHAR filePath[MAX_PATH + 1]{};
			GetModuleFileNameW(wil::GetModuleInstanceHandle(), filePath, _countof(filePath));
			return std::wstring{ filePath };
		}();

		WCHAR filePath[MAX_PATH + 1]{ L"" };
		[&]()
			{
				wcscpy_s(filePath, s_current_module_path.c_str());
				RETURN_IF_FAILED(PathCchRemoveFileSpec(filePath, _countof(filePath)));
				RETURN_IF_FAILED(PathCchAppend(filePath, _countof(filePath), baseFileName.data()));
				return S_OK;
			}
		();

		return std::wstring{ filePath };
	}

	// type = 1, input desktop (grpdeskRitInput)
	// type = 2, default desktop? (grpdeskIODefault)
	// type = 3, unknown
	// type = 4, winlogon desktop (grpdeskLogon)
	// type = ..., desktop created by CreateDesktop?
	FORCEINLINE bool WINAPI GetDesktopID(ULONG_PTR type, ULONG_PTR* desktopID)
	{
		static const auto pfnGetDesktopID = reinterpret_cast<decltype(&GetDesktopID)>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDesktopID"));
		if (pfnGetDesktopID) [[likely]]
		{
			return pfnGetDesktopID(type, desktopID);
		}

		return false;
	}

	FORCEINLINE bool WINAPI IsBatterySaverEnabled()
	{
		SYSTEM_POWER_STATUS powerStatus{};
		return GetSystemPowerStatus(&powerStatus) && powerStatus.SystemStatusFlag;
	}

	FORCEINLINE bool WINAPI IsTransparencyEnabled()
	{
		DWORD value{ 0 };
		LOG_IF_FAILED(wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency", &value));
		return static_cast<bool>(value);
	}
	FORCEINLINE bool WINAPI IsTransparencyEnabled(HKEY key)
	{
		DWORD value{ 0 };
		LOG_IF_FAILED(wil::reg::get_value_dword_nothrow(key, L"EnableTransparency", &value));
		return static_cast<bool>(value);
	}

	FORCEINLINE bool WINAPI IsRunAsLocalSystem()
	{
		BOOL isLocalSystem{ FALSE };
		wil::unique_sid sid{ nullptr };
		SID_IDENTIFIER_AUTHORITY authority{ SECURITY_NT_AUTHORITY };
		if (AllocateAndInitializeSid(&authority, 1ul, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &sid))
		{
			CheckTokenMembership(0, sid.get(), &isLocalSystem);
		}
		return static_cast<bool>(isLocalSystem);
	}

	FORCEINLINE D2D1_COLOR_F FromAbgr(DWORD color)
	{
		auto abgr = reinterpret_cast<const UCHAR*>(&color);
		return
		{
			static_cast<float>(abgr[0]) / 255.f,
			static_cast<float>(abgr[1]) / 255.f,
			static_cast<float>(abgr[2]) / 255.f,
			static_cast<float>(abgr[3]) / 255.f
		};
	}
	FORCEINLINE D2D1_COLOR_F FromArgb(DWORD color)
	{
		auto argb = reinterpret_cast<const UCHAR*>(&color);
		return
		{
			static_cast<float>(argb[2]) / 255.f,
			static_cast<float>(argb[1]) / 255.f,
			static_cast<float>(argb[0]) / 255.f,
			static_cast<float>(argb[3]) / 255.f
		};

		/*
		float r;
		float g;
		float b;
		float a;
		*/
	}
}

#define DEFINE_INVOKER(fn) static const auto s_fn_ptr = Utils::cast_pointer<decltype(&fn)>(g_symbolMap.at(#fn))
#define DEFINE_USER_INVOKER(type, name) static const auto s_fn_ptr = Utils::cast_pointer<decltype(&type)>(g_symbolMap.at(name))
#define DEFINE_CUSTOM_INVOKER(type, name) static const auto s_fn_ptr = Utils::cast_pointer<type>(g_symbolMap.at(name))
#define INVOKE_MEMBERFUNCTION(...) std::invoke(s_fn_ptr, this, ##__VA_ARGS__)
#define INVOKE_FUNCTION(...) std::invoke(s_fn_ptr, ##__VA_ARGS__)