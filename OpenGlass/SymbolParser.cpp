#include "pch.h"
#include "Utils.hpp"
#include "HookHelper.hpp"
#include "SymbolParser.hpp"

using namespace OpenGlass;

BOOL CALLBACK SymbolParser::SymCallback(
	HANDLE /*hProcess*/,
	ULONG ActionCode,
	ULONG64 CallbackData,
	ULONG64 UserContext
)
{
	if (ActionCode == CBA_EVENT)
	{
		if (UserContext)
		{
			auto& symbolResolver = *reinterpret_cast<SymbolParser*>(UserContext);
			auto event = reinterpret_cast<PIMAGEHLP_CBA_EVENTW>(CallbackData);

			if (wcsstr(event->desc, L"from http://"))
			{
				symbolResolver.m_downloading = true;
				symbolResolver.m_downloadNotifyCallback(SymbolDownloaderStatus::Start, symbolResolver.m_currentModule);
			}
			if (symbolResolver.m_downloading)
			{
				symbolResolver.m_downloadNotifyCallback(SymbolDownloaderStatus::Downloading, event->desc);
			}
			if (wcsstr(event->desc, L"copied"))
			{
				symbolResolver.m_downloading = false;
				symbolResolver.m_downloadNotifyCallback(SymbolDownloaderStatus::OK, L"");
			}
		}

		return TRUE;
	}
	return FALSE;
}

HMODULE WINAPI SymbolParser::MyLoadLibraryExW(
	LPCWSTR lpLibFileName,
	HANDLE hFile,
	DWORD dwFlags
)
{
	static const auto s_symsrvSysFullPath = wil::GetSystemDirectoryW<std::wstring, MAX_PATH + 1>() + L"\\symsrv.dll";
	static const auto s_symsrvCurFullPath = Utils::make_current_folder_file_wstring(L"symsrv.dll");
	if (
		!_wcsicmp(lpLibFileName, s_symsrvSysFullPath.c_str())
	)
	{
		return LoadLibraryW(s_symsrvCurFullPath.c_str());
	}

	return LoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

SymbolParser::SymbolParser()
{
	try
	{
		m_LoadLibraryExW_Org = HookHelper::WriteIAT(GetModuleHandleW(L"dbghelp.dll"), "api-ms-win-core-libraryloader-l1-1-0.dll", "LoadLibraryExW", MyLoadLibraryExW);
		THROW_IF_WIN32_BOOL_FALSE(SymInitialize(GetCurrentProcess(), nullptr, FALSE));

		SymSetOptions(SYMOPT_DEFERRED_LOADS);
		THROW_IF_WIN32_BOOL_FALSE(SymRegisterCallbackW64(GetCurrentProcess(), SymCallback, reinterpret_cast<ULONG64>(this)));

		WCHAR curDir[MAX_PATH + 1]{};
		THROW_LAST_ERROR_IF(GetModuleFileName(wil::GetModuleInstanceHandle(), curDir, MAX_PATH) == 0);
		THROW_IF_FAILED(PathCchRemoveFileSpec(curDir, MAX_PATH));

		const auto symPath = std::wstring{ L"SRV*" } + curDir + L"\\symbols";
		THROW_IF_WIN32_BOOL_FALSE(SymSetSearchPathW(GetCurrentProcess(), symPath.c_str()));
	}
	catch (...)
	{
		m_lastErr = wil::ResultFromCaughtException();
		SymCleanup(GetCurrentProcess());
	}
}

SymbolParser::~SymbolParser() noexcept
{
	SymCleanup(GetCurrentProcess());
	m_LoadLibraryExW_Org = HookHelper::WriteIAT(GetModuleHandleW(L"dbghelp.dll"), "api-ms-win-core-libraryloader-l1-1-0.dll", "LoadLibraryExW", m_LoadLibraryExW_Org);
}

HRESULT SymbolParser::Walk(
	std::wstring_view dllName,
	const SymbolDownloaderCallback& downloadNotifyCallback,
	const SymbolParserCallback& enumCallback
) try
{
	DWORD64 dllBase{ 0 };
	WCHAR filePath[MAX_PATH + 1]{}, symFile[MAX_PATH + 1]{};
	MODULEINFO modInfo{};

	auto symCleanUp = wil::scope_exit([&]
	{
		if (dllBase != 0)
		{
			SymUnloadModule64(GetCurrentProcess(), dllBase);
			dllBase = 0;
		}
	});

	THROW_HR_IF(E_INVALIDARG, dllName.empty());

	wil::unique_hmodule moduleHandle{ LoadLibraryExW(dllName.data(), nullptr, DONT_RESOLVE_DLL_REFERENCES) };
	THROW_LAST_ERROR_IF_NULL(moduleHandle);
	THROW_LAST_ERROR_IF(GetModuleFileNameW(moduleHandle.get(), filePath, MAX_PATH) == 0);
	THROW_IF_WIN32_BOOL_FALSE(GetModuleInformation(GetCurrentProcess(), moduleHandle.get(), &modInfo, sizeof(modInfo)));

	if (SymGetSymbolFileW(GetCurrentProcess(), nullptr, filePath, sfPdb, symFile, MAX_PATH, symFile, MAX_PATH) == FALSE)
	{
		DWORD lastError{ GetLastError() };
		THROW_WIN32_IF(lastError, lastError != ERROR_FILE_NOT_FOUND);

		WCHAR curDir[MAX_PATH + 1]{};
		THROW_LAST_ERROR_IF(GetModuleFileName(wil::GetModuleInstanceHandle(), curDir, MAX_PATH) == 0);
		THROW_IF_FAILED(PathCchRemoveFileSpec(curDir, MAX_PATH));

		const auto symPath = std::wstring{ L"SRV*" } + curDir + L"\\symbols*http://msdl.microsoft.com/download/symbols";

		DWORD options = SymSetOptions(SymGetOptions() | SYMOPT_DEBUG);

		auto cleanUp = wil::scope_exit([&]
		{
			SymSetOptions(options);
			m_downloadNotifyCallback = nullptr;
		});

		m_downloadNotifyCallback = downloadNotifyCallback;
		m_currentModule = dllName;
		THROW_IF_WIN32_BOOL_FALSE(SymGetSymbolFileW(GetCurrentProcess(), symPath.c_str(), filePath, sfPdb, symFile, MAX_PATH, symFile, MAX_PATH));

	}

	dllBase = SymLoadModuleExW(GetCurrentProcess(), nullptr, filePath, nullptr, reinterpret_cast<DWORD64>(modInfo.lpBaseOfDll), modInfo.SizeOfImage, nullptr, 0);
	THROW_LAST_ERROR_IF(dllBase == 0);
	THROW_IF_WIN32_BOOL_FALSE(SymEnumSymbols(GetCurrentProcess(), dllBase, nullptr, SymbolParser::EnumSymbolsCallback, (const PVOID)(&enumCallback)));

	return S_OK;
}
CATCH_LOG_RETURN_HR(wil::ResultFromCaughtException())

BOOL SymbolParser::EnumSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG /*SymbolSize*/, PVOID UserContext)
{
	auto& callback{ *reinterpret_cast<SymbolParserCallback*>(UserContext) };

	if (callback)
	{
		std::string_view functionName{ reinterpret_cast<const CHAR*>(pSymInfo->Name), pSymInfo->NameLen };
		CHAR fullyUnDecoratedFunctionName[MAX_PATH + 1]{};
		UnDecorateSymbolName(
			functionName.data(), fullyUnDecoratedFunctionName, MAX_PATH,
			UNDNAME_NAME_ONLY
		);

		return static_cast<BOOL>(callback(functionName, fullyUnDecoratedFunctionName, HookHelper::OffsetStorage::From(pSymInfo->ModBase, pSymInfo->Address), pSymInfo));
	}

	return TRUE;
}