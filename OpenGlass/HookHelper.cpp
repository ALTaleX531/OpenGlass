#include "pch.h"
#include "HookHelper.hpp"
#include "detours.h"

using namespace OpenGlass;
namespace OpenGlass::HookHelper::Detours
{
	// Begin to install hooks
	HRESULT Begin();
	// End attaching
	HRESULT End(bool commit = true);
}

HookHelper::ThreadSnapshot::ThreadSnapshot()
{
	if (PssCaptureSnapshot(GetCurrentProcess(), PSS_CAPTURE_THREADS, 0, &m_snapshot))
	{
		return;
	}
}

HookHelper::ThreadSnapshot::~ThreadSnapshot()
{
	if (m_snapshot)
	{
		PssFreeSnapshot(GetCurrentProcess(), m_snapshot);
		m_snapshot = nullptr;
	}
}

void HookHelper::ThreadSnapshot::Walk(const std::function<bool(const PSS_THREAD_ENTRY&)>&& callback)
{
	HPSSWALK walk{ nullptr };
	if (PssWalkMarkerCreate(nullptr, &walk))
	{
		return;
	}
	auto cleanUp = [&]
	{
		if (walk)
		{
			PssWalkMarkerFree(walk);
			walk = nullptr;
		}
	};

	PSS_THREAD_ENTRY threadEntry{};
	while (!PssWalkSnapshot(m_snapshot, PSS_WALK_THREADS, walk, &threadEntry, sizeof(threadEntry)))
	{
		if (threadEntry.ThreadId != GetCurrentThreadId())
		{
			if (!callback(threadEntry))
			{
				return;
			}
		}
	}
}

PVOID HookHelper::WritePointerInternal(PVOID* memoryAddress, PVOID value) try
{
	THROW_HR_IF_NULL(E_INVALIDARG, memoryAddress);

	DWORD oldProtect{0};
	THROW_IF_WIN32_BOOL_FALSE(
		VirtualProtect(
			memoryAddress,
			sizeof(value),
			PAGE_EXECUTE_READWRITE,
			&oldProtect
		)
	);
	PVOID oldValue{ InterlockedExchangePointer(memoryAddress, value) };
	THROW_IF_WIN32_BOOL_FALSE(
		VirtualProtect(
			memoryAddress,
			sizeof(value),
			oldProtect,
			&oldProtect
		)
	);

	return oldValue;
}
catch (...) { return nullptr; }

HMODULE HookHelper::GetProcessModule(HANDLE processHandle, std::wstring_view dllPath)
{
	HMODULE targetModule{ nullptr };
	DWORD bytesNeeded{ 0 };
	if (!EnumProcessModules(processHandle, nullptr, 0, &bytesNeeded))
	{
		return targetModule;
	}
	DWORD moduleCount{ bytesNeeded / sizeof(HMODULE) };
	auto moduleList = std::make_unique<HMODULE[]>(moduleCount);
	if (!EnumProcessModules(processHandle, moduleList.get(), bytesNeeded, &bytesNeeded))
	{
		return targetModule;
	}

	for (DWORD i = 0; i < moduleCount; i++)
	{
		HMODULE moduleHandle{ moduleList[i] };
		WCHAR modulePath[MAX_PATH + 1];
		GetModuleFileNameExW(processHandle, moduleHandle, modulePath, MAX_PATH);

		if (!_wcsicmp(modulePath, dllPath.data()))
		{
			targetModule = moduleHandle;
			break;
		}
	}

	return targetModule;
}

void HookHelper::WalkIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName)> callback) try
{
	THROW_HR_IF(E_INVALIDARG, dllName.empty());
	THROW_HR_IF_NULL(E_INVALIDARG, baseAddress);
	THROW_HR_IF_NULL(E_INVALIDARG, callback);

	ULONG size{ 0ul };
	auto importDescriptor = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(
		ImageDirectoryEntryToData(
			baseAddress,
			TRUE,
			IMAGE_DIRECTORY_ENTRY_IMPORT,
			&size
		)
	);

	THROW_HR_IF_NULL(E_INVALIDARG, importDescriptor);

	bool found = false;
	while (importDescriptor->Name)
	{
		auto moduleName = reinterpret_cast<LPCSTR>(reinterpret_cast<UINT_PTR>(baseAddress) + importDescriptor->Name);

		if (!_stricmp(moduleName, dllName.data()))
		{
			found = true;
			break;
		}

		importDescriptor++;
	}

	THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !found);

	auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<UINT_PTR>(baseAddress) + importDescriptor->FirstThunk);
	auto nameThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<UINT_PTR>(baseAddress) + importDescriptor->OriginalFirstThunk);

	bool result{ true };
	while (thunk->u1.Function)
	{
		LPCSTR functionName{ nullptr };
		auto functionAddress = reinterpret_cast<PVOID*>(&thunk->u1.Function);

		bool importedByName{ !IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal) };
		if (importedByName)
		{
			functionName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
				RVA_TO_ADDR(baseAddress, static_cast<RVA>(nameThunk->u1.AddressOfData))
				)->Name;
		}
		else
		{
			functionName = MAKEINTRESOURCEA(IMAGE_ORDINAL(nameThunk->u1.Ordinal));
		}

		result = callback(functionAddress, functionName, importedByName);
		if (!result)
		{
			break;
		}

		thunk++;
		nameThunk++;
	}
}
catch (...) {}

void HookHelper::WalkDelayloadIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(HMODULE* moduleHandle, PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName)> callback) try
{
	THROW_HR_IF(E_INVALIDARG, dllName.empty());
	THROW_HR_IF_NULL(E_INVALIDARG, baseAddress);
	THROW_HR_IF_NULL(E_INVALIDARG, callback);

	ULONG size{ 0ul };
	auto importDescriptor = static_cast<PIMAGE_DELAYLOAD_DESCRIPTOR>(
		ImageDirectoryEntryToData(
			baseAddress,
			TRUE,
			IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT,
			&size
		)
	);

	THROW_HR_IF_NULL(E_INVALIDARG, importDescriptor);

	bool found = false;
	while (importDescriptor->DllNameRVA)
	{
		auto moduleName = reinterpret_cast<LPCSTR>(
			RVA_TO_ADDR(baseAddress, importDescriptor->DllNameRVA)
			);

		if (!_stricmp(moduleName, dllName.data()))
		{
			found = true;
			break;
		}

		importDescriptor++;
	}

	THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !found);

	auto attributes = importDescriptor->Attributes.RvaBased;
	THROW_WIN32_IF_MSG(ERROR_FILE_NOT_FOUND, attributes != 1, "Unsupported delay loaded dll![%hs]", dllName.data());

	auto moduleHandle = reinterpret_cast<HMODULE*>(
		RVA_TO_ADDR(baseAddress, importDescriptor->ModuleHandleRVA)
		);
	auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
		RVA_TO_ADDR(baseAddress, importDescriptor->ImportAddressTableRVA)
		);
	auto nameThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
		RVA_TO_ADDR(baseAddress, importDescriptor->ImportNameTableRVA)
		);

	bool result{ true };
	while (thunk->u1.Function)
	{
		LPCSTR functionName{ nullptr };
		auto functionAddress = reinterpret_cast<PVOID*>(&thunk->u1.Function);

		bool importedByName{ !IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal) };
		if (importedByName)
		{
			functionName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
				RVA_TO_ADDR(baseAddress, nameThunk->u1.AddressOfData)
				)->Name;
		}
		else
		{
			functionName = MAKEINTRESOURCEA(IMAGE_ORDINAL(nameThunk->u1.Ordinal));
		}

		result = callback(moduleHandle, functionAddress, functionName, importedByName);
		if (!result)
		{
			break;
		}

		thunk++;
		nameThunk++;
	}
}
catch (...) {}

PVOID* HookHelper::GetIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal)
{
	PVOID* originalFunction{ nullptr };
	WalkIAT(baseAddress, dllName, [&](PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName) -> bool
	{
		if (
			(importedByName == TRUE && targetFunctionNameOrOrdinal && !strcmp(functionNameOrOrdinal, targetFunctionNameOrOrdinal)) ||
			(importedByName == FALSE && functionNameOrOrdinal == targetFunctionNameOrOrdinal)
		)
		{
			originalFunction = functionAddress;

			return false;
		}

		return true;
	});

	return originalFunction;
}
std::pair<HMODULE*, PVOID*> HookHelper::GetDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, bool resolveAPI)
{
	std::pair<HMODULE*, PVOID*> originalInfo{ nullptr, nullptr };
	WalkDelayloadIAT(baseAddress, dllName, [&](HMODULE* moduleHandle, PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName) -> bool
	{
		if (
			(importedByName == TRUE && targetFunctionNameOrOrdinal && (reinterpret_cast<DWORD64>(targetFunctionNameOrOrdinal) & 0xFFFF0000) != 0 && !strcmp(functionNameOrOrdinal, targetFunctionNameOrOrdinal)) ||
			(importedByName == FALSE && functionNameOrOrdinal == targetFunctionNameOrOrdinal)
		)
		{
			originalInfo.first = moduleHandle;
			originalInfo.second = functionAddress;

			if (resolveAPI)
			{
				ResolveDelayloadIAT(originalInfo, baseAddress, dllName, targetFunctionNameOrOrdinal);
			}
			return false;
		}

		return true;
	});

	return originalInfo;
}

PVOID HookHelper::WriteIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction)
{
	PVOID originalFunction{ nullptr };

	auto functionAddress = GetIAT(baseAddress, dllName, targetFunctionNameOrOrdinal);
	if (functionAddress)
	{
		originalFunction = *functionAddress;

		WritePointer(functionAddress, detourFunction);
	}

	return originalFunction;
}
std::pair<HMODULE, PVOID> HookHelper::WriteDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction, std::optional<HMODULE> newModuleHandle)
{
	HMODULE originalModule{ nullptr };
	PVOID originalFunction{ nullptr };

	auto [moduleHandle, functionAddress] = GetDelayloadIAT(baseAddress, dllName, targetFunctionNameOrOrdinal, true);
	if (functionAddress)
	{
		originalModule = *moduleHandle;
		originalFunction = *functionAddress;

		if (newModuleHandle)
		{
			WritePointer(moduleHandle, newModuleHandle.value());
		}
		WritePointer(functionAddress, detourFunction);
	}

	return std::make_pair(originalModule, originalFunction);
}

void HookHelper::ResolveDelayloadIAT(const std::pair<HMODULE*, PVOID*>& info, PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal)
{
	auto& [moduleHandle, functionAddress] {info};
	if (DetourGetContainingModule(*functionAddress) == baseAddress || DetourGetContainingModule(DetourCodeFromPointer(*functionAddress, nullptr)) == baseAddress)
	{
		if (!(*moduleHandle))
		{
			HMODULE importModule{ LoadLibraryA(dllName.data()) };

			if (importModule)
			{
				WritePointer(moduleHandle, importModule);
				WritePointer(functionAddress, GetProcAddress(importModule, targetFunctionNameOrOrdinal));
			}
		}
		else
		{
			WritePointer(functionAddress, GetProcAddress(*moduleHandle, targetFunctionNameOrOrdinal));
		}
	}
}

HRESULT HookHelper::Detours::Begin()
{
	DetourSetIgnoreTooSmall(TRUE);
	RETURN_IF_WIN32_ERROR(DetourTransactionBegin());
	RETURN_IF_WIN32_ERROR(DetourUpdateThread(GetCurrentThread()));
	return S_OK;
}

HRESULT HookHelper::Detours::End(bool commit)
{
	return (commit ? HRESULT_FROM_WIN32(DetourTransactionCommit()) : HRESULT_FROM_WIN32(DetourTransactionAbort()));
}

HRESULT HookHelper::Detours::Write(const std::function<void()>&& callback) try
{
	HRESULT hr{ HookHelper::Detours::Begin() };
	if (FAILED(hr))
	{
		return hr;
	}

	callback();

	return HookHelper::Detours::End(true);
}
catch (...)
{
	LOG_CAUGHT_EXCEPTION();
	LOG_IF_FAILED(HookHelper::Detours::End(false));
	return wil::ResultFromCaughtException();
}

void HookHelper::Detours::Attach(std::string_view dllName, std::string_view funcName, PVOID* realFuncAddr, PVOID hookFuncAddr)
{
	THROW_HR_IF_NULL(E_INVALIDARG, realFuncAddr);
	THROW_HR_IF_NULL(E_INVALIDARG, *realFuncAddr);
	*realFuncAddr = DetourFindFunction(dllName.data(), funcName.data());
	THROW_LAST_ERROR_IF_NULL(*realFuncAddr);

	Attach(realFuncAddr, hookFuncAddr);
}

void HookHelper::Detours::Attach(PVOID* realFuncAddr, PVOID hookFuncAddr)
{
	THROW_HR_IF_NULL(E_INVALIDARG, realFuncAddr);
	THROW_HR_IF_NULL(E_INVALIDARG, *realFuncAddr);
	THROW_HR_IF_NULL(E_INVALIDARG, hookFuncAddr);

	THROW_IF_WIN32_ERROR(DetourAttach(realFuncAddr, hookFuncAddr));
}

void HookHelper::Detours::Detach(PVOID* realFuncAddr, PVOID hookFuncAddr)
{
	THROW_HR_IF_NULL(E_INVALIDARG, realFuncAddr);
	THROW_HR_IF_NULL(E_INVALIDARG, *realFuncAddr);
	THROW_HR_IF_NULL(E_INVALIDARG, hookFuncAddr);

	THROW_IF_WIN32_ERROR(DetourDetach(realFuncAddr, hookFuncAddr));
}