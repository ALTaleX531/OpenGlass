#include "pch.h"
#include "TestHarness.hpp"
#include "HookHelper.hpp"
#include <array>
#include <thread>

using namespace OpenGlass;

namespace
{
	using PointerTestFunction = int (*)(int);
	int PointerTestOriginal(int value)
	{
		return value + 1;
	}
	PointerTestFunction g_pointerTestTarget{ &PointerTestOriginal };
	PointerTestFunction g_pointerTestOriginal{};
	int PointerTestReplacement(int value)
	{
		return g_pointerTestOriginal(value) + 10;
	}
	HookHelper::PointerHook<&PointerTestReplacement> g_pointerTestHook;
	PointerTestFunction g_importTestOriginal{ &PointerTestOriginal };
	int ImportTestReplacement(int value)
	{
		return g_importTestOriginal(value) + 100;
	}
	__declspec(noinline) int InlineTarget1(int value)
	{
		return value + 2;
	}
	__declspec(noinline) int InlineTarget2(int value)
	{
		return value + 3;
	}
	PointerTestFunction g_inlineOriginal1{ &InlineTarget1 };
	PointerTestFunction g_inlineOriginal2{ &InlineTarget2 };
	int InlineReplacement1(int value)
	{
		return g_inlineOriginal1(value) + 20;
	}
	int InlineReplacement2(int value)
	{
		return g_inlineOriginal2(value) + 30;
	}
}

namespace OpenGlassTests
{
	void TestHookRundown()
	{
		HookHelper::HookRundown rundown;
		rundown.Open();
		Check(!rundown.IsClosing());
		Check(rundown.TryAcquire());

		std::atomic<bool> drained{};
		std::thread waiter([&]
		{
			rundown.WaitForDrain(std::chrono::seconds{ 1 });
			drained.store(true, std::memory_order_release);
		});

		rundown.BeginShutdown();
		Check(rundown.IsClosing());
		Check(!rundown.TryAcquire());
		Check(!drained.load(std::memory_order_acquire));
		rundown.Release();
		waiter.join();
		Check(drained.load(std::memory_order_acquire));

		wil::unique_virtualalloc_ptr<uint8_t> instructions{
			static_cast<uint8_t*>(VirtualAlloc(nullptr, 2, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
		};
		Check(!!instructions);
		instructions.get()[0] = 0x74;
		instructions.get()[1] = 0x23;
		const std::array<uint8_t, 1> firstOriginal{ 0x74 };
		const std::array<uint8_t, 1> secondOriginal{ 0x23 };
		const std::array<uint8_t, 1> replacement{ 0x90 };
		HookHelper::InstructionPatch firstPatch;
		HookHelper::InstructionPatch secondPatch;
		firstPatch.Prepare(instructions.get(), firstOriginal, replacement);
		secondPatch.Prepare(instructions.get() + 1, secondOriginal, replacement);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
			transaction.Apply(firstPatch);
			transaction.Apply(secondPatch);
			transaction.Commit();
		}
		Check(instructions.get()[0] == 0x90 && instructions.get()[1] == 0x90);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
			transaction.Apply(secondPatch);
			transaction.Apply(firstPatch);
			transaction.Commit();
		}
		Check(instructions.get()[0] == 0x74 && instructions.get()[1] == 0x23);

		HookHelper::GetHookRundown().Open();
		g_pointerTestHook.Prepare(&g_pointerTestTarget, &g_pointerTestOriginal);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
			transaction.Apply(g_pointerTestHook);
			transaction.Commit();
		}
		Check(g_pointerTestTarget(1) == 12);
		HookHelper::GetHookRundown().BeginShutdown();
		Check(g_pointerTestTarget(1) == 2);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
			transaction.Apply(g_pointerTestHook);
			transaction.Commit();
		}
		HookHelper::GetHookRundown().WaitForDrain(std::chrono::seconds{ 1 });
		Check(g_pointerTestTarget == &PointerTestOriginal);
		g_pointerTestHook.AttachOnce(&g_pointerTestTarget, &g_pointerTestOriginal);
		Check(!g_pointerTestHook.IsInstalled());
		Check(g_pointerTestTarget == &PointerTestOriginal);

		HookHelper::GetHookRundown().Open();
		const auto importDetour = HookHelper::MakeImportDetour<&ImportTestReplacement>("ImportTest", &g_importTestOriginal);
		const auto importThunk = reinterpret_cast<PointerTestFunction>(importDetour.detour);
		Check(importThunk(1) == 102);
		HookHelper::GetHookRundown().BeginShutdown();
		Check(importThunk(1) == 2);
		HookHelper::GetHookRundown().WaitForDrain(std::chrono::seconds{ 1 });

		const std::array inlineHooks
		{
			HookHelper::DetourInfo{ &g_inlineOriginal1, &InlineReplacement1 },
			HookHelper::DetourInfo{ &g_inlineOriginal2, &InlineReplacement2 }
		};
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
			transaction.ApplyInline("OpenGlassTests", inlineHooks);
			transaction.Commit();
		}
		volatile PointerTestFunction callFirst{ &InlineTarget1 };
		volatile PointerTestFunction callSecond{ &InlineTarget2 };
		Check(callFirst(1) == 23);
		Check(callSecond(1) == 34);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
			transaction.ApplyInline("OpenGlassTests", inlineHooks);
			transaction.Commit();
		}
		Check(callFirst(1) == 3);
		Check(callSecond(1) == 4);
	}

	void TestImportHooks()
	{
		// A private mapped PE fixture keeps the mixed-name/ordinal test independent of system DLLs.
		struct ImportImage
		{
			IMAGE_DOS_HEADER dos;
			IMAGE_NT_HEADERS64 nt;
			IMAGE_IMPORT_DESCRIPTOR imports[2];
			IMAGE_DELAYLOAD_DESCRIPTOR delays[2];
			char dllName[16];
			struct ImportName { WORD hint; char name[16]; } names[2];
			IMAGE_THUNK_DATA nameThunks[4];
			IMAGE_THUNK_DATA thunks[4];
		};
		wil::unique_virtualalloc_ptr<ImportImage> image{
			static_cast<ImportImage*>(VirtualAlloc(nullptr, sizeof(ImportImage), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
		};
		THROW_LAST_ERROR_IF(!image);
		std::construct_at(image.get());
		image->dos.e_magic = IMAGE_DOS_SIGNATURE;
		image->dos.e_lfanew = offsetof(ImportImage, nt);
		image->nt.Signature = IMAGE_NT_SIGNATURE;
		image->nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
		image->nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
		image->nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
		image->nt.OptionalHeader.SizeOfHeaders = sizeof(ImportImage);
		image->nt.OptionalHeader.SizeOfImage = 4096;
		image->nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
		image->nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = { offsetof(ImportImage, imports), sizeof(image->imports) };
		image->nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT] = { offsetof(ImportImage, delays), sizeof(image->delays) };
		image->imports[0].Name = offsetof(ImportImage, dllName);
		image->imports[0].OriginalFirstThunk = offsetof(ImportImage, nameThunks);
		image->imports[0].FirstThunk = offsetof(ImportImage, thunks);
		image->delays[0].Attributes.RvaBased = 1;
		image->delays[0].DllNameRVA = offsetof(ImportImage, dllName);
		image->delays[0].ImportNameTableRVA = offsetof(ImportImage, nameThunks);
		image->delays[0].ImportAddressTableRVA = offsetof(ImportImage, thunks);
		strcpy_s(image->dllName, "fixture.dll");
		strcpy_s(image->names[0].name, "Unchanged");
		strcpy_s(image->names[1].name, "ImportTest");
		image->nameThunks[0].u1.AddressOfData = offsetof(ImportImage, names);
		image->nameThunks[1].u1.Ordinal = IMAGE_ORDINAL_FLAG64 | 131;
		image->nameThunks[2].u1.AddressOfData = offsetof(ImportImage, names) + sizeof(ImportImage::ImportName);
		const auto original = reinterpret_cast<ULONG_PTR>(&PointerTestOriginal);
		for (size_t index = 0; index < 3; ++index) image->thunks[index].u1.Function = original;
		const HookHelper::ImportFunctionDetourInfo functions[]
		{
			HookHelper::MakeImportDetour<&PointerTestReplacement>("Disabled", nullptr, false),
			HookHelper::MakeImportDetour<&ImportTestReplacement>(MAKEINTRESOURCEA(131), &g_importTestOriginal),
			HookHelper::MakeImportDetour<&PointerTestReplacement>("ImportTest", &g_pointerTestOriginal)
		};
		const HookHelper::ImportDllDetourInfo imports[]{ { "FIXTURE.dll", functions } };
		for (const bool delayLoad : { false, true })
		{
			HookHelper::ImportHook hook;
			const auto module = reinterpret_cast<HMODULE>(image.get());
			const auto originalImportStorage = g_importTestOriginal;
			const auto originalPointerStorage = g_pointerTestOriginal;
			Check(hook.Prepare(nullptr, imports, delayLoad) == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
			auto& directory = image->nt.OptionalHeader.DataDirectory[delayLoad ? IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT : IMAGE_DIRECTORY_ENTRY_IMPORT];
			const auto savedDirectory = directory;
			directory = {};
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			directory = savedDirectory;
			auto& nameTable = delayLoad ? image->delays[0].ImportNameTableRVA : image->imports[0].OriginalFirstThunk;
			nameTable = 0;
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			nameTable = offsetof(ImportImage, nameThunks);
			auto& addressTable = delayLoad ? image->delays[0].ImportAddressTableRVA : image->imports[0].FirstThunk;
			addressTable = 0;
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			addressTable = offsetof(ImportImage, thunks);
			image->nameThunks[1].u1.Ordinal = IMAGE_ORDINAL_FLAG64 | 132;
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			image->nameThunks[1].u1.Ordinal = IMAGE_ORDINAL_FLAG64 | 131;
			strcpy_s(image->names[1].name, "Missing");
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			strcpy_s(image->names[1].name, "ImportTest");
			image->nameThunks[0].u1.Ordinal = IMAGE_ORDINAL_FLAG64 | 131;
			Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
			image->nameThunks[0].u1.AddressOfData = offsetof(ImportImage, names);
			if (delayLoad)
			{
				image->delays[0].Attributes.RvaBased = 0;
				Check(hook.Prepare(module, imports, delayLoad) == E_NOINTERFACE);
				image->delays[0].Attributes.RvaBased = 1;
			}
			for (size_t index = 0; index < 3; ++index) Check(image->thunks[index].u1.Function == original);
			Check(g_importTestOriginal == originalImportStorage && g_pointerTestOriginal == originalPointerStorage);
			HookHelper::GetHookRundown().Open();
			{
				HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
				THROW_IF_FAILED(hook.Prepare(module, imports, delayLoad));
				for (size_t index = 0; index < 3; ++index) Check(image->thunks[index].u1.Function == original);
				transaction.Apply(hook);
				transaction.Commit();
			}
			Check(image->thunks[0].u1.Function == original);
			Check(reinterpret_cast<PointerTestFunction>(image->thunks[1].u1.Function)(1) == 102);
			Check(reinterpret_cast<PointerTestFunction>(image->thunks[2].u1.Function)(1) == 12);
			HookHelper::GetHookRundown().BeginShutdown();
			Check(reinterpret_cast<PointerTestFunction>(image->thunks[1].u1.Function)(1) == 2);
			Check(reinterpret_cast<PointerTestFunction>(image->thunks[2].u1.Function)(1) == 2);
			{
				HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
				transaction.Apply(hook);
				transaction.Commit();
			}
			HookHelper::GetHookRundown().WaitForDrain(std::chrono::seconds{ 1 });
			for (size_t index = 0; index < 3; ++index) Check(image->thunks[index].u1.Function == original);
		}
	}
}
