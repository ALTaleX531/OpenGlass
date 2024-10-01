// https://winclassic.net/thread/2178/loading-visual-styles-application
// special thanks to @ephemeralViolette
#include "pch.h"
#include "HookHelper.hpp"
#include "uDwmProjection.hpp"
#include "CustomMsstyleLoader.hpp"

using namespace OpenGlass;
namespace OpenGlass::CustomMsstyleLoader
{
	HRESULT WINAPI MyGetCurrentThemeName(
		LPWSTR pszThemeFileName,
		int    cchMaxNameChars,
		LPWSTR pszColorBuff,
		int    cchMaxColorChars,
		LPWSTR pszSizeBuff,
		int    cchMaxSizeChars
	);
	HTHEME WINAPI MyOpenThemeData(
		HWND hwnd,
		LPCWSTR pszClassList
	);
	HTHEME OpenActualThemeData(std::wstring_view className)
	{
		return MyOpenThemeData(nullptr, className.data());
	}
	decltype(&MyGetCurrentThemeName) g_GetCurrentThemeName_Org{ nullptr };
	decltype(&MyOpenThemeData) g_OpenThemeData_Org{ nullptr };

	struct CUxThemeFile
	{
		CHAR themeHeader[8]{ "thmfile" }; // "thmfile" or "deleted"
		wil::unique_mapview_ptr<void> sharableSectionView;
		wil::unique_handle sharableSection;
		wil::unique_mapview_ptr<DWORD> unsharableSectionView;
		wil::unique_handle unsharableSection;
		DWORD unknown1;
		DWORD unknwon2;
		CHAR themeFooter[4]{ "end" };

		bool IsValid() const
		{
			return sharableSectionView && unsharableSectionView;
		}
		~CUxThemeFile()
		{
			auto unknown = *unsharableSectionView.get();
			sharableSectionView.reset();
			unsharableSectionView.reset();
			if ((unknown & 4) != 0 && (unknown & 2) == 0)
			{
				static const auto s_ClearTheme = reinterpret_cast<HRESULT(WINAPI*)(HANDLE sharableSection, HANDLE unsharableSection, BOOL clearWhatever)>(GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(84)));
				if (s_ClearTheme) [[likely]]
				{
					LOG_IF_FAILED(s_ClearTheme(sharableSection.release(), unsharableSection.release(), FALSE));
				}
			}
			sharableSection.reset();
			unsharableSection.reset();
			strcpy_s(themeHeader, "deleted");
		}
	};
	std::unique_ptr<CUxThemeFile> g_msstyleThemeFile{};
	std::wstring g_msstyleThemePath{};
	bool g_useMsstyleDefaultScheme{ true };
}

HRESULT WINAPI CustomMsstyleLoader::MyGetCurrentThemeName(
	LPWSTR pszThemeFileName,
	int    cchMaxNameChars,
	LPWSTR pszColorBuff,
	int    cchMaxColorChars,
	LPWSTR pszSizeBuff,
	int    cchMaxSizeChars
)
{
	HRESULT hr{ g_GetCurrentThemeName_Org(pszThemeFileName, cchMaxNameChars, pszColorBuff, cchMaxColorChars, pszSizeBuff, cchMaxSizeChars) };

	if (pszThemeFileName && g_msstyleThemeFile)
	{
		wcscpy_s(pszThemeFileName, cchMaxNameChars, g_msstyleThemePath.c_str());
	}

	return hr;
}

HTHEME WINAPI CustomMsstyleLoader::MyOpenThemeData(
	HWND hwnd,
	LPCWSTR pszClassList
)
{
	if (g_msstyleThemeFile && g_msstyleThemeFile->IsValid())
	{
		static const auto s_OpenThemeDataFromFile = reinterpret_cast<HTHEME(WINAPI*)(CUxThemeFile * hThemeFile, HWND hWnd, LPCWSTR pszClassList, DWORD dwFlags)>(GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(16)));
		if (s_OpenThemeDataFromFile) [[likely]]
		{
			return s_OpenThemeDataFromFile(g_msstyleThemeFile.get(), hwnd, pszClassList, 0);
		}
	}

	return g_OpenThemeData_Org(hwnd, pszClassList);
}

void CustomMsstyleLoader::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Theme)
	{
		WCHAR msstyleThemePath[MAX_PATH + 1]{};
		ConfigurationFramework::DwmGetStringFromHKCUAndHKLM(
			L"CustomThemeMsstyle",
			msstyleThemePath
		);

		DWORD value{ ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"CustomThemeMsstyleUseDefaults", 1)};

		if (g_msstyleThemePath != msstyleThemePath || g_useMsstyleDefaultScheme != static_cast<bool>(value))
		{
			g_msstyleThemeFile.reset();
			g_msstyleThemePath = msstyleThemePath;
			g_useMsstyleDefaultScheme = static_cast<bool>(value);
			if (!g_msstyleThemePath.empty() && PathFileExistsW(g_msstyleThemePath.c_str()))
			{
				try
				{
					WCHAR themeFileName[MAX_PATH + 1]{};
					WCHAR colorSchemeName[MAX_PATH + 1]{};
					WCHAR sizeName[MAX_PATH + 1]{};

					static const auto s_GetThemeDefaults = reinterpret_cast<HRESULT(WINAPI*)(LPCWSTR pszThemeFileName, LPWSTR pszColorName, DWORD dwColorNameLen, LPWSTR pszSizeName, DWORD dwSizeNameLen)>(GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(7)));
					if (s_GetThemeDefaults && g_useMsstyleDefaultScheme)
					{
						THROW_IF_FAILED(s_GetThemeDefaults(g_msstyleThemePath.c_str(), colorSchemeName, MAX_PATH, sizeName, MAX_PATH));
					}
					else
					{
						THROW_IF_FAILED(GetCurrentThemeName(themeFileName, MAX_PATH, colorSchemeName, MAX_PATH, sizeName, MAX_PATH));
					}

					g_msstyleThemeFile = std::make_unique<CUxThemeFile>();
					static const auto s_LoaderLoadTheme = GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(92));
					if (s_LoaderLoadTheme) [[likely]]
					{
						HRESULT hr{ S_OK };
						if (os::buildNumber < os::build_w11_21h2)
						{
							hr = reinterpret_cast<
								HRESULT(WINAPI*)(
									HANDLE hThemeFile,
									HINSTANCE hInstance,
									LPCWSTR pszThemeFileName,
									LPCWSTR pszColorParam,
									LPCWSTR pszSizeParam,
									PHANDLE hSharableSection,
									LPWSTR pszSharableSectionName,
									int cchSharableSectionName,
									PHANDLE hNonsharableSection,
									LPWSTR pszNonsharableSectionName,
									int cchNonsharableSectionName,
									PVOID pfnCustomLoadHandler,
									PHANDLE hReuseSection,
									int unknown1,
									int unknown2,
									BOOL fEmulateGlobal
								)
							>(s_LoaderLoadTheme)(
								nullptr,
								nullptr,
								g_msstyleThemePath.c_str(),
								colorSchemeName,
								sizeName,
								g_msstyleThemeFile->sharableSection.put(),
								nullptr,
								0,
								g_msstyleThemeFile->unsharableSection.put(),
								nullptr,
								0,
								nullptr,
								nullptr,
								0,
								0,
								FALSE
								);
						}
						else
						{
							hr = reinterpret_cast<
								HRESULT(WINAPI*)(
									HANDLE hThemeFile,
									HINSTANCE hInstance,
									LPCWSTR pszThemeFileName,
									LPCWSTR pszColorParam,
									LPCWSTR pszSizeParam,
									PHANDLE hSharableSection,
									LPWSTR pszSharableSectionName,
									int cchSharableSectionName,
									PHANDLE hNonsharableSection,
									LPWSTR pszNonsharableSectionName,
									int cchNonsharableSectionName,
									PVOID pfnCustomLoadHandler,
									PHANDLE hReuseSection,
									int unknown,
									BOOL fEmulateGlobal
								)
							>(s_LoaderLoadTheme)(
								nullptr,
								nullptr,
								g_msstyleThemePath.c_str(),
								colorSchemeName,
								sizeName,
								g_msstyleThemeFile->sharableSection.put(),
								nullptr,
								0,
								g_msstyleThemeFile->unsharableSection.put(),
								nullptr,
								0,
								nullptr,
								nullptr,
								0,
								FALSE
							);
						}
						THROW_IF_FAILED(hr);
						g_msstyleThemeFile->sharableSectionView.reset(
							MapViewOfFile3(
								g_msstyleThemeFile->sharableSection.get(),
								GetCurrentProcess(),
								nullptr,
								0,
								0,
								0,
								FILE_MAP_READ,
								nullptr,
								0
							)
						);
						g_msstyleThemeFile->unsharableSectionView.reset(
							reinterpret_cast<DWORD*>(
								MapViewOfFile3(
									g_msstyleThemeFile->unsharableSection.get(),
									GetCurrentProcess(),
									nullptr,
									0,
									0,
									0,
									FILE_MAP_READ,
									nullptr,
									0
								)
							)
						);
					}
				}
				CATCH_LOG()
			}
		}
	}
}

HRESULT CustomMsstyleLoader::Startup()
{
	if (os::buildNumber < os::build_w10_2004)
	{
		g_GetCurrentThemeName_Org = reinterpret_cast<decltype(g_GetCurrentThemeName_Org)>(HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "uxtheme.dll", "GetCurrentThemeName", MyGetCurrentThemeName).second);
		g_OpenThemeData_Org = reinterpret_cast<decltype(g_OpenThemeData_Org)>(HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-0.dll", "OpenThemeData", MyOpenThemeData).second);
	}
	else
	{
		g_GetCurrentThemeName_Org = reinterpret_cast<decltype(g_GetCurrentThemeName_Org)>(HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-2.dll", "GetCurrentThemeName", MyGetCurrentThemeName).second);
		g_OpenThemeData_Org = reinterpret_cast<decltype(g_OpenThemeData_Org)>(HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-0.dll", "OpenThemeData", MyOpenThemeData).second);
	}
	
	return S_OK;
}
void CustomMsstyleLoader::Shutdown()
{
	if (os::buildNumber < os::build_w10_2004)
	{
		HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "uxtheme.dll", "GetCurrentThemeName", g_GetCurrentThemeName_Org);
		HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-0.dll", "OpenThemeData", g_OpenThemeData_Org);
	}
	else
	{
		HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-2.dll", "GetCurrentThemeName", g_GetCurrentThemeName_Org);
		HookHelper::WriteDelayloadIAT(uDwm::g_moduleHandle, "ext-ms-win-uxtheme-themes-l1-1-0.dll", "OpenThemeData", g_OpenThemeData_Org);
	}
	
	g_msstyleThemeFile.reset();
	g_msstyleThemePath.clear();
}