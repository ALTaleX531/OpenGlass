#pragma once
#include "pch.h"
#include "HookHelper.hpp"

namespace OpenGlass
{
	enum class SymbolDownloaderStatus : UCHAR
	{
		None,
		Start,
		Downloading,
		OK
	};
	using SymbolDownloaderCallback = std::function<void(SymbolDownloaderStatus status, std::wstring_view text)>;
	using SymbolParserCallback = std::function<bool(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset, const PSYMBOL_INFO originalSymInfo)>;

	class SymbolParser
	{
		static HMODULE WINAPI MyLoadLibraryExW(
			LPCWSTR lpLibFileName,
			HANDLE hFile,
			DWORD dwFlags
		);
		static BOOL CALLBACK EnumSymbolsCallback(
			PSYMBOL_INFO pSymInfo,
			ULONG SymbolSize,
			PVOID UserContext
		);
		static BOOL CALLBACK SymCallback(
			HANDLE hProcess,
			ULONG ActionCode,
			ULONG64 CallbackData,
			ULONG64 UserContext
		);

		bool m_downloading{ false };
		HRESULT m_lastErr{ S_OK };
		PVOID m_LoadLibraryExW_Org{ nullptr };
		SymbolDownloaderCallback m_downloadNotifyCallback{ nullptr };
	public:
		SymbolParser();
		~SymbolParser() noexcept;

		operator HRESULT() const { return m_lastErr; }
		HRESULT Walk(
			std::wstring_view dllName, 
			const SymbolDownloaderCallback& downloadNotifyCallback,
			const SymbolParserCallback& enumCallback
		);
	};
}