#pragma once

#include "pch.h"

namespace OpenGlass
{
	inline constexpr int SymbolDownloadTimeoutSeconds = 5;

	enum class SymbolDownloadResult
	{
		Success,
		Cancelled,
		Failed,
	};

	struct SymbolDownloadProgress
	{
		int percent{ 0 };
		bool indeterminate{ true };
		std::wstring phase;
		std::wstring detail;
	};

	struct SymbolDownloadOutcome
	{
		SymbolDownloadResult result{ SymbolDownloadResult::Failed };
		HRESULT hr{ S_OK };
		std::wstring summary;
		std::wstring details;
		std::wstring symbolDirectory;
	};

	using SymbolDownloadProgressCallback = std::function<void(const SymbolDownloadProgress&)>;

	[[nodiscard]] std::wstring GetSymbolCacheDirectory();
	[[nodiscard]] SymbolDownloadOutcome DownloadSymbols(std::wstring symbolDirectory, const std::stop_token& stopToken, const SymbolDownloadProgressCallback& progressCallback = {});
}
