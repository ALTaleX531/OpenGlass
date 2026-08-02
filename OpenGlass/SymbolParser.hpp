#pragma once
#include "HookHelper.hpp"
#include "pch.h"

namespace OpenGlass
{
	namespace Projection
	{
		class ModuleRegistry;
	}

	class CSymbolParser
	{
		struct EnumerationContext;
		static BOOL CALLBACK EnumProjectionSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext);

	public:
		CSymbolParser(LPCWSTR symbolsPath);
		~CSymbolParser() noexcept;

		HRESULT ParsePdb(HMODULE moduleHandle, Projection::ModuleRegistry& registry);
	};
} // namespace OpenGlass
