#include "pch.h"
#include "SymbolParser.hpp"
#include "HookHelper.hpp"
#include "ProjectionHelper.hpp"
#include "Util.hpp"

using namespace OpenGlass;

namespace
{
	constexpr DWORD CompleteSymbolNameCapacity{64 * 1024};
}

#ifndef _DEBUG
#pragma optimize("s", on)
#endif

struct CSymbolParser::EnumerationContext
{
	Projection::ModuleRegistry& registry;
	std::unique_ptr<CHAR[]> scratch;
};

BOOL CALLBACK CSymbolParser::EnumProjectionSymbolsCallback(PSYMBOL_INFO pSymInfo, [[maybe_unused]] ULONG SymbolSize,
														   PVOID UserContext)
{
	auto& context = *static_cast<EnumerationContext*>(UserContext);
	const auto length = UnDecorateSymbolName(
		pSymInfo->Name,
		context.scratch.get(),
		CompleteSymbolNameCapacity,
		UNDNAME_COMPLETE
	);
	if (!length || length >= CompleteSymbolNameCapacity)
	{
		context.registry.RecordUndecorationFailure();
		return TRUE;
	}
	context.registry.Collect(context.scratch.get(), reinterpret_cast<PVOID>(pSymInfo->Address));
	return TRUE;
}

CSymbolParser::CSymbolParser(LPCWSTR symbolsPath)
{
	THROW_IF_WIN32_BOOL_FALSE(SymInitializeW(GetCurrentProcess(), symbolsPath, FALSE));

	// SYMOPT_PUBLICS_ONLY keeps enumeration on decorated public records so each
	// candidate can be normalized once with UNDNAME_COMPLETE.
	SymSetOptions(SYMOPT_PUBLICS_ONLY | SYMOPT_EXACT_SYMBOLS | SYMOPT_IGNORE_NT_SYMPATH);
}

CSymbolParser::~CSymbolParser() noexcept
{
	THROW_IF_WIN32_BOOL_FALSE(SymCleanup(GetCurrentProcess()));
}

HRESULT CSymbolParser::ParsePdb(HMODULE moduleHandle, Projection::ModuleRegistry& registry)
try
{
	registry.ResetSymbols();

	CHAR modulePath[MAX_PATH]{};
	THROW_LAST_ERROR_IF(GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH) == 0);

	DWORD64 moduleBase = 0;
	MODULEINFO moduleInfo{};
	THROW_IF_WIN32_BOOL_FALSE(GetModuleInformation(GetCurrentProcess(), moduleHandle, &moduleInfo, sizeof(moduleInfo)));
	moduleBase = SymLoadModuleEx(GetCurrentProcess(), nullptr, modulePath, nullptr,
								 reinterpret_cast<DWORD64>(moduleInfo.lpBaseOfDll), moduleInfo.SizeOfImage, nullptr, 0);
	THROW_LAST_ERROR_IF(moduleBase == 0);

	const auto symCleanUp =
		wil::scope_exit([=] { LOG_IF_WIN32_BOOL_FALSE(SymUnloadModule64(GetCurrentProcess(), moduleBase)); });

	IMAGEHLP_MODULEW64 symbolModuleInfo{sizeof(symbolModuleInfo)};
	THROW_IF_WIN32_BOOL_FALSE(SymGetModuleInfoW64(GetCurrentProcess(), moduleBase, &symbolModuleInfo));
	THROW_WIN32_IF_MSG(ERROR_FILE_NOT_FOUND, symbolModuleInfo.LoadedPdbName[0] == L'\0',
					   "Symbols NOT loaded for module %p", moduleHandle);
	THROW_WIN32_IF_MSG(ERROR_FILE_CORRUPT, (symbolModuleInfo.SymType & 0xFFFFFFFB) == 0,
					   "Invalid symbol type %d for module %p", symbolModuleInfo.SymType, moduleHandle);

	EnumerationContext context{registry, std::make_unique_for_overwrite<CHAR[]>(CompleteSymbolNameCapacity)};
	THROW_IF_WIN32_BOOL_FALSE(
		SymEnumSymbols(GetCurrentProcess(), moduleBase, nullptr, EnumProjectionSymbolsCallback, &context));
	return S_OK;
}
CATCH_RETURN()
