#include "pch.h"
#include "PeCodeViewIdentity.hpp"
#include "SymbolDownloader.hpp"
#include <winrt/Windows.Storage.Streams.h>
namespace winrt
{
	using namespace Windows::Web::Http;
	using namespace Windows::Foundation;
}

OpenGlass::CSymbolDownloader::CSymbolDownloader() : m_client{ winrt::HttpClient{} } {}

winrt::IAsyncOperation<int> OpenGlass::CSymbolDownloader::DownloadAsync(
	const winrt::hstring& url,
	const winrt::hstring& destinationPath,
	Callback* progressCallback
) try
{
	// Notify UI that we started.
	if (progressCallback)
	{
		progressCallback(CDownloadProgress{ -1.0 });
	}

	const winrt::Uri uri{ url };
	const winrt::HttpRequestMessage request{ winrt::HttpMethod::Get(), uri };
	const winrt::HttpResponseMessage response = co_await m_client.SendRequestAsync(request, winrt::HttpCompletionOption::ResponseHeadersRead);
	response.EnsureSuccessStatusCode();

	uint64_t totalBytes = 0;
	if (auto length = response.Content().Headers().ContentLength())
	{
		totalBytes = length.Value();
	}

	const auto stream = co_await response.Content().ReadAsInputStreamAsync();
	winrt::Windows::Storage::Streams::Buffer buffer{ 128 * 1024 };
	
	if (PathFileExistsW(destinationPath.c_str()))
	{
		std::filesystem::remove_all(destinationPath.c_str());
	}
	auto file = wil::open_or_truncate_existing_file(
		destinationPath.c_str(),
		GENERIC_WRITE,
		0
	);

	// weird...
	bool completed = false;
	const auto cleanup = wil::scope_exit([&]
	{
		file.reset();
		if (!completed)
		{
			std::filesystem::remove(destinationPath.c_str());
		}
	});

	uint64_t downloaded = 0;
	for (;;)
	{
		const auto read = co_await stream.ReadAsync(buffer, buffer.Capacity(), winrt::Windows::Storage::Streams::InputStreamOptions::Partial);
		if (read.Length() == 0)
		{
			break;
		}

		DWORD written = 0;
		THROW_IF_WIN32_BOOL_FALSE(WriteFile(file.get(), read.data(), read.Length(), &written, nullptr));

		downloaded += read.Length();

		double pct = -1.0;
		if (totalBytes > 0)
		{
			pct = (static_cast<double>(downloaded) / static_cast<double>(totalBytes)) * 100.0;
		}

		if (progressCallback)
		{
			progressCallback(CDownloadProgress{
				pct,
				totalBytes,
				downloaded,
				static_cast<uint32_t>(read.Length())
			});
		}
	}

	completed = true;
	co_return S_OK;
}
catch (...)
{
	co_return wil::ResultFromCaughtException();
}

winrt::Windows::Foundation::IAsyncOperation<int> OpenGlass::DownloadSymbolForModuleAsync(
	CSymbolDownloader& downloader,
	HMODULE moduleHandle,
	LPCWSTR symbolServerBase,
	LPCWSTR destinationRoot,
	CDownloadContext** contextReceiver,
	CSymbolDownloader::Callback* progressCallback
) try
{
	PeCodeViewIdentity identity{};
	THROW_IF_FAILED(ReadLoadedPeCodeViewIdentity(moduleHandle, identity));

	WCHAR url[MAX_PATH]{};
	CDownloadContext context{ url };
	wcscpy_s(url, symbolServerBase);
	wcscat_s(url, identity.pdbName.c_str());
	wcscat_s(url, L"/");
	swprintf_s(
		url + wcslen(url),
		33,
		L"%08lX%04hX%04hX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		identity.pdbGuid.Data1,
		identity.pdbGuid.Data2,
		identity.pdbGuid.Data3,
		identity.pdbGuid.Data4[0],
		identity.pdbGuid.Data4[1],
		identity.pdbGuid.Data4[2],
		identity.pdbGuid.Data4[3],
		identity.pdbGuid.Data4[4],
		identity.pdbGuid.Data4[5],
		identity.pdbGuid.Data4[6],
		identity.pdbGuid.Data4[7]
	);
	swprintf_s(url + wcslen(url), 16, L"%x/", identity.pdbAge);
	wcscat_s(url, identity.pdbName.c_str());

	WCHAR pdbFilePath[MAX_PATH]{};
	THROW_IF_FAILED(
		PathCchCombine(
			pdbFilePath,
			MAX_PATH,
			destinationRoot,
			identity.pdbName.c_str()
		)
	);

	const auto contextScope = wil::scope_exit([contextReceiver]
	{
		if (contextReceiver)
		{
			*contextReceiver = nullptr;
		}
	});
	if (contextReceiver)
	{
		context.pdbFileName = identity.pdbName.c_str();
		context.url = url;
		*contextReceiver = &context;
	}
	const HRESULT hr = co_await downloader.DownloadAsync(url, pdbFilePath, progressCallback);
	co_return hr;
}
catch (...)
{
	co_return wil::ResultFromCaughtException();
}
