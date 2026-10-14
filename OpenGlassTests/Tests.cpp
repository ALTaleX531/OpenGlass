#include "pch.h"
#include "ConfigurationResources.hpp"
#include "ProjectionFixture.hpp"
#include "RegistryValueResolver.hpp"
#include "DwmSettings.hpp"
#include "../OpenGlassGUI/EditorScope.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"
#include "../OpenGlassGUI/EffectiveConfiguration.hpp"
#include "BlurSettings.hpp"
#include "PngAssetValidation.hpp"
#include "ThemeAtlasLayout.hpp"
#include "../OpenGlassGUI/ColorizationPresets.hpp"
#include "../OpenGlassGUI/ColorPreference.hpp"
#include "../OpenGlassGUI/ColorPolicy.hpp"
#include "../OpenGlassGUI/ShellColorRefresh.hpp"
#include <future>
#include "../OpenGlassGUI/SettingsCatalog.hpp"
#include "../OpenGlassGUI/ConfigurationMigration.hpp"
#include "../OpenGlassGUI/PresetPackage.hpp"
#include "HookHelper.hpp"
#include "Util.hpp"
#include "PeCodeViewIdentity.hpp"
#include "SymbolCatalog.hpp"

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/log.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <wrl/client.h>

int TestWrappingTextLayout();

using namespace OpenGlass;
using namespace OpenGlassTests;

LONG OpenGlassTests::g_layoutOffsets[8]{};
bool OpenGlassTests::g_layoutSupported[8]{};

extern "C" __declspec(noinline) int ProjectionFieldReadHotPath(const std::byte* base) noexcept
{
	constexpr Projection::FieldHandle<FixtureModuleTag, 0, int> field{};
	return field.read(base);
}

namespace
{
	int g_failures{};
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
	int ProjectionChainReplacement1(int value);
	int ProjectionChainReplacement2(int value);
	using ProjectionChain = Projection::ChainDetour<
		g_symbol,
		&ProjectionChainReplacement2,
		&ProjectionChainReplacement1
	>;
	ProjectionChain::Node<1> g_projectionChain1;
	ProjectionChain::Node<0> g_projectionChain2;
	inline constexpr Projection::SymbolHandle<FixtureModuleTag, 1, TestFunction> g_customDispatchSymbol{};
	inline constexpr Projection::SymbolHandle<FixtureModuleTag, 2, TestFunction> g_customPhysicalDispatchSymbol{};
	int ProjectionReplacement(int value)
	{
		return value;
	}
	Projection::Detour<g_customDispatchSymbol, &ProjectionReplacement> g_projectionDetour{};
	int CustomPhysicalDispatch(int value)
	{
		return value;
	}
	Projection::CustomDispatchDetour<g_customPhysicalDispatchSymbol, &CustomPhysicalDispatch> g_customDispatchDetour{};
	int ProjectionChainReplacement1(int value)
	{
		return g_projectionChain1(value) + 100;
	}
	int ProjectionChainReplacement2(int value)
	{
		return g_projectionChain2(value) + 1000;
	}

	void Check(bool condition, const std::source_location& location = std::source_location::current())
	{
		if (!condition)
		{
			g_failures++;
			fprintf(stderr, "Check failed at %s:%u\n", location.file_name(), location.line());
		}
	}

	bool RectNear(const D2D1_RECT_F& actual, const D2D1_RECT_F& expected)
	{
		constexpr float epsilon = 0.0001f;
		return
			std::fabs(actual.left - expected.left) < epsilon &&
			std::fabs(actual.top - expected.top) < epsilon &&
			std::fabs(actual.right - expected.right) < epsilon &&
			std::fabs(actual.bottom - expected.bottom) < epsilon;
	}

	void TestPixelAlign()
	{
		Check(RectNear(
			RectF::PixelAlign({ 10.999f, 20.001f, 30.001f, 40.999f }),
			{ 11.f, 20.f, 30.f, 41.f }
		));
		Check(RectNear(
			RectF::PixelAlign({ -30.001f, -40.999f, -10.999f, -20.001f }),
			{ -30.f, -41.f, -11.f, -20.f }
		));

		const D2D1_RECT_F deviceSpaceRectangle{ 10.999f, 20.001f, 30.001f, 40.999f };
		const auto identity = D2D1::Matrix4x4F{};
		Check(RectNear(
			RectF::ResolveDeviceBounds(deviceSpaceRectangle, identity, true),
			deviceSpaceRectangle
		));
		Check(RectNear(
			RectF::ResolveDeviceBounds(deviceSpaceRectangle, identity, false),
			{ 11.f, 20.f, 30.f, 41.f }
		));

		constexpr float pixelSnapTolerance = 1.f / 256.f;
		Check(RectNear(
			RectF::PixelAlign(
				{
					10.f + pixelSnapTolerance,
					20.f + pixelSnapTolerance,
					30.f - pixelSnapTolerance,
					40.f - pixelSnapTolerance
				}
			),
			{ 10.f, 20.f, 30.f, 40.f }
		));
		Check(RectNear(
			RectF::PixelAlign(
				{
					10.f + 2.f * pixelSnapTolerance,
					20.f + 2.f * pixelSnapTolerance,
					30.f - 2.f * pixelSnapTolerance,
					40.f - 2.f * pixelSnapTolerance
				}
			),
			{ 10.f, 20.f, 30.f, 40.f }
		));
	}

	void TestTransform2DBounds()
	{
		const D2D1_RECT_F rectangle{ 1.f, 2.f, 4.f, 6.f };

		auto translation = D2D1::Matrix4x4F{};
		translation._41 = 3.25f;
		translation._42 = -1.5f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, translation),
			{ 4.25f, 0.5f, 7.25f, 4.5f }
		));

		auto negativeScale = D2D1::Matrix4x4F{};
		negativeScale._11 = -2.f;
		negativeScale._22 = -3.f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, negativeScale),
			{ -8.f, -18.f, -2.f, -6.f }
		));

		auto rotation = D2D1::Matrix4x4F{};
		rotation._11 = 0.f;
		rotation._12 = 1.f;
		rotation._21 = -1.f;
		rotation._22 = 0.f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, rotation),
			{ -6.f, 1.f, -2.f, 4.f }
		));

		auto perspective = D2D1::Matrix4x4F{};
		perspective._14 = 0.1f;
		Check(RectNear(
			RectF::Transform2DBounds({ 0.f, 0.f, 2.f, 1.f }, perspective),
			{ 0.f, 0.f, 5.f / 3.f, 1.f }
		));

		auto crossesProjectionPlane = D2D1::Matrix4x4F{};
		crossesProjectionPlane._14 = 1.f;
		crossesProjectionPlane._44 = -1.f;
		Check(RectNear(
			RectF::Transform2DBounds({ 0.f, 0.f, 2.f, 1.f }, crossesProjectionPlane),
			D2D1::InfiniteRect()
		));

		auto behindProjectionPlane = D2D1::Matrix4x4F{};
		behindProjectionPlane._44 = -1.f;
		Check(wil::rect_is_empty(RectF::Transform2DBounds(rectangle, behindProjectionPlane)));
	}

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

	template <size_t SymbolCount, size_t BindingCount, size_t LayoutCount, size_t CaseCount, size_t SymbolSpecCount = SymbolCount>
	struct RegistryStorage
	{
		std::array<char, 512> strings{};
		std::array<size_t, 8> symbolNameOffsets{};
		std::array<Projection::Version, 4> versions{};
		std::array<ULONG, 2> knownBuilds{200, 250};
		std::array<Projection::SymbolSpec, SymbolSpecCount> symbols{};
		std::array<PVOID, SymbolCount> candidates{};
		std::array<PVOID, SymbolCount> resolved{};
		std::array<Projection::ResolutionState, SymbolCount> resolutionStates{};
		std::array<Projection::BindingSpec, BindingCount> bindings{};
		std::array<Projection::LayoutSpec, LayoutCount> layouts{};
		std::array<Projection::LayoutCase, CaseCount> cases{};
		Projection::ModuleRegistry registry;

		RegistryStorage(Projection::VersionRange supportedRange = Projection::all_versions)
			: registry{
				"test", strings.data(), std::span{symbolNameOffsets}, std::span{versions}, std::span{knownBuilds},
				std::span{symbols}, std::span{candidates}, std::span{resolved}, std::span{resolutionStates},
				std::span{bindings}, std::span{layouts}, std::span{cases}, std::span{g_layoutOffsets}.first(LayoutCount),
				std::span{g_layoutSupported}.first(LayoutCount), supportedRange
			}
		{
		}
	};

	template <typename Storage, size_t Size>
	void SetStrings(Storage& storage, const char (&value)[Size])
	{
		static_assert(Size <= storage.strings.size());
		memcpy(storage.strings.data(), value, Size);
	}

	template <typename Storage, size_t Size>
	size_t AddString(Storage& storage, size_t& cursor, const char (&value)[Size])
	{
		const auto offset = cursor;
		memcpy(storage.strings.data() + cursor, value, Size);
		cursor += Size;
		return offset;
	}

	struct AbiSample
	{
		int Member(double) const noexcept;
	};

	struct Aggregate
	{
		ULONG_PTR first;
		ULONG_PTR second;
	};

	using AggregateFunction = Aggregate (*)(Aggregate, const int&);
	static_assert(std::is_same_v<Projection::projected_abi_t<&AbiSample::Member>, int (*)(const AbiSample*, double)>);
	static_assert(std::is_same_v<Projection::projected_abi_t<static_cast<AggregateFunction>(nullptr)>, AggregateFunction>);
	static_assert(Projection::is_discard_return_compatible_v<int (*)(AbiSample*, double), void (*)(AbiSample*, double)>);
	static_assert(!Projection::is_discard_return_compatible_v<void (*)(AbiSample*, double), void (*)(AbiSample*, double)>);
	static_assert(Projection::is_extra_trailing_argument_compatible_v<
		void (*)(const AbiSample*, double), void (*)(const AbiSample*, double, bool)>);
	static_assert(!Projection::is_extra_trailing_argument_compatible_v<
		void (*)(const AbiSample*, double), void (*)(const AbiSample*, int, bool)>);

	void TestVersionsAndFields()
	{
		Check(Projection::VersionBefore({199, 999}, {200, 0}));
		Check(!Projection::VersionBefore({200, 0}, {200, 0}));
		Check(Projection::VersionBefore({200, 9}, {200, 10}));
		Check(!Projection::VersionBefore({200, 10}, {200, 10}));

		const Projection::VersionRange supportedRange{{200, 10}, {300, 0}};
		RegistryStorage<0, 0, 0, 0> bounded{supportedRange};
		Check(!bounded.registry.SupportsVersion({200, 9}));
		Check(bounded.registry.RecognizesBuild(200));
		Check(!bounded.registry.RecognizesBuild(201));
		Check(bounded.registry.RecognizesBuild(250));
		Check(!bounded.registry.Freeze({200, 9}));
		Check(!bounded.registry.descriptor_error());
		Check(bounded.registry.Freeze({200, 10}));
		Check(!bounded.registry.Freeze({300, 0}));

		RegistryStorage<0, 0, 5, 7> storage;
		storage.versions[1] = {200, 0};
		storage.versions[2] = {200, 10};
		storage.layouts = {{{0, 2}, {2, 1}, {3, 1}, {4, 2}, {6, 1}}};
		storage.cases = {{{8, 1}, {16, 0}, {-8, 0}, {0, 1}, {24, 2}, {32, 0},
			{static_cast<LONG>(2 * sizeof(PVOID)), 0}}};
		g_activeRegistry = &storage.registry;

		constexpr Projection::FieldHandle<FixtureModuleTag, 0, int> positive{};
		constexpr Projection::FieldHandle<FixtureModuleTag, 1, int> negative{};
		constexpr Projection::FieldHandle<FixtureModuleTag, 2, int> unsupported{};
		constexpr Projection::FieldHandle<FixtureModuleTag, 3, int> revision{};
		constexpr Projection::VtableSlotHandle<FixtureModuleTag, 4, PVOID> slot{};
		static_assert(std::is_same_v<decltype(positive.address(static_cast<std::byte*>(nullptr))), int*>);
		static_assert(std::is_same_v<decltype(positive.address(static_cast<const std::byte*>(nullptr))), const int*>);
		static_assert(std::is_same_v<decltype(positive.ref(static_cast<std::byte*>(nullptr))), int&>);
		static_assert(std::is_same_v<decltype(positive.ref(static_cast<const std::byte*>(nullptr))), const int&>);
		static_assert(std::is_same_v<decltype(positive.mutable_address(static_cast<const std::byte*>(nullptr))), int*>);
		static_assert(std::is_same_v<decltype(positive.mutable_ref(static_cast<const std::byte*>(nullptr))), int&>);

		Check(storage.registry.Freeze({150, 0}));
		Check(positive.offset() == 8);
		Check(negative.offset() == -8);
		Check(revision.offset() == 24);
		Check(slot.offset() == 2 * sizeof(PVOID));

		std::array<std::byte, 40> bytes{};
		auto base = bytes.data() + 8;
		*positive.address(base) = 42;
		*negative.address(base) = 17;
		Check(positive.read(base) == 42);
		Check(negative.ref(base) == 17);
		Check(positive.ref(static_cast<const std::byte*>(base)) == 42);
		Check(ProjectionFieldReadHotPath(base) == 42);

		Check(storage.registry.Freeze({200, 9}));
		Check(revision.offset() == 24);
		Check(storage.registry.Freeze({200, 10}));
		Check(revision.offset() == 32);
		Check(!unsupported.is_supported());
	}

	void TestCompleteNameResolutionAndFallback()
	{
		RegistryStorage<2, 1, 0, 0> storage;
		size_t cursor{1};
		const auto requiredId = AddString(storage, cursor, "Required.Id");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "public: int __cdecl Target(int)");
		storage.symbolNameOffsets[1] = AddString(storage, cursor, "public: int __cdecl TargetAlias(int)");
		const auto optionalId = AddString(storage, cursor, "Optional.Id");
		storage.symbolNameOffsets[2] = AddString(storage, cursor, "public: int __cdecl Optional(int)");
		storage.symbols = {{{0, requiredId, 0, 2, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{1, optionalId, 2, 1, 0, 0, Projection::Requirement::Optional, Projection::SymbolFlags::Data}}};
		PVOID published{};
		storage.bindings[0] = {0, &published, Util::force_cast_from(&Replacement)};
		g_activeRegistry = &storage.registry;

		Check(storage.registry.Freeze({150, 0}));
		bool isData{};
		Check(storage.registry.SymbolIsData(0, isData) && !isData);
		Check(storage.registry.SymbolIsData(1, isData) && isData);
		Check(!storage.registry.SymbolIsData(storage.registry.symbol_count(), isData));
		Check(published == Util::force_cast_from(&Replacement));
		storage.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		storage.registry.Collect("public: int __cdecl TargetAlias(int)", Util::force_cast_from(&Target));
		storage.registry.Collect("public: int __cdecl Optional(int)", Util::force_cast_from(&Replacement));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(g_symbol.get() == &Target);
		Check(InvokeCrossTu(2) == 12);
		Check(published == Util::force_cast_from(&Target));
		storage.registry.RecordUndecorationFailure();
		std::string report;
		storage.registry.ReportUnresolved(report, "test!");
		Check(report.empty());

		storage.registry.ResetSymbols();
		Check(g_symbol.try_get() == nullptr);
		Check(published == Util::force_cast_from(&Replacement));
		storage.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		storage.registry.Collect("public: int __cdecl TargetAlias(int)", Util::force_cast_from(&Replacement));
		Check(!storage.registry.ValidateSymbols());
		storage.registry.RecordUndecorationFailure();
		storage.registry.ReportUnresolved(report, "test!");
		Check(report.find("Required.Id (ambiguous)") != std::string::npos);
		Check(report.find("complete-name undecoration failures: 1") != std::string::npos);

		storage.registry.ResetSymbols();
		Check(!storage.registry.CollectResolvedAddress(storage.registry.symbol_count(), Util::force_cast_from(&Target)));
		Check(!storage.registry.CollectResolvedAddress(0, nullptr));
		Check(storage.registry.CollectResolvedAddress(0, Util::force_cast_from(&Target)));
		Check(storage.registry.CollectResolvedAddress(0, Util::force_cast_from(&Target)));
		Check(storage.registry.ValidateSymbols());
		Check(storage.registry.CollectResolvedAddress(0, Util::force_cast_from(&Replacement)));
		Check(!storage.registry.ValidateSymbols());

		RegistryStorage<2, 0, 0, 0> overloads;
		cursor = 1;
		const auto firstId = AddString(overloads, cursor, "Overload.Int");
		overloads.symbolNameOffsets[0] = AddString(overloads, cursor, "public: int __cdecl Overload(int)");
		const auto secondId = AddString(overloads, cursor, "Overload.Double");
		overloads.symbolNameOffsets[1] = AddString(overloads, cursor, "public: int __cdecl Overload(double)");
		overloads.symbols = {{{0, firstId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{1, secondId, 1, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
		Check(overloads.registry.Freeze({150, 0}));
		overloads.registry.Collect("public: int __cdecl Overload(int)", Util::force_cast_from(&Target));
		overloads.registry.Collect("public: int __cdecl Overload(double)", Util::force_cast_from(&Replacement));
		Check(overloads.registry.ValidateSymbols());

		RegistryStorage<2, 0, 0, 0> vtables;
		cursor = 1;
		const auto derivedId = AddString(vtables, cursor, "Derived.Vtable");
		vtables.symbolNameOffsets[0] = AddString(vtables, cursor, "const Derived::`vftable'{for `BaseA'}");
		const auto siblingId = AddString(vtables, cursor, "Sibling.Vtable");
		vtables.symbolNameOffsets[1] = AddString(vtables, cursor, "const Derived::`vftable'{for `BaseB'}");
		vtables.symbols = {{{0, derivedId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{1, siblingId, 1, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
		Check(vtables.registry.Freeze({150, 0}));
		vtables.registry.Collect("const Derived::`vftable'{for `BaseA'}", Util::force_cast_from(&Target));
		vtables.registry.Collect("const Derived::`vftable'{for `BaseB'}", Util::force_cast_from(&Replacement));
		Check(vtables.registry.ValidateSymbols());
	}

	void TestSymbolCatalogCollection()
	{
		RegistryStorage<1, 0, 0, 0> storage;
		size_t cursor{1};
		const auto id = AddString(storage, cursor, "Catalog.Target");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "public: int __cdecl Target(int)");
		storage.symbols[0] = {
			0, id, 0, 1, 0, 0,
			Projection::Requirement::Required,
			Projection::SymbolFlags::None
		};

		const auto module = GetModuleHandleW(nullptr);
		PeCodeViewIdentity identity{};
		Check(SUCCEEDED(ReadLoadedPeCodeViewIdentity(module, identity)));
		const auto base = reinterpret_cast<const BYTE*>(module);
		const auto target = reinterpret_cast<const BYTE*>(Util::force_cast_from(&Target));
		const auto replacement = reinterpret_cast<const BYTE*>(Util::force_cast_from(&Replacement));
		Check(target >= base);
		Check(replacement >= base);
		const auto targetRva = static_cast<UINT32>(target - base);
		const auto replacementRva = static_cast<UINT32>(replacement - base);
		Check(static_cast<size_t>(targetRva) == static_cast<size_t>(target - base));
		Check(static_cast<size_t>(replacementRva) == static_cast<size_t>(replacement - base));

		const Projection::Version firstVersion{100, 7};
		const Projection::Version secondVersion{100, 8};
		std::wstring strings = identity.pdbName;
		strings.push_back(L'\0');
		Projection::SymbolCatalogRecord exactRecord
		{
			Projection::ModuleId::uDWM,
			identity.machine,
			identity.timeDateStamp,
			identity.sizeOfImage,
			identity.pdbGuid,
			identity.pdbAge,
			0,
			firstVersion,
			1,
			1
		};
		auto otherRecord = exactRecord;
		otherRecord.version = secondVersion;
		otherRecord.pdbGuid.Data4[7] ^= 1;
		otherRecord.firstEntry = 0;
		std::array records{otherRecord, exactRecord};
		std::array entries
		{
			replacementRva,
			targetRva
		};
		Projection::SymbolCatalog catalog
		{
			strings.c_str(),
			strings.size(),
			records,
			entries
		};

		Check(storage.registry.Freeze(firstVersion));
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::Collected
		);
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(0, false) == Util::force_cast_from(&Target));

		records[0] = exactRecord;
		records[0].version = secondVersion;
		records[0].firstEntry = 0;
		records[1].pdbGuid.Data4[7] ^= 1;
		Check(storage.registry.Freeze(secondVersion));
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::Collected
		);
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(0, false) == Util::force_cast_from(&Replacement));

		Check(storage.registry.Freeze(firstVersion));
		records[0].module = Projection::ModuleId::DwmCore;
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::NotFound
		);
		records[0].module = Projection::ModuleId::uDWM;
		Check(storage.registry.CollectResolvedAddress(0, Util::force_cast_from(&Target)));
		Check(storage.registry.ValidateSymbols());

		records[0] = exactRecord;
		records[1] = exactRecord;
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::Rejected
		);
		Check(!storage.registry.ValidateSymbols());

		records[1].pdbGuid.Data4[7] ^= 1;
		entries[1] = identity.sizeOfImage;
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::Rejected
		);
		Check(!storage.registry.ValidateSymbols());
		entries[1] = targetRva;

		std::array lateFailureEntries
		{
			targetRva,
			replacementRva
		};
		records[0].firstEntry = 0;
		records[0].entryCount = lateFailureEntries.size();
		catalog.entries = lateFailureEntries;
		Check(
			Projection::CollectSymbolsFromCatalog(
				module,
				Projection::ModuleId::uDWM,
				storage.registry,
				catalog
			) == Projection::SymbolCatalogResult::Rejected
		);
		Check(!storage.registry.ValidateSymbols());
	}

	void TestAtomicCommitAndDetourStorage()
	{
		RegistryStorage<1, 0, 0, 0> first;
		RegistryStorage<1, 0, 0, 0> second;
		size_t cursor{1};
		const auto firstId = AddString(first, cursor, "Target.Id");
		first.symbolNameOffsets[0] = AddString(first, cursor, "public: int __cdecl Target(int)");
		cursor = 1;
		const auto secondId = AddString(second, cursor, "Target.Id");
		second.symbolNameOffsets[0] = AddString(second, cursor, "public: int __cdecl Target(int)");
		first.symbols[0] = {0, firstId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		second.symbols[0] = {0, secondId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		Check(first.registry.Freeze({150, 0}));
		Check(second.registry.Freeze({150, 0}));
		first.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(!Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == nullptr);

		second.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == Util::force_cast_from(&Target));

		g_activeRegistry = &first.registry;
		Check(g_symbol.get() == &Target);
		Check(g_symbol(1) == 11);

		HookHelper::GetHookRundown().Open();
		const HookHelper::DetourInfo firstChainHook{ &g_projectionChain1 };
		const auto chainDispatch = reinterpret_cast<TestFunction>(firstChainHook.detour);
		Check(chainDispatch(1) == 111);
		const HookHelper::DetourInfo secondChainHook{ &g_projectionChain2 };
		Check(chainDispatch(1) == 1111);
		const std::array chainedHooks{ firstChainHook, secondChainHook };
		Check(chainedHooks[0].original == chainedHooks[1].original);
		Check(chainedHooks[0].detour == chainedHooks[1].detour);
		const auto projectionDispatch = g_projectionDetour.prepare_detour();
		Check(projectionDispatch != &ProjectionReplacement);
		Check(g_projectionDetour.prepare_detour() == projectionDispatch);
		Check(g_customDispatchDetour.prepare_detour() == &CustomPhysicalDispatch);
		Check(g_customDispatchDetour.prepare_detour() == &CustomPhysicalDispatch);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
			transaction.ApplyInline("ProjectionChain", chainedHooks);
			transaction.Commit();
		}
		volatile TestFunction invokeTarget{ &Target };
		Check(invokeTarget(1) == 1111);
		HookHelper::GetHookRundown().BeginShutdown();
		Check(invokeTarget(1) == 11);
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
			transaction.ApplyInline("ProjectionChain", chainedHooks);
			transaction.Commit();
		}
		HookHelper::GetHookRundown().WaitForDrain(std::chrono::seconds{ 1 });
		Check(invokeTarget(1) == 11);
	}

	void TestDisjointProjectedBindings()
	{
		RegistryStorage<2, 2, 0, 0> storage;
		size_t cursor{1};
		const auto oldId = AddString(storage, cursor, "Target.Old");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "public: int __cdecl TargetOld(int)");
		const auto newId = AddString(storage, cursor, "Target.New");
		storage.symbolNameOffsets[1] = AddString(storage, cursor, "public: int __cdecl TargetNew(int)");
		storage.versions[1] = {100, 0};
		storage.symbols = {{{0, oldId, 0, 1, 0, 1, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{1, newId, 1, 1, 1, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
		PVOID published{};
		storage.bindings = {{{0, &published, Util::force_cast_from(&Replacement)},
			{1, &published, Util::force_cast_from(&Replacement)}}};

		Check(storage.registry.Freeze({99, 0}));
		storage.registry.Collect("public: int __cdecl TargetOld(int)", Util::force_cast_from(&Target));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(published == Util::force_cast_from(&Target));

		Check(storage.registry.Freeze({100, 0}));
		Check(published == Util::force_cast_from(&Replacement));
		storage.registry.Collect("public: int __cdecl TargetNew(int)", Util::force_cast_from(&Replacement));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(published == Util::force_cast_from(&Replacement));
	}

	void TestLogicalSymbolBindings()
	{
		RegistryStorage<1, 1, 0, 0, 2> storage;
		size_t cursor{1};
		const auto id = AddString(storage, cursor, "Target.Logical");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "public: int __cdecl TargetOld(int)");
		storage.symbolNameOffsets[1] = AddString(storage, cursor, "public: int __cdecl TargetNew(int)");
		storage.versions[1] = {100, 0};
		storage.versions[2] = {200, 0};
		storage.symbols = {{{0, id, 0, 1, 0, 1, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{0, id, 1, 1, 2, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
		PVOID published{};
		storage.bindings[0] = {0, &published, Util::force_cast_from(&Replacement)};

		Check(storage.registry.Freeze({99, 0}));
		storage.registry.Collect("public: int __cdecl TargetOld(int)", Util::force_cast_from(&Target));
		Check(storage.registry.ValidateSymbols());
		Check(Projection::IsVersionInRange({99, 0}, storage.registry.SymbolRange(0)));
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(0, false) == Util::force_cast_from(&Target));
		Check(published == Util::force_cast_from(&Target));

		Check(storage.registry.Freeze({150, 0}));
		Check(storage.registry.ValidateSymbols());
		Check(!Projection::IsVersionInRange({150, 0}, storage.registry.SymbolRange(0)));
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(0, false) == nullptr);
		Check(published == Util::force_cast_from(&Replacement));

		Check(storage.registry.Freeze({200, 0}));
		storage.registry.Collect("public: int __cdecl TargetNew(int)", Util::force_cast_from(&Replacement2));
		Check(storage.registry.ValidateSymbols());
		Check(Projection::IsVersionInRange({200, 0}, storage.registry.SymbolRange(0)));
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(0, false) == Util::force_cast_from(&Replacement2));
		Check(published == Util::force_cast_from(&Replacement2));
	}

	void TestInvalidMetadata()
	{
		RegistryStorage<1, 0, 1, 0> storage;
		storage.symbols[0] = {0, 0, 8, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		storage.layouts[0] = {0, 1};
		Check(!storage.registry.Freeze({150, 0}));
		Check(storage.registry.descriptor_error());

		RegistryStorage<1, 0, 0, 0, 2> overlappingBindings;
		size_t cursor{1};
		overlappingBindings.symbolNameOffsets[0] = AddString(
			overlappingBindings,
			cursor,
			"public: int __cdecl Target(int)"
		);
		overlappingBindings.symbols = {{{
			0, 0, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None
		}, {
			0, 0, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None
		}}};
		Check(!overlappingBindings.registry.Freeze({150, 0}));
		Check(overlappingBindings.registry.descriptor_error());
	}

	void TestDwmSettings()
	{
		GUID id{};
		THROW_IF_FAILED(CoCreateGuid(&id));
		WCHAR guidString[40]{};
		THROW_HR_IF(E_UNEXPECTED, !StringFromGUID2(id, guidString, ARRAYSIZE(guidString)));
		const auto path = std::wstring(L"Software\\OpenGlassTests\\DwmSettings\\") + guidString;
		const auto cleanup = wil::scope_exit([&] { RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); });
		const auto user = wil::reg::create_unique_key(HKEY_CURRENT_USER, path.c_str(), wil::reg::key_access::readwrite);
		const auto machine = wil::reg::create_unique_key(user.get(), L"Machine", wil::reg::key_access::readwrite);

		// Explicit zero is a user value; an invalid type falls through to HKLM.
		DwmSettings::ValueSource source{};
		wil::reg::set_value_dword(machine.get(), L"Value", 1);
		wil::reg::set_value_dword(user.get(), L"Value", 0);
		Check(DwmSettings::GetDword(user.get(), machine.get(), L"Value", 1, &source) == 0);
		Check(source == DwmSettings::ValueSource::User);
		wil::reg::set_value_string(user.get(), L"Value", L"invalid DWORD");
		Check(DwmSettings::GetDword(user.get(), machine.get(), L"Value", 0, &source) == 1);
		Check(source == DwmSettings::ValueSource::Machine);
		Check(DwmSettings::GetDword(user.get(), nullptr, L"Value", 7, &source) == 7);
		Check(source == DwmSettings::ValueSource::Default);
		Check(DwmSettings::GetDword(nullptr, machine.get(), L"Value") == 1);
		wil::reg::set_value_dword(user.get(), L"Value", 2);
		wil::reg::set_value_dword(machine.get(), L"Override", 3);
		Check(DwmSettings::GetOverridableDword(user.get(), machine.get(), L"Value", L"Override") == 3);
		wil::reg::set_value_dword(user.get(), L"Override", 4);
		Check(DwmSettings::GetOverridableDword(user.get(), machine.get(), L"Value", L"Override") == 4);

		WCHAR value[16]{};
		wil::reg::set_value_string(machine.get(), L"Path", L"machine.png");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		// An empty string masks HKLM, and REG_EXPAND_SZ remains unexpanded.
		wil::reg::set_value_string(user.get(), L"Path", L"");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(value[0] == L'\0');
		wil::reg::set_value_expanded_string(user.get(), L"Path", L"%TEMP%\\x.png");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"%TEMP%\\x.png");
		wil::reg::set_value_dword(user.get(), L"Path", 0);
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		// A too-small buffer falls through; failure in both layers clears it.
		wil::reg::set_value_string(user.get(), L"Path", L"a path longer than the buffer");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		DwmSettings::GetString(user.get(), nullptr, L"Path", value);
		Check(value[0] == L'\0');
		DwmSettings::GetString(nullptr, machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		DwmSettings::GetString(nullptr, nullptr, L"Path", value);
		Check(value[0] == L'\0');
	}

	void TestOverridableRegistryValueResolution()
	{
		constexpr DWORD defaultValue = 55;
		for (unsigned mask = 0; mask < 16; mask++)
		{
			const std::optional<DWORD> userOverride = (mask & 0x1) ? std::optional<DWORD>{ 11 } : std::nullopt;
			const std::optional<DWORD> userBase = (mask & 0x2) ? std::optional<DWORD>{ 22 } : std::nullopt;
			const std::optional<DWORD> machineOverride = (mask & 0x4) ? std::optional<DWORD>{ 33 } : std::nullopt;
			const std::optional<DWORD> machineBase = (mask & 0x8) ? std::optional<DWORD>{ 44 } : std::nullopt;
			const auto resolved = ResolveOverridableRegistryValue(
				userOverride,
				userBase,
				machineOverride,
				machineBase,
				defaultValue
			);

			if (userOverride)
			{
				Check(resolved == 11);
			}
			else if (machineOverride)
			{
				Check(resolved == 33);
			}
			else if (userBase)
			{
				Check(resolved == 22);
			}
			else if (machineBase)
			{
				Check(resolved == 44);
			}
			else
			{
				Check(resolved == defaultValue);
			}
		}

		// Typed registry reads make missing and wrong-type values unavailable.
		for (unsigned combination = 0; combination < 81; ++combination)
		{
			std::array<std::optional<DWORD>, 4> values;
			auto states = combination;
			for (std::size_t slot = 0; slot < values.size(); ++slot, states /= 3)
			{
				const std::variant<std::monostate, DWORD, std::wstring> raw = states % 3 == 0
					? std::variant<std::monostate, DWORD, std::wstring>{}
					: states % 3 == 1 ? std::variant<std::monostate, DWORD, std::wstring>{ static_cast<DWORD>(slot + 1) }
					: std::variant<std::monostate, DWORD, std::wstring>{ L"invalid DWORD type" };
				if (const auto value = std::get_if<DWORD>(&raw)) values[slot] = *value;
			}
			const auto resolved = ResolveOverridableRegistryValue(values[0], values[2], values[1], values[3], defaultValue);
			DWORD expected = defaultValue;
			for (const auto& value : values) if (value) { expected = *value; break; }
			Check(resolved == expected);
			for (const bool userScope : { false, true })
			{
				const auto notice = GetEditorRegistryNotice(userScope, values[2].has_value(), values[3].has_value());
				Check((notice == EditorRegistryNotice::Inherited) == (userScope && !values[2] && values[3]));
				Check((notice == EditorRegistryNotice::Overridden) == (!userScope && values[2].has_value()));
			}
		}

		// Empty HKCU reports inheritance when HKLM is customized.
		const std::optional<DWORD> absent, zero = 0u, one = 1u;
		Check(GetEditorRegistryNotice(true, false, true) == EditorRegistryNotice::Inherited);
		// An explicit default is a real user value, not inheritance (also masks equal HKLM).
		Check(GetEditorRegistryNotice(true, zero.has_value(), one.has_value()) == EditorRegistryNotice::None);
		Check(GetEditorRegistryNotice(false, zero.has_value(), zero.has_value()) == EditorRegistryNotice::Overridden);
		Check(GetEditorRegistryNotice(false, true, false) == EditorRegistryNotice::Overridden);
		// Runtime inheritance continues when HKCU is absent.
		Check(ResolveOverridableRegistryValue(absent, absent, absent, one, DWORD{}) == 1u);
		const std::optional<std::wstring> emptyPath = L"", machinePath = L"machine.png";
		Check(GetEditorRegistryNotice(true, emptyPath.has_value(), machinePath.has_value()) == EditorRegistryNotice::None);
	}

	const std::array<unsigned char, 120>& ValidPng()
	{
		static constexpr std::array<unsigned char, 120> bytes
		{
			0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
			0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,
			0x89,0x00,0x00,0x00,0x01,0x73,0x52,0x47,0x42,0x00,0xae,0xce,0x1c,0xe9,0x00,0x00,
			0x00,0x04,0x67,0x41,0x4d,0x41,0x00,0x00,0xb1,0x8f,0x0b,0xfc,0x61,0x05,0x00,0x00,
			0x00,0x09,0x70,0x48,0x59,0x73,0x00,0x00,0x0e,0xc3,0x00,0x00,0x0e,0xc3,0x01,0xc7,
			0x6f,0xa8,0x64,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x18,0x57,0x63,0xf8,0xcf,
			0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0xa6,0x5c,0x9b,0x5d,0x00,0x00,0x00,0x00,
			0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
		};
		return bytes;
	}

	std::uint32_t TestPngCrc(std::span<const unsigned char> bytes)
	{
		std::uint32_t crc = 0xFFFFFFFFu;
		for (const auto value : bytes)
		{
			crc ^= value;
			for (unsigned bit = 0; bit < 8; ++bit)
			{
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
		return crc ^ 0xFFFFFFFFu;
	}

	void AppendPngChunk(std::vector<unsigned char>& png, const char (&type)[5], std::span<const unsigned char> data = {})
	{
		const auto appendBigEndian = [&png](std::uint32_t value)
		{
			png.push_back(static_cast<unsigned char>(value >> 24));
			png.push_back(static_cast<unsigned char>(value >> 16));
			png.push_back(static_cast<unsigned char>(value >> 8));
			png.push_back(static_cast<unsigned char>(value));
		};
		appendBigEndian(static_cast<std::uint32_t>(data.size()));
		const auto crcStart = png.size();
		png.insert(png.end(), type, type + 4);
		png.insert(png.end(), data.begin(), data.end());
		appendBigEndian(TestPngCrc({ png.data() + crcStart, png.size() - crcStart }));
	}

	std::vector<unsigned char> MakeStructuralPng(UINT width = 1, UINT height = 1, std::size_t ancillaryChunks = 0)
	{
		std::vector<unsigned char> png{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
		std::array<unsigned char, 13> header
		{
			static_cast<unsigned char>(width >> 24), static_cast<unsigned char>(width >> 16),
			static_cast<unsigned char>(width >> 8), static_cast<unsigned char>(width),
			static_cast<unsigned char>(height >> 24), static_cast<unsigned char>(height >> 16),
			static_cast<unsigned char>(height >> 8), static_cast<unsigned char>(height),
			8, 6, 0, 0, 0
		};
		AppendPngChunk(png, "IHDR", header);
		for (std::size_t index = 0; index < ancillaryChunks; ++index)
		{
			AppendPngChunk(png, "tEXt");
		}
		const std::array<unsigned char, 1> compressed{ 0 };
		AppendPngChunk(png, "IDAT", compressed);
		AppendPngChunk(png, "IEND");
		return png;
	}

	std::span<const std::byte> AsBytes(std::span<const unsigned char> bytes)
	{
		return { reinterpret_cast<const std::byte*>(bytes.data()), bytes.size() };
	}

	void TestPngAssetValidation()
	{
		PngAssetValidation::ImageInfo info{};
		const auto& valid = ValidPng();
		Check(SUCCEEDED(PngAssetValidation::ValidateStructure(AsBytes(valid), info)));
		Check(info.width == 1 && info.height == 1);

		auto malformed = std::vector<unsigned char>{ valid.begin(), valid.end() };
		malformed[0] = 0;
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end());
		malformed[29] ^= 1;
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end() - 12);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end());
		malformed.push_back(0);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));

		auto structural = MakeStructuralPng();
		const std::vector<unsigned char> duplicateHeader(structural.begin() + 8, structural.begin() + 33);
		structural.insert(structural.begin() + 33, duplicateHeader.begin(), duplicateHeader.end());
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(PngAssetValidation::MaximumDimension + 1, 1);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(8192, 8192);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(1, 1, PngAssetValidation::MaximumChunkCount);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));

		const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		Check(SUCCEEDED(initializeResult) || initializeResult == RPC_E_CHANGED_MODE);
		const bool uninitialize = SUCCEEDED(initializeResult);
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
		Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))));
		if (factory)
		{
			Check(SUCCEEDED(PngAssetValidation::ValidateStructure(AsBytes(valid), info)));
			Microsoft::WRL::ComPtr<IWICStream> stream;
			Check(SUCCEEDED(factory->CreateStream(&stream)));
			auto copy = std::vector<unsigned char>{ valid.begin(), valid.end() };
			Check(SUCCEEDED(stream->InitializeFromMemory(copy.data(), static_cast<DWORD>(copy.size()))));
			Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
			Check(SUCCEEDED(PngAssetValidation::CreateValidatedWicSource(factory.Get(), stream.Get(), &info, &converter)));
			converter.Reset();
			PngAssetValidation::ImageInfo wrong{ 2, 1 };
			Check(FAILED(PngAssetValidation::CreateValidatedWicSource(factory.Get(), stream.Get(), &wrong, &converter)));
		}
		factory.Reset();
		if (uninitialize) CoUninitialize();
	}

	std::span<const std::byte> AsBytes(std::string_view value)
	{
		return { reinterpret_cast<const std::byte*>(value.data()), value.size() };
	}

	void TestThemeAtlasLayoutParser()
	{
		std::string valid = "# legacy comment ";
		valid.push_back(static_cast<char>(0xE9));
		valid += "\r\n1; 2; 3 = 4, 5, 6, 7\t# mapping comment ";
		valid.push_back(static_cast<char>(0xE9));
		valid += "\n1;2;3=8,9,10,11\nUnknownProperty=9 # ignored by the consumer\nRS1Compatibility=1\n12;2;3=-1,+2,3,4\nCaptionHeight=20\nCaptionHeight=21\n";
		ThemeAtlasLayout::Document document;
		Check(SUCCEEDED(ThemeAtlasLayout::Parse(AsBytes(valid), document)));
		Check(document.records.size() == 7);
		if (document.records.size() == 7)
		{
			const auto& first = std::get<ThemeAtlasLayout::Mapping>(document.records[0]);
			Check(first.part == 1 && first.state == 2 && first.property == 3);
			Check(first.value == std::array<std::int32_t, 4>{ 4, 5, 6, 7 });
			Check(std::get<ThemeAtlasLayout::Property>(document.records[2]).name == "UnknownProperty");
			Check(std::get<ThemeAtlasLayout::Property>(document.records[3]).name == "RS1Compatibility");
			Check(std::get<ThemeAtlasLayout::Mapping>(document.records[4]).part == 12);
			Check(std::get<ThemeAtlasLayout::Property>(document.records[6]).value == 21);
		}

		const auto rejected = [&document](std::string_view value)
		{
			document.records.emplace_back(ThemeAtlasLayout::Property{ "stale", 1 });
			const auto result = ThemeAtlasLayout::Parse(AsBytes(value), document);
			return FAILED(result) && document.records.empty();
		};
		Check(rejected("1;2;3=1,2,3"));
		Check(rejected("1;2;3=1,2,3,4,5"));
		Check(rejected("1;2;3=2147483648,2,3,4"));
		Check(rejected("Property=1=2"));
		Check(rejected(std::string("Property=1\0ignored", 18)));
		Check(rejected("Property=1\x01"));
		Check(rejected(std::string(ThemeAtlasLayout::MaximumLineLength + 1, 'a')));
		std::string tooManyLines;
		for (std::size_t line = 0; line <= ThemeAtlasLayout::MaximumLineCount; ++line) tooManyLines += "#\n";
		Check(rejected(tooManyLines));
	}

	void TestColorizationPresets()
	{
		using namespace ColorizationPresets;

		constexpr std::array expectedVista
		{
			std::pair{ std::wstring_view{ L"Default" }, 0x45409EFEu },
			std::pair{ std::wstring_view{ L"Graphite" }, 0xA3000000u },
			std::pair{ std::wstring_view{ L"Blue" }, 0xA8004ADEu },
			std::pair{ std::wstring_view{ L"Teal" }, 0x82008CA5u },
			std::pair{ std::wstring_view{ L"Red" }, 0x9CCE0C0Fu },
			std::pair{ std::wstring_view{ L"Orange" }, 0xA6FF7700u },
			std::pair{ std::wstring_view{ L"Pink" }, 0x49F93EE7u },
			std::pair{ std::wstring_view{ L"Frost" }, 0xCCEFF7F7u }
		};
		constexpr std::array expectedWindows7
		{
			std::pair{ std::wstring_view{ L"Sky" }, 0x6B74B8FCu },
			std::pair{ std::wstring_view{ L"Twilight" }, 0xA80046ADu },
			std::pair{ std::wstring_view{ L"Sea" }, 0x8032CDCDu },
			std::pair{ std::wstring_view{ L"Leaf" }, 0x6614A600u },
			std::pair{ std::wstring_view{ L"Lime" }, 0x6697D937u },
			std::pair{ std::wstring_view{ L"Sun" }, 0x54FADC0Eu },
			std::pair{ std::wstring_view{ L"Pumpkin" }, 0x80FF9C00u },
			std::pair{ std::wstring_view{ L"Ruby" }, 0xA8CE0F0Fu },
			std::pair{ std::wstring_view{ L"Fuchsia" }, 0x66FF0099u },
			std::pair{ std::wstring_view{ L"Blush" }, 0x70FCC7F8u },
			std::pair{ std::wstring_view{ L"Violet" }, 0x856E3BA1u },
			std::pair{ std::wstring_view{ L"Lavender" }, 0x528D5A94u },
			std::pair{ std::wstring_view{ L"Taupe" }, 0x6698844Cu },
			std::pair{ std::wstring_view{ L"Chocolate" }, 0xA84F1B1Bu },
			std::pair{ std::wstring_view{ L"Slate" }, 0x80555555u },
			std::pair{ std::wstring_view{ L"Frost" }, 0x54FCFCFCu }
		};
		Check(Vista.size() == expectedVista.size());
		Check(Windows7.size() == expectedWindows7.size());
		for (size_t index = 0; index < Vista.size(); index++)
		{
			Check(Vista[index].name == expectedVista[index].first);
			Check(Vista[index].argb == expectedVista[index].second);
		}
		for (size_t index = 0; index < Windows7.size(); index++)
		{
			Check(Windows7[index].name == expectedWindows7[index].first);
			Check(Windows7[index].argb == expectedWindows7[index].second);
		}

		for (const auto family : { Family::Vista, Family::Windows7 })
		{
			for (const auto& preset : Get(family))
			{
				Check(preset.family == family);
			}
		}

		constexpr std::array expectedVistaOpacity{ 27u, 64u, 66u, 51u, 61u, 65u, 29u, 80u };
		Check(ClassicIntensityMinimum == 10);
		Check(ClassicIntensityMaximum == 85);
		for (size_t index = 0; index < Vista.size(); index++)
		{
			Check(CalculateVistaOpacity(Vista[index].argb) == expectedVistaOpacity[index]);
		}
		Check(CalculateVistaOpacity(0x00000000) == 0);
		Check(CalculateVistaOpacity(0xFF000000) == 100);
		Check(CalculateIntensityAlpha(0) == 0);
		Check(CalculateIntensityAlpha(100) == 255);
		for (const auto& preset : Windows7)
		{
			Check(CalculateIntensityAlpha(CalculateVistaOpacity(preset.argb)) == (preset.argb >> 24));
		}

		constexpr Windows7Parameters sky
		{
			0x6B74B8FC,
			0x6B74B8FC,
			8,
			43,
			49
		};
		constexpr Windows7Parameters skyOpaque
		{
			0x6B74B8FC,
			0x6B74B8FC,
			42,
			10,
			48
		};
		Check(CalculateWindows7Parameters(0x6B74B8FC, false) == sky);
		Check(CalculateWindows7Parameters(0x6B74B8FC, true) == skyOpaque);
		Check(CalculateWindows7Parameters(0xA80046AD, false).colorBalance == 56);
		Check(CalculateWindows7Parameters(0xA80046AD, false).afterglowBalance == 11);
		Check(CalculateWindows7Parameters(0xA80046AD, false).blurBalance == 33);
		Check(CalculateWindows7Parameters(0xA8CE0F0F, false).colorBalance == 56);
		Check(CalculateWindows7Parameters(0xA8CE0F0F, false).afterglowBalance == 11);
		Check(CalculateWindows7Parameters(0xA8CE0F0F, false).blurBalance == 33);
		Check(CalculateWindows7Parameters(0x65000000, false).colorBalance == 5);
		Check(CalculateWindows7Parameters(0x65000000, false).afterglowBalance == 44);
		Check(CalculateWindows7Parameters(0x65000000, false).blurBalance == 51);
		Check(CalculateWindows7Parameters(0x66000000, false).colorBalance == 5);
		Check(CalculateWindows7Parameters(0x66000000, false).afterglowBalance == 45);
		Check(CalculateWindows7Parameters(0x66000000, false).blurBalance == 50);
		Check(CalculateWindows7Parameters(0xBD000000, false).colorBalance == 70);
		Check(CalculateWindows7Parameters(0xBD000000, false).afterglowBalance == 0);
		Check(CalculateWindows7Parameters(0xBD000000, false).blurBalance == 30);

		// Preset identity uses the saved GUI intensity, not mutable system Alpha.
		auto synchronizedSky = sky;
		synchronizedSky.color &= 0x00FFFFFF;
		synchronizedSky.afterglow |= 0xFF000000;
		Check(MatchesWindows7Preset(Windows7.front(), synchronizedSky, 42, false));
		Check(!MatchesWindows7Preset(Windows7.front(), synchronizedSky, 63, false));
		++synchronizedSky.blurBalance;
		Check(!MatchesWindows7Preset(Windows7.front(), synchronizedSky, 42, false));
		for (const auto& preset : Windows7)
		{
			for (const bool opaque : { false, true })
			{
				const auto intensity = CalculateVistaOpacity(preset.argb);
				Check(MatchesWindows7Preset(preset,
					CalculateWindows7Parameters((preset.argb & 0x00FFFFFF) | (CalculateIntensityAlpha(intensity) << 24), opaque),
					intensity, opaque));
			}
		}
	}

	void TestBlurSettings()
	{
		Check(BlurSettings::DecodeBlurAmount(0) == 0.f);
		Check(BlurSettings::DecodeBlurAmount(BlurSettings::DefaultEncodedDeviation) == 9.f);
		Check(BlurSettings::DecodeBlurAmount(100) == 30.f);
		Check(BlurSettings::DecodeBlurAmount(UINT32_MAX) == BlurSettings::MaximumBlurAmount);
		Check(BlurSettings::DecodeGuiBlurAmount(UINT32_MAX) == BlurSettings::GuiMaximumBlurAmount);
		Check(BlurSettings::EncodeGuiBlurAmount(-1) == 0);
		Check(BlurSettings::EncodeGuiBlurAmount(9) == BlurSettings::DefaultEncodedDeviation);
		Check(BlurSettings::EncodeGuiBlurAmount(31) == 100);
		for (int blurAmount = BlurSettings::GuiMinimumBlurAmount; blurAmount <= BlurSettings::GuiMaximumBlurAmount; ++blurAmount)
		{
			Check(
				BlurSettings::DecodeGuiBlurAmount(
					BlurSettings::EncodeGuiBlurAmount(blurAmount)
				) == blurAmount
			);
		}
		Check(BlurSettings::Direct3DStandardDeviation == 3.f);
	}

	void TestColorPolicy()
	{
		const LSTATUS statuses[]{ ERROR_SUCCESS, ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND,
			ERROR_UNSUPPORTED_TYPE, ERROR_MORE_DATA, ERROR_ACCESS_DENIED };
		for (const auto backgroundStatus : statuses)
		{
			unsigned calls{};
			const auto read = [&](HKEY root, const wchar_t* path, const wchar_t* name,
				DWORD flags, DWORD* type, void*, DWORD* size) -> LSTATUS
			{
				++calls;
				Check(root == HKEY_LOCAL_MACHINE && std::wstring_view(path) == LR"(Software\Policies\Microsoft\Windows\Personalization)");
				Check(type == nullptr);
				Check(std::wstring_view(name) == L"PersonalColors_Background" && flags == RRF_RT_REG_SZ && *size == 16);
				return backgroundStatus;
			};
			HRESULT result = S_OK;
			bool blocked{};
			try { blocked = ColorPolicy::IsAccentSyncBlocked(read); }
			catch (...) { result = wil::ResultFromCaughtException(); }
			const bool inaccessible = backgroundStatus == ERROR_ACCESS_DENIED;
			// NoChangingStartMenuBackground is deliberately not read, regardless of its value/type.
			Check(calls == 1 && result == (inaccessible ? E_ACCESSDENIED : S_OK));
			if (!inaccessible) Check(blocked == (backgroundStatus == ERROR_SUCCESS));
		}
	}

	void TestSettingsCatalog()
	{
		std::vector<std::wstring_view> names;

		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			Check(static_cast<std::size_t>(spec.id) == index);
			Check(!spec.name.empty());
			Check(spec.introducedIn > 0 && spec.introducedIn <= Settings::CatalogVersion);
			Check(std::find(names.begin(), names.end(), spec.name) == names.end());
			names.push_back(spec.name);
			Check(Settings::Find(spec.name) == &spec);
			if (spec.type == Settings::ValueType::String)
			{
				Check(spec.assetRole != Settings::AssetRole::None);
			}
		}
		Check(Settings::PresetPackSettingCount(1) == Settings::PresetPackSettingCount() + 5);
		Check(Settings::Get(Settings::Id::GlassOverrideAccent).impact == Settings::UpdateImpact::Colorization);
		Check(Settings::Get(Settings::Id::GlassSafetyZoneMode).impact == Settings::UpdateImpact::Colorization);
		Check(Settings::Get(Settings::Id::UseDirect3DRendering).impact == Settings::UpdateImpact::Colorization);
		Check(!Settings::Get(Settings::Id::MinMaxButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::CloseButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::ToolCloseButtonGlowId).includeInPresetPacks);
		Check(Settings::PresetPackSettingCount() + 10 == Settings::Catalog.size());
		Check(Settings::Find(L"NotAnOpenGlassSetting") == nullptr);
	}

	void TestPreviewJournal()
	{
		PreviewJournal<int, std::optional<int>> journal;
		std::map<int, int> registry{ { 1, 10 } };
		auto read = [&](int key) -> std::optional<int> { if (registry.contains(key)) return registry[key]; return {}; };
		auto write = [&](int key, std::optional<int> value) { if (value) registry[key] = *value; else registry.erase(key); return true; };
		auto apply = [&](int value) { journal.Begin(); journal.Touch(1, read); registry[1] = value; journal.CommitAttempt(); };
		apply(20); // A
		journal.Begin(); journal.Touch(1, read); registry[1] = 30; journal.Touch(2, read); registry[2] = 7;
		Check(journal.RollbackAttempt(write)); // B failed, return to A, including missing values.
		Check(registry[1] == 20 && !registry.contains(2) && journal.IsDirty());
		apply(40); // C
		Check(journal.Revert(write)); Check(registry[1] == 10 && !journal.IsDirty());
		apply(20); apply(30); apply(40); // Successful A -> B -> C keeps the initial checkpoint.
		Check(journal.Revert(write)); Check(registry[1] == 10 && !journal.IsDirty());
		apply(50); journal.Accept(); apply(60);
		Check(!journal.Revert([](int, auto) { return false; })); Check(journal.IsDirty());
		Check(journal.Revert(write)); Check(registry[1] == 50);
		journal.Begin(); journal.Touch(2, read); registry[2] = 99;
		Check(!journal.RollbackAttempt(write, false)); // Failed independent color recovery keeps baseline.
		Check(journal.IsDirty()); Check(journal.Revert(write)); Check(!registry.contains(2));
		apply(70);
		Check(journal.Revert(write, true, false) && journal.IsDirty()); // Retain until refresh succeeds.
		journal.Accept();
		apply(80);
		journal.Begin(); journal.Touch(1, read); registry[1] = 90;
		journal.DiscardBaseline(1); // Automatic takes over, but this attempt can still fail.
		Check(!journal.IsDirty());
		Check(journal.RollbackAttempt(write) && registry[1] == 80 && journal.IsDirty());
		Check(journal.Revert(write) && registry[1] == 50);
		journal.Begin(); journal.Touch(1, read); registry[1] = 100;
		journal.DiscardBaseline(1); journal.Reconcile(read); journal.CommitAttempt();
		Check(!journal.IsDirty());
		Check(journal.Revert(write) && registry[1] == 100);
		using ScopedKey = std::pair<Settings::Scope, int>;
		PreviewJournal<ScopedKey, int> scoped;
		std::map<ScopedKey, int> layers{ { { Settings::Scope::User, 1 }, 10 }, { { Settings::Scope::Machine, 1 }, 20 } };
		for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto original = layers;
			const ScopedKey key{ scope, 1 };
			scoped.Begin(); scoped.Touch(key, [&](const auto& k) { return layers.at(k); }); layers[key] = 90; scoped.CommitAttempt();
			Check(layers.at({ scope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, 1 })
				== original.at({ scope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, 1 }));
			Check(scoped.Revert([&](const auto& k, int value) { layers[k] = value; return true; })); Check(layers == original);
		}
	}

	void TestEffectiveConfiguration()
	{
		using namespace EffectiveConfiguration;
		using enum Settings::Id;
		// The catalog is the sole classification used by cleanup; unrelated values survive.
		Layer populated;
		unsigned colorOverrides{};
		for (const auto& spec : Settings::Catalog)
		{
			populated.values[spec.id] = { true, REG_BINARY, { 1 } };
			if (Settings::IsColorOverride(spec.id))
			{
				Check(spec.id == ColorizationColorOverride || spec.id == ColorizationAfterglowOverride);
				Check(!Settings::IsPresetPackSetting(spec));
				++colorOverrides;
			}
		}
		const auto colorCleanup = PlanColorCleanup(populated, populated);
		Check(colorOverrides == 2 && colorCleanup.size() == 2 * colorOverrides);
		std::set<std::pair<Settings::Scope, Settings::Id>> removed;
		for (const auto& change : colorCleanup)
		{
			Check(Settings::IsColorOverride(change.id) && change.before == populated.values.at(change.id) && !change.after.present);
			Check(removed.emplace(change.scope, change.id).second);
		}
		for (unsigned u = 0; u < 3; ++u) for (unsigned m = 0; m < 3; ++m)
		{
			Layer user, machine;
			auto raw = [](unsigned kind, DWORD number) { return kind == 0 ? RegistryConfig::RawValue{} : kind == 1 ? Encode(Value{ number }) : RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2 } }; };
			user.values[GlassOpacity] = raw(u, 17); machine.values[GlassOpacity] = raw(m, 31);
			const Value expected = u == 1 ? Value{ DWORD{17} } : m == 1 ? Value{ DWORD{31} } : Value{};
			Check(Resolve(user, machine, GlassOpacity) == expected);
			Check(Capture(user, machine).at(GlassOpacity) == expected);
			for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
			for (const auto& desired : { Value{}, Value{ DWORD{17} }, Value{ DWORD{0xFFFFFFFF} } })
			{
				auto newUser = user, newMachine = machine;
				const auto plan = Plan(user, machine, { { GlassOpacity, desired } }, target);
				for (const auto& change : plan)
				{
					auto& layer = change.scope == Settings::Scope::User ? newUser : newMachine;
					layer.values[change.id] = change.after;
				}
				if (std::holds_alternative<std::monostate>(desired))
				{
					const auto& other = target == Settings::Scope::User ? machine : user;
					Check(Resolve(newUser, newMachine, GlassOpacity) == Decode(Raw(other, GlassOpacity), Settings::Get(GlassOpacity)));
					Check(!Raw(target == Settings::Scope::User ? newUser : newMachine, GlassOpacity).present);
					if (target == Settings::Scope::Machine) Check(newUser.values == user.values);
				}
				else Check(Resolve(newUser, newMachine, GlassOpacity) == desired);
				if (target == Settings::Scope::User) Check(newMachine.values == machine.values);
			}
		}
		for (const auto [base, over] : { std::pair{ ColorizationColorBalance, ColorizationColorBalanceOverride },
			std::pair{ ColorizationAfterglowBalance, ColorizationAfterglowBalanceOverride },
			std::pair{ ColorizationBlurBalance, ColorizationBlurBalanceOverride } })
		for (unsigned u = 0; u < 3; ++u) for (unsigned m = 0; m < 3; ++m)
		{
			Layer user, machine;
			user.values[base] = Encode(Value{ DWORD{33} }); machine.values[base] = Encode(Value{ DWORD{66} });
			auto raw = [](unsigned kind, DWORD value) { return kind == 0 ? RegistryConfig::RawValue{} : kind == 1
				? Encode(Value{ value }) : RegistryConfig::RawValue{ true, REG_BINARY, { 1 } }; };
			user.values[over] = raw(u, 17); machine.values[over] = raw(m, 31);
			const Value expected = u == 1 ? Value{ DWORD{17} } : m == 1 ? Value{ DWORD{31} } : Value{};
			for (const auto version : { 1u, Settings::CatalogVersion })
			{
				const auto captured = Capture(user, machine, version);
				Check(!captured.contains(base) && captured.at(over) == expected);
				for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
					for (const auto& change : Plan(user, machine, captured, target, version)) Check(!Settings::IsWindowsColorBase(change.id));
			}
		}
		Layer user, machine;
		user.values[ColorizationColor] = Encode(Value{ DWORD{0xAA123456} });
		user.values[ColorizationAfterglow] = Encode(Value{ DWORD{0xAA654321} });
		machine.values[ColorizationColorOverride] = Encode(Value{ DWORD{0xBB112233} });
		Check(!Capture(user, machine).contains(ColorizationColorOverride));
		Check(!Capture(user, machine).contains(ColorizationAfterglowOverride));
		user.values.erase(ColorizationColor);
		Check(!Capture(user, machine).contains(ColorizationAfterglowOverride));
		user.values[ColorizationColor] = Encode(Value{ DWORD{0xAA123456} });
		for (bool explicitUser : { false, true })
		{
			const auto resolved = ResolveOverridableRegistryValue<DWORD>(explicitUser ? std::optional<DWORD>{1} : std::nullopt, 3, 2, 4, 5);
			Check(resolved == (explicitUser ? 1u : 2u));
		}
		user.values[ColorizationColorOverride] = { true, REG_BINARY, { 1, 2 } };
		user.values[ColorizationAfterglowOverride] = Encode(Value{ DWORD{0xBB112233} });
		machine.values[ColorizationAfterglowOverride] = Encode(Value{ DWORD{0xBB112233} });
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto cleanup = Plan(user, machine, { { ColorizationColorOverride, DWORD{5} }, { ColorizationAfterglowOverride, DWORD{6} } }, target);
			Check(cleanup.size() == 4);
			for (const auto& change : cleanup) Check(Settings::IsColorOverride(change.id) && change.before.present && !change.after.present);
		}
		user.values[GlassOpacity] = machine.values[GlassOpacity] = Encode(Value{ DWORD{42} });
		const auto same = Plan(user, machine, { { GlassOpacity, DWORD{42} } }, Settings::Scope::Machine);
		Check(same.size() == 4);
		const auto partial = Plan(user, machine, { { ColorizationColor, DWORD{5} }, { MinMaxButtonGlowId, DWORD{6} } }, Settings::Scope::Machine);
		Check(partial.size() == 4); // Only the independent color cleanup, never base/internal settings.
		user.values[CustomThemeReflection] = { true, REG_BINARY, { 1 } };
		machine.values[CustomThemeReflection] = Encode(Value{ std::wstring(L"machine.png") });
		Check(std::get<std::wstring>(Resolve(user, machine, CustomThemeReflection)) == L"machine.png");
		user.values[GlassOpacity] = Encode(Value{ DWORD{12} });
		machine.values[GlassOpacity] = Encode(Value{ DWORD{34} });
		const Model model{ { GlassOpacity, DWORD{56} }, { CustomThemeReflection, std::monostate{} } };
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto plan = Plan(user, machine, model, target);
			using Key = std::pair<Settings::Scope, std::wstring>;
			using RawValue = RegistryConfig::RawValue;
			std::map<Key, RawValue> initial;
			for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
			{
				const auto& layer = scope == Settings::Scope::User ? user : machine;
				for (const auto& [id, raw] : layer.values) if (raw.present) initial[{ scope, std::wstring(Settings::Get(id).name) }] = raw;
			}
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				auto state = initial;
				PreviewJournal<Key, RawValue> journal;
				auto read = [&](const Key& key) { const auto found = state.find(key); return found == state.end() ? RawValue{} : found->second; };
				auto write = [&](const Key& key, const RawValue& value) { if (value.present) state[key] = value; else state.erase(key); return true; };
				journal.Begin();
				for (std::size_t index = 0; index <= failAt; ++index)
				{
					const auto& change = plan[index]; const Key key{ change.scope, change.Name() };
					journal.Touch(key, read); write(key, change.after);
				}
				journal.Reconcile(read);
				const Key outside{ Settings::Scope::User, L"FutureSetting" };
				state[outside] = Encode(Value{ DWORD{99} });
				Check(journal.RollbackAttempt(write));
				Check(state.at(outside) == Encode(Value{ DWORD{99} })); state.erase(outside);
				Check(state == initial);
			}
		}

	}

	void TestConfigurationStrings()
	{
		using namespace EffectiveConfiguration;
		constexpr auto id = Settings::Id::CustomThemeReflection;
		const std::wstring samples[]{ L"", L"user.png", std::wstring(L"user.png\0ignored", 16), L"%TEMP%\\texture.png" };
		for (const DWORD type : { REG_SZ, REG_EXPAND_SZ })
			for (const auto& text : samples)
				for (const bool terminated : { false, true })
				{
					RegistryConfig::RawValue raw{ true, type, std::vector<BYTE>((text.size() + terminated) * sizeof(wchar_t)) };
					if (!raw.bytes.empty()) std::memcpy(raw.bytes.data(), text.c_str(), raw.bytes.size());
					const Value expected{ text.substr(0, text.find(L'\0')) };
					Check(Decode(raw, Settings::Get(id)) == expected);
					Layer user, machine;
					user.values[id] = raw;
					machine.values[id] = Encode(Value{ std::wstring(L"machine.png") });
					Check(Capture(user, machine).at(id) == expected); // An explicit empty path also masks HKLM.
					for (const auto& change : Plan(user, machine, { { id, expected } }, Settings::Scope::Machine))
						Check(change.scope != Settings::Scope::User); // Same-value user strings are harmless.
					Check(ConfigurationMigration::Prepare(user.values, machine.values, Settings::Scope::User).empty());
					const auto merge = ConfigurationMigration::Prepare(user.values, machine.values, Settings::Scope::Machine);
					Check(merge.size() == 2);
					if (merge.size() == 2)
					{
						Check(merge[0].scope == Settings::Scope::Machine && merge[0].after == raw);
						Check(Decode(merge[0].after, Settings::Get(id)) == expected);
						Check(merge[1].scope == Settings::Scope::User && merge[1].before == raw && !merge[1].after.present);
					}
				}
	}

	void TestConfigurationReset()
	{
		using namespace EffectiveConfiguration;
		using RawValue = RegistryConfig::RawValue;
		using Key = std::pair<Settings::Scope, Settings::Id>;
		Layer user, machine;
		for (const auto& spec : Settings::Catalog)
		{
			user.values[spec.id] = spec.type == Settings::ValueType::String
				? Encode(Value{ std::wstring(L"user.png") }) : Encode(Value{ DWORD{17} });
			machine.values[spec.id] = spec.type == Settings::ValueType::String
				? Encode(Value{ std::wstring(L"machine.png") }) : Encode(Value{ DWORD{31} });
		}
		// Explicit reset removes malformed known values too, with exact raw rollback.
		user.values[Settings::Id::MinMaxButtonGlowId] = { true, REG_BINARY, { 9, 8, 7 } };
		const auto unknown = Settings::Id::Count;
		user.values[unknown] = machine.values[unknown] = { true, REG_BINARY, { 1, 2 } };
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			Check(PlanReset({}, target).empty());
			const auto plan = PlanReset(target == Settings::Scope::User ? user : machine, target);
			Check(plan.size() == Settings::Catalog.size() - 5);
			for (const auto id : { Settings::Id::MinMaxButtonGlowId, Settings::Id::CloseButtonGlowId, Settings::Id::ToolCloseButtonGlowId })
				Check(std::ranges::any_of(plan, [&](const auto& change) { return change.id == id; }));
			for (const auto& change : plan)
				Check(change.scope == target && !Settings::IsWindowsColorBase(change.id) && change.id != unknown && !change.after.present);
			auto users = user, machines = machine;
			auto& reset = target == Settings::Scope::User ? users : machines;
			for (const auto& change : plan) reset.values.erase(change.id);
			Check(PlanReset(reset, target).empty());
			Check((target == Settings::Scope::User ? machines.values : users.values) == (target == Settings::Scope::User ? machine.values : user.values));
			Check(reset.values.at(unknown) == user.values.at(unknown));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				Check(reset.values.at(spec.id) == Raw(target == Settings::Scope::User ? user : machine, spec.id));
			Check(Resolve(users, machines, Settings::Id::GlassOpacity) == Value{ DWORD{target == Settings::Scope::User ? 31u : 17u} });
			// Repeating a reset has no attempt changes, even while the first reset
			// still has an undo baseline. It must not require another DWM refresh.
			PreviewJournal<Key, RawValue> preview;
			reset.values[Settings::Id::GlassOpacity] = Encode(Value{ DWORD{42} });
			auto readReset = [&](const Key& key) { return Raw(reset, key.second); };
			for (const bool first : { true, false })
			{
				preview.Begin();
				for (const auto& change : PlanReset(reset, target))
				{
					preview.Touch({ change.scope, change.id }, readReset);
					reset.values.erase(change.id);
				}
				preview.Reconcile(readReset);
				Check(preview.HasAttemptChanges() == first && preview.IsDirty());
				preview.CommitAttempt();
			}
			Check(preview.Revert([&](const Key& key, const RawValue& value) { reset.values[key.second] = value; return true; }));
			Check(Raw(reset, Settings::Id::GlassOpacity) == Encode(Value{ DWORD{42} }));
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				std::map<Key, RawValue> state;
				for (const auto& [id, value] : user.values) state[{ Settings::Scope::User, id }] = value;
				for (const auto& [id, value] : machine.values) state[{ Settings::Scope::Machine, id }] = value;
				auto expected = state;
				PreviewJournal<Key, RawValue> journal;
				auto read = [&](const Key& key) { const auto found = state.find(key); return found == state.end() ? RawValue{} : found->second; };
				auto write = [&](const Key& key, const RawValue& value) { if (value.present) state[key] = value; else state.erase(key); return true; };
				journal.Begin();
				for (std::size_t index = 0; index <= failAt; ++index)
				{
					const auto& change = plan[index]; const Key key{ change.scope, change.id };
					journal.Touch(key, read); write(key, change.after);
				}
				const Key untouched{ target == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, Settings::Id::GlassOpacity };
				state[untouched] = expected[untouched] = Encode(Value{ DWORD{77} });
				Check(journal.RollbackAttempt(write)); Check(state == expected);
			}
		}
	}

	void TestShellColorRefresh()
	{
		// Real cross-thread window delivery, with a receiver modeling the audited
		// one-shot pending flag. No Explorer, private API or registry is touched.
		struct Receiver
		{
			DWORD color{};
			bool pending{ true }, reject{};
		};
		const auto instance = GetModuleHandleW(nullptr);
		constexpr auto className = L"OpenGlassTests.ColorRefresh";
		WNDCLASSW windowClass{};
		windowClass.hInstance = instance;
		windowClass.lpszClassName = className;
		windowClass.lpfnWndProc = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT
		{
			if (message == WM_NCCREATE)
				SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
			auto* receiver = reinterpret_cast<Receiver*>(GetWindowLongPtrW(window, GWLP_USERDATA));
			if (message == 0x52C && wparam == 14 && receiver)
			{
				if (receiver->reject) return E_ACCESSDENIED;
				if (lparam == static_cast<LPARAM>(0xFFFFFFFFull) || receiver->pending) receiver->color = 0x123456;
				receiver->pending = false;
				return S_OK;
			}
			if (message == WM_APP && receiver) { receiver->color = 0x74B8FC; return 0; } // Sky
			if (message == WM_APP + 1 && receiver) return receiver->color;
			if (message == WM_APP + 2 && receiver) { receiver->reject = true; return 0; }
			if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
			return DefWindowProcW(window, message, wparam, lparam);
		};
		Check(RegisterClassW(&windowClass) != 0);
		auto unregister = wil::scope_exit([&] { UnregisterClassW(className, instance); });
		std::promise<HWND> ready;
		auto future = ready.get_future();
		std::jthread thread([&]
		{
			Receiver receiver;
			const auto window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, &receiver);
			ready.set_value(window);
			if (!window) return;
			MSG message{};
			while (GetMessageW(&message, nullptr, 0, 0) > 0) DispatchMessageW(&message);
		});
		const auto window = future.get();
		auto close = wil::scope_exit([&] { if (window) PostMessageW(window, WM_CLOSE, 0, 0); });
		Check(window != nullptr);
		if (!window) return;
		Check(SUCCEEDED(ShellColorRefresh::Request(window)));
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x123456);
		SendMessageW(window, WM_APP, 0, 0);
		// Reproduce the bug: ordinary refresh has no pending wallpaper result.
		SendMessageW(window, 0x52C, 14, 0);
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x74B8FC);
		Check(SUCCEEDED(ShellColorRefresh::Request(window)));
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x123456);
		SendMessageW(window, WM_APP, 0, 0);
		Check(SendMessageW(window, WM_APP + 1, 0, 0) == 0x74B8FC); // No late posted refresh.
		SendMessageW(window, WM_APP + 2, 0, 0);
		Check(ShellColorRefresh::Request(window) == E_ACCESSDENIED);
		Check(FAILED(ShellColorRefresh::Request(nullptr)));
	}

	void TestAutomaticColorPreview()
	{
		using Snapshot = ColorPreference::Snapshot;
		struct Backend final : ColorPreference::Backend
		{
			Snapshot live;
			DWORD wallpaper{ 0x123456 }, derived{};
			bool reject{}, fail{}, failAfterAccent{}, failAfterDwmAccent{};
			unsigned writes{}, refreshes{};
			HRESULT Capture(const std::wstring&, Snapshot& value) noexcept override { value = live; return S_OK; }
			HRESULT Prepare(const Snapshot&) noexcept override { return reject ? E_NOINTERFACE : S_OK; }
			HRESULT Apply(const Snapshot& value, Snapshot* applied) noexcept override
			{
				++writes;
				if (value.applyChoice) live.automatic = value.automatic; // Failure may follow a partial mutation.
				if (fail) return E_FAIL;
				if (value.accent && !value.RestoresAutomatic()) live.accent = value.accent;
				if (value.applyChoice)
				{
					live.rgb = value.rgb;
					if (value.IsAutomatic()) { ++refreshes; derived = wallpaper; }
					else derived = *value.rgb;
				}
				if (value.RestoresAutomatic() && live.accent)
					live.accent = RegistryConfig::RawValue{ true, REG_DWORD,
						{ static_cast<BYTE>(wallpaper >> 16), static_cast<BYTE>(wallpaper >> 8), static_cast<BYTE>(wallpaper), 0xFF } };
				if (failAfterAccent) return E_FAIL;
				if (value.RestoresAutomatic() && live.dwmAccent)
					live.dwmAccent = RegistryConfig::RawValue{ true, REG_DWORD,
						{ static_cast<BYTE>(wallpaper >> 16), static_cast<BYTE>(wallpaper >> 8), static_cast<BYTE>(wallpaper), 0xFF } };
				else if (value.dwmAccent) live.dwmAccent = value.dwmAccent;
				if (failAfterDwmAccent) return E_FAIL;
				if (applied)
				{
					*applied = value;
					if (value.RestoresAutomatic())
					{
						applied->accent = live.accent;
						applied->dwmAccent = live.dwmAccent;
					}
				}
				return S_OK;
			}
		};
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			ColorPreference fresh(std::move(backend));
			const Snapshot persisted{ DWORD{0}, DWORD{0xABCDEF} };
			Check(SUCCEEDED(fresh.RecoverSnapshot(persisted)) && live->writes == 1 && live->live == persisted && !fresh.IsDirty());
			live->reject = true;
			Check(FAILED(fresh.RecoverSnapshot(persisted)) && live->writes == 1);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			Snapshot persisted{ 1u, std::nullopt };
			persisted.accent = RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } };
			ColorPreference preview(std::move(backend));
			// An interrupted Manual edit may have an Automatic baseline and an
			// independent raw Accent backup. Restore it before recomputing Automatic.
			live->reject = true;
			Check(FAILED(preview.RecoverSnapshot(persisted)) && live->writes == 0 && !live->live.IsAutomatic());
			live->reject = false;
			live->fail = true;
			Check(FAILED(preview.RecoverSnapshot(persisted)) && live->refreshes == 0 && !live->live.IsAutomatic());
			live->fail = false;
			Check(SUCCEEDED(preview.RecoverSnapshot(persisted)) && live->live.IsAutomatic() && live->refreshes == 1
				&& live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x112233u };
			// Restore raw Accent independently of the original DWM RGB, including its type.
			original.accent = RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } };
			live->live = original;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566)) && live->live.accent->type == REG_DWORD);
			preview.CommitAttempt();
			live->live.accent.reset(); // Policy now prevents forward accent synchronization.
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899)) && !live->live.accent);
			preview.CommitAttempt();
			const auto baseline = preview.Baseline();
			Check(baseline && baseline->accent == original.accent);
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
			live->live.accent.reset();
			Check(SUCCEEDED(preview.RecoverSnapshot(*baseline)) && live->live == original);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x112233u };
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			// Accent first becomes writable after an earlier DWM-only preview.
			live->live.accent = RegistryConfig::RawValue{};
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			Check(preview.Baseline()->accent && !preview.Baseline()->accent->present);
			Check(SUCCEEDED(preview.Revert()) && live->live.rgb == DWORD{0x112233}
				&& live->live.accent && !live->live.accent->present);
		}

		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0xFF } };
			ColorPreference preview(std::move(backend));
			bool recordedAccent{};
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233, [&](const Snapshot& before) { recordedAccent = before.accent.has_value(); })));
			Check(!recordedAccent && !preview.Baseline()->accent); // Equal Accent produces no write or backup.
			preview.CommitAttempt();
			const RegistryConfig::RawValue external{ true, REG_DWORD, { 7, 8, 9, 0xFF } };
			live->live.accent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.rgb == DWORD{0x445566} && live->live.accent == external);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x112233u };
			live->live.accent = RegistryConfig::RawValue{};
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233))); preview.CommitAttempt();
			Check(preview.IsDirty() && !preview.Baseline()->applyChoice && preview.Baseline()->accent);
			live->live.automatic = 1; // Untouched mode changed externally during an Accent-only preview.
			Check(SUCCEEDED(preview.Revert()) && live->live.automatic == 1 && !live->live.accent->present);
		}
		for (const auto& raw : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } },
			RegistryConfig::RawValue{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0x77 } } })
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x112233u }; original.dwmAccent = raw;
			live->live = original; // Forced-color policy may exclude AccentColorMenu, never DWM AccentColor.
			ColorPreference preview(std::move(backend));
			Snapshot persisted;
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566, [&](const Snapshot& before) { persisted = before; })));
			Check(!live->live.accent && live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x44, 0x55, 0x66, 0xFF } });
			Check(persisted.dwmAccent == raw); preview.CommitAttempt();
			const auto first = live->live;
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", 0x778899)));
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == first && preview.IsDirty());
			// Automatic may replace AccentColor before a later step fails; preserve the Manual attempt baseline.
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == first && preview.IsDirty());
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt();
			Check(live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
			Check(SUCCEEDED(preview.Revert()) && live->live == original); // Includes Manual -> Automatic.
			Check(SUCCEEDED(preview.RecoverSnapshot(persisted)) && live->live == original);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x112233u };
			const RegistryConfig::RawValue raw{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0xFF } };
			live->live.dwmAccent = raw;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233, [&](const Snapshot& before) { Check(!before.dwmAccent); })));
			Check(!preview.IsDirty()); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233)) && !preview.IsDirty()); preview.CommitAttempt();
			// An equal, untouched DWM Accent survives Revert of a different Manual RGB.
			live->live.rgb = 0x778899;
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233))); preview.CommitAttempt();
			Check(!preview.Baseline()->dwmAccent);
			const RegistryConfig::RawValue external{ true, REG_BINARY, { 7, 8, 9 } };
			live->live.dwmAccent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.dwmAccent == external);
			preview.Accept();
			// A DWM Accent-only correction must not own mode or Manual RGB.
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			Check(!preview.Baseline()->applyChoice && preview.Baseline()->dwmAccent == external);
			live->live.automatic = 1;
			live->fail = true;
			Check(FAILED(preview.Revert()) && preview.IsDirty());
			live->fail = false;
			Check(SUCCEEDED(preview.Revert()) && live->live.automatic == 1 && live->live.dwmAccent == external);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.dwmAccent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566, [](const Snapshot& before)
			{
				Check(before.RestoresAutomatic() && !before.dwmAccent); // Durable recovery must recompute it.
			}))); preview.CommitAttempt();
			Check(!preview.Baseline()->dwmAccent);
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty()); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Revert()) && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } });
			preview.Accept();
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233)));
			Check(SUCCEEDED(preview.RollbackAttempt()) && !preview.IsDirty() && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } });
		}
		for (const auto& raw : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 7, 8, 9 } },
			RegistryConfig::RawValue{ true, REG_DWORD, { 0x44, 0x55, 0x66, 0xFF } } })
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x445566u };
			original.accent = raw; original.dwmAccent = raw;
			live->live = original;
			ColorPreference preview(std::move(backend));
			Snapshot recovery;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt, [&](const Snapshot& before) { recovery = before; })));
			Check(live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } }
				&& live->live.dwmAccent == live->live.accent);
			preview.CommitAttempt();
			Check(preview.Baseline()->accent == raw && preview.Baseline()->dwmAccent == raw);
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
			Check(SUCCEEDED(preview.RecoverSnapshot(recovery)) && live->live == original);
			preview.Accept();
			live->failAfterAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && live->live.accent != raw && live->live.dwmAccent == raw);
			live->failAfterAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == original && !preview.IsDirty());
			// A correction failure retains both raw values for retry, including absence/type.
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && preview.IsDirty());
			Check(FAILED(preview.RollbackAttempt()) && preview.IsDirty());
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.Baseline()->accent);
			preview.CommitAttempt();
			const RegistryConfig::RawValue external{ true, REG_BINARY, { 9, 8, 7 } };
			live->live.accent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.accent == external); // Equal target remains unowned.
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			live->live.dwmAccent = live->live.accent;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty()); preview.CommitAttempt();
			Check(live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } }
				&& live->live.dwmAccent == live->live.accent); // Automatic owns derived Accent state.
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			live->live.dwmAccent = live->live.accent;
			ColorPreference preview(std::move(backend));
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty());
			// A later preset/Override step can fail after a successful same-mode refresh.
			Check(preview.Baseline(true) && preview.Baseline(true)->RestoresAutomatic());
			live->wallpaper = 0x778899;
			Check(SUCCEEDED(preview.RollbackAttempt()) && !preview.IsDirty() && live->refreshes == 2
				&& live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x77, 0x88, 0x99, 0xFF } }
				&& live->live.dwmAccent == live->live.accent);
		}
		for (const DWORD mode : { 0u, 1u, 2u })
		{
			auto backend = std::make_unique<Backend>();
			auto& live = *backend;
			const Snapshot original{ mode, mode ? std::nullopt : std::optional<DWORD>{0x112233} };
			live.live = original;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)));
			Check(preview.IsAttemptActive()); // Automatic may refresh RGB without changing its mode.
			Check(preview.IsDirty() == (original != Snapshot{ 1u, std::nullopt }));
			Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == original && !preview.IsDirty());
			Check(!preview.IsAttemptActive());
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0))); preview.CommitAttempt();
			live.wallpaper = 0x778899;
			Check(SUCCEEDED(preview.Revert()) && live.live == original);
			if (original.IsAutomatic()) Check(live.derived == live.wallpaper); // Recompute current wallpaper.
			preview.Accept(); Check(!preview.IsDirty());
		}
		auto backend = std::make_unique<Backend>();
		auto& live = *backend;
		const Snapshot original{ 0u, 0x112233u };
		live.live = original;
		ColorPreference preview(std::move(backend));
		Check(SUCCEEDED(preview.Apply(L"user", 0xFF112233)) && !preview.IsDirty()); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
		const auto a = live.live;
		live.reject = true;
		const auto writes = live.writes;
		Check(FAILED(preview.Apply(L"user", std::nullopt)) && writes == live.writes);
		Check(!preview.IsAttemptActive()); // Capability rejection happens before color mutation.
		Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == a && preview.IsDirty());
		live.reject = false; live.fail = true;
		Check(FAILED(preview.Apply(L"user", std::nullopt)));
		live.fail = false;
		Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == a && preview.IsDirty());
		Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
		// Untracked Windows output never becomes part of the GUI checkpoint.
		live.derived = 0xABCDEF;
		Check(preview.IsDirty());
		live.fail = true;
		Check(FAILED(preview.Revert()) && preview.IsDirty());
		live.fail = false;
		Check(SUCCEEDED(preview.Revert()) && live.live == original); preview.Accept();
		Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", *original.rgb)) && !preview.IsDirty()); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt(); preview.Accept();
		live.wallpaper = 0xABCDEF; live.derived = live.wallpaper;
		Check(!preview.IsDirty());
		Check(SUCCEEDED(preview.Apply(L"user", 0))); preview.CommitAttempt();
		live.wallpaper = 0xFEDCBA;
		Check(SUCCEEDED(preview.Revert()) && live.live.IsAutomatic() && live.derived == 0xFEDCBA); preview.Accept();
		// Failed recovery must keep a retryable baseline even after attempt rollback.
		live.fail = true;
		Check(FAILED(preview.Apply(L"user", 0x123456)));
		Check(FAILED(preview.RollbackAttempt()) && preview.IsDirty());
		live.fail = false;
		Check(SUCCEEDED(preview.Revert()) && live.live.IsAutomatic()); preview.Accept();
	}

	void TestNetPreviewJournal()
	{
		PreviewJournal<int, int> journal;
		std::map<int, int> state{ {1, 10}, {2, 20}, {3, 30} };
		auto read = [&](int key) { return state.at(key); };
		auto write = [&](int key, int value) { state[key] = value; return true; };
		auto edit = [&](int key, int value)
		{
			journal.Begin(); journal.Touch(key, read); state[key] = value; journal.Reconcile(read); journal.CommitAttempt();
		};
		journal.Begin(); journal.Reconcile(read);
		Check(!journal.HasAttemptChanges() && !journal.IsDirty());
		Check(journal.RollbackAttempt([](int, int) { return false; })); // Nothing to restore.
		edit(1, 10); Check(!journal.IsDirty());
		edit(1, 11); Check(journal.IsDirty());
		journal.Begin(); journal.Touch(1, read); journal.Reconcile(read);
		Check(!journal.HasAttemptChanges() && journal.IsDirty());
		Check(journal.RollbackAttempt([](int, int) { return false; }) && journal.IsDirty());
		journal.Begin(); journal.Touch(1, read); state[1] = 10; journal.Reconcile(read);
		Check(journal.HasAttemptChanges() && !journal.IsDirty()); // Returning to baseline still needs a refresh.
		journal.CommitAttempt(); Check(!journal.HasAttemptChanges());
		edit(1, 12); state[2] = 200; state[1] = 999; // An external edit to an owned key does not discard its baseline.
		edit(3, 31); Check(journal.Revert(write)); Check(state[1] == 10 && state[2] == 200 && state[3] == 30);
		edit(1, 11);
		journal.Begin(); journal.Touch(1, read); state[1] = 10; journal.Reconcile(read);
		Check(!journal.RollbackAttempt([](int, int) { return false; }));
		Check(journal.IsDirty()); Check(journal.Revert(write)); Check(state[1] == 10);
		edit(1, 40); journal.Accept(); edit(1, 41); Check(journal.Revert(write)); Check(state[1] == 40);
	}

	void TestConfigurationMergePolicy()
	{
		using enum Settings::Id;
		using Raw = RegistryConfig::RawValue;
		using Values = ConfigurationMigration::Values;
		auto raw = [](DWORD value) { return EffectiveConfiguration::Encode(EffectiveConfiguration::Value{ value }); };
		const Values user{ { GlassOpacity, raw(40) }, { ColorizationColorBalance, raw(33) },
			{ ColorizationAfterglowBalance, raw(12) }, { ColorizationBlurBalance, raw(55) },
			{ ColorizationColor, raw(0x8074B8FC) }, { ColorizationAfterglow, raw(0x80ABCDEF) } };
		const Values machine{ { GlassOpacity, raw(60) }, { ColorizationColorBalanceOverride, raw(70) },
			{ CustomThemeReflection, EffectiveConfiguration::Encode(EffectiveConfiguration::Value{ std::wstring(L"machine.png") }) },
			{ ColorizationColor, raw(0xFF123456) }, { ColorizationAfterglow, raw(0xFF123456) } };
		auto apply = [](Values& users, Values& machines, const auto& plan)
		{
			for (const auto& change : plan)
			{
				auto& layer = change.scope == Settings::Scope::User ? users : machines;
				if (change.after.present) layer[change.id] = change.after;
				else layer.erase(change.id);
			}
		};
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			auto withUserOverrides = user, withMachineOverrides = machine;
			withUserOverrides[ColorizationColorOverride] = raw(0xAA123456);
			withMachineOverrides[ColorizationAfterglowOverride] = raw(0xBB112233);
			const auto ignored = ConfigurationMigration::Prepare(withUserOverrides, withMachineOverrides, target);
			for (const auto& change : ignored) Check(!Settings::IsColorOverride(change.id));
			apply(withUserOverrides, withMachineOverrides, ignored);
			Check(withUserOverrides.at(ColorizationColorOverride) == raw(0xAA123456));
			Check(withMachineOverrides.at(ColorizationAfterglowOverride) == raw(0xBB112233));
			Check(!withMachineOverrides.contains(ColorizationColorOverride) && !withUserOverrides.contains(ColorizationAfterglowOverride));
			const auto plan = ConfigurationMigration::Prepare(user, machine, target);
			bool deleting{};
			for (const auto& change : plan)
			{
				Check(!Settings::IsWindowsColorBase(change.id) && !Settings::IsColorOverride(change.id));
				if (target == Settings::Scope::User) Check(change.scope == target && change.after.present);
				else
				{
					if (change.scope == Settings::Scope::User) { deleting = true; Check(!change.after.present); }
					else Check(!deleting); // Complete destination writes before deleting sources.
					Check(change.id != ColorizationColorBalanceOverride); // Machine Override already wins.
				}
			}
			auto users = user, machines = machine;
			apply(users, machines, plan);
			const auto& destination = target == Settings::Scope::User ? users : machines;
			Check(destination.at(GlassOpacity) == raw(40));
			Check(destination.at(ColorizationColorBalanceOverride) == raw(70));
			Check(destination.at(ColorizationAfterglowBalanceOverride) == raw(12));
			Check(destination.at(ColorizationBlurBalanceOverride) == raw(55));
			Check(!destination.contains(ColorizationAfterglowOverride));
			Check(destination.at(CustomThemeReflection) == machine.at(CustomThemeReflection));
			if (target == Settings::Scope::User) Check(machines == machine);
			else Check(!users.contains(GlassOpacity));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
			{
				if (user.contains(spec.id)) Check(users.at(spec.id) == user.at(spec.id));
				else Check(!users.contains(spec.id));
				if (machine.contains(spec.id)) Check(machines.at(spec.id) == machine.at(spec.id));
				else Check(!machines.contains(spec.id));
			}
			Check(ConfigurationMigration::Prepare(users, machines, target).empty());
			// Every failed step restores both the destination and any deleted source.
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				using Key = std::pair<Settings::Scope, Settings::Id>;
				std::map<Key, Raw> state;
				for (const auto& [id, value] : user) state[{ Settings::Scope::User, id }] = value;
				for (const auto& [id, value] : machine) state[{ Settings::Scope::Machine, id }] = value;
				const auto before = state;
				PreviewJournal<Key, Raw> journal; journal.Begin();
				for (std::size_t n = 0; n <= failAt; ++n)
				{
					const auto& change = plan[n]; const Key key{ change.scope, change.id };
					journal.Touch(key, [&](const Key& k) { const auto it = state.find(k); return it == state.end() ? Raw{} : it->second; });
					if (change.after.present) state[key] = change.after; else state.erase(key);
				}
				Check(journal.RollbackAttempt([&](const Key& key, const Raw& value) { if (value.present) state[key] = value; else state.erase(key); return true; }));
				Check(state == before);
			}
			users = user;
			users[GlassOpacity] = { true, REG_BINARY, { 0, 1 } };
			const auto malformed = ConfigurationMigration::Prepare(users, machine, target);
			if (target == Settings::Scope::Machine)
				Check(std::ranges::none_of(malformed, [](const auto& change) { return change.id == GlassOpacity; }));
			else
			{
				const auto replacement = std::ranges::find(malformed, GlassOpacity, &ConfigurationMigration::Change::id);
				Check(replacement != malformed.end() && replacement->before == users.at(GlassOpacity) && replacement->after == raw(60));
			}
		}
		// Odd byte counts cannot represent a complete UTF-16 sequence.
		for (const auto badPath : { Raw{ true, REG_SZ, { 'x' } }, Raw{ true, REG_EXPAND_SZ, { 'x', 0, 'y' } } })
		{
			const Values malformedUser{ { CustomThemeReflection, badPath } };
			Check(std::holds_alternative<std::monostate>(EffectiveConfiguration::Decode(badPath, Settings::Get(CustomThemeReflection))));
			const auto toMachine = ConfigurationMigration::Prepare(malformedUser, machine, Settings::Scope::Machine);
			Check(std::ranges::none_of(toMachine, [](const auto& change) { return change.id == CustomThemeReflection; }));
			const auto toUser = ConfigurationMigration::Prepare(malformedUser, machine, Settings::Scope::User);
			const auto replacement = std::ranges::find(toUser, CustomThemeReflection, &ConfigurationMigration::Change::id);
			Check(replacement != toUser.end() && replacement->before == badPath && replacement->after == machine.at(CustomThemeReflection));
		}
		// Compatibility merge preserves all four lookup positions without changing base values.
		for (unsigned uo = 0; uo < 3; ++uo) for (unsigned mo = 0; mo < 3; ++mo)
		for (unsigned ub = 0; ub < 3; ++ub) for (unsigned mb = 0; mb < 3; ++mb)
		{
			auto value = [&](unsigned kind, DWORD number) { return kind == 0 ? Raw{} : kind == 1 ? raw(number) : Raw{ true, REG_BINARY, { 1, 2 } }; };
			const Values users{ { ColorizationColorBalanceOverride, value(uo, 10) }, { ColorizationColorBalance, value(ub, 30) } };
			const Values machines{ { ColorizationColorBalanceOverride, value(mo, 20) }, { ColorizationColorBalance, value(mb, 40) } };
			const auto expected = uo == 1 ? raw(10) : mo == 1 ? raw(20) : ub == 1 ? raw(30) : mb == 1 ? raw(40) : Raw{};
			for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
			{
				auto mergedUsers = users, mergedMachines = machines;
				const auto plan = ConfigurationMigration::Prepare(users, machines, target);
				apply(mergedUsers, mergedMachines, plan);
				if (expected.present) Check((target == Settings::Scope::User ? mergedUsers : mergedMachines).at(ColorizationColorBalanceOverride) == expected);
				else Check(plan.empty());
				Check(mergedUsers.at(ColorizationColorBalance) == users.at(ColorizationColorBalance));
				Check(mergedMachines.at(ColorizationColorBalance) == machines.at(ColorizationColorBalance));
				if (target == Settings::Scope::User) Check(mergedMachines == machines);
			}
		}
	}

	void TestPresetProvenance()
	{
		PresetPackages::PreviewProvenance state;
		PresetPackages::Package a, b, c;
		a.metadata.uuid = "A"; a.libraryId = "entry-a"; a.digest = "A"; a.metadata.authorName = L"Author A"; a.sourceType = "local";
		b.metadata.uuid = "B"; b.libraryId = "entry-b"; b.digest = "B"; b.metadata.authorName = L"Author B"; b.sourceType = "imported";
		c.metadata.uuid = "C"; c.libraryId = "entry-c"; c.digest = "C"; c.sourceType = "local-copy";
		Check(!state.Origin());
		state.SetOrigin(a, true); state.Revert(); Check(!state.Origin());
		state.SetOrigin(a, false);
		state.SetOrigin(b, true); state.SetOrigin(c, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Author A");
		a.metadata.authorName = L"Updated author A";
		state.SetOrigin(a, false); // A saved snapshot retains known notices.
		state.SetOrigin(b, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Updated author A");
		state.SetOrigin(b, true); state.Accept(); state.SetOrigin(c, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Author B");
		Check(!PresetPackages::HasInheritedMetadata(a));
		Check(PresetPackages::HasInheritedMetadata(b) && PresetPackages::HasInheritedMetadata(c));
		a.attribution = { L"Known source" }; Check(!PresetPackages::HasInheritedMetadata(a));
		a.legacyLicense = true; Check(PresetPackages::HasInheritedMetadata(a));
		a.legacyLicense = false; a.digest = "digest-A"; a.assets["assets/reflection.png"] = { std::byte{1} };

		b.digest = "digest-B"; b.attribution = { L"Earlier source" }; b.licenseText = "B image terms";
		PresetPackages::CreateRequest update;
		update.metadata = a.metadata;
		update.licenseText = "Edited local terms";
		PresetPackages::PreserveRevisionProvenance(update, &a, &b);
		Check(update.metadata.authorName == a.metadata.authorName);
		Check(std::ranges::find(update.attribution, L"Earlier source") != update.attribution.end());
		Check(std::ranges::any_of(update.attribution, [](const auto& value) { return value.find(L"Author B") != std::wstring::npos; }));
		Check(update.licenseText == "Edited local terms" && update.inheritedLicenses.empty()); // Config-only source does not import image terms.
		const auto notices = update.attribution;
		PresetPackages::PreserveSource(update, b, false);
		Check(update.attribution == notices); // Repeated capture is deduplicated.
		PresetPackages::PreserveSource(update, b, true);
		Check(update.licenseText == "Edited local terms\n\nB image terms");
		Check(update.inheritedLicenses == std::vector<std::string>{ "B image terms" });
		b.legacyLicense = true;
		update.licenseText = "Edited local terms"; update.inheritedLicenses.clear();
		PresetPackages::PreserveRevisionProvenance(update, &a, &b);
		Check(update.legacyLicense && update.licenseText == "Edited local terms\n\nB image terms"); // Legacy package terms also cover configuration.

	}

	void TestPresetPackageRoundTrip()
	{
		auto createArchive = [](const std::filesystem::path& path, const PresetPackages::CreateRequest& request)
		{
			PresetPackages::ExportArchive(path, PresetPackages::CreateSnapshot(request));
		};
		Check(PresetPackages::IsValidHomepageUrl(L"https://example.com"));
		Check(PresetPackages::IsValidHomepageUrl(L"http://example.com/author"));
		Check(!PresetPackages::IsValidHomepageUrl(L"https://"));
		Check(!PresetPackages::IsValidHomepageUrl(L"example.com"));
		Check(!PresetPackages::IsValidHomepageUrl(L"file:///C:/preset"));

		wxInitializer initializer;
		Check(initializer.IsOk());
		if (!initializer.IsOk()) return;
		wxLogNull suppressExpectedArchiveErrors;

		const auto generatedUuid = PresetPackages::GeneratePackageUuid();
		const std::wstring generatedUuidWide(generatedUuid.begin(), generatedUuid.end());
		const auto directory = std::filesystem::temp_directory_path() / (L"OpenGlassPresetTests-" + generatedUuidWide);
		std::filesystem::create_directories(directory);
		auto cleanup = wil::scope_exit([&]
		{
			for (const auto& path : std::filesystem::recursive_directory_iterator(directory)) SetFileAttributesW(path.path().c_str(), FILE_ATTRIBUTE_NORMAL);
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		});

		PresetPackages::CreateRequest request;
		request.metadata = {
			"00112233-4455-6677-8899-aabbccddeeff",
			L"Round trip",
			L"Preset package test",
			L"OpenGlass tests",
			L"https://example.com/author",
			L"MIT"
		};
		request.accentColor = 0x74B8FC;
		request.licenseText = "MIT License\n\nPermission is granted for this test package.\n";
		for (const auto& spec : Settings::Catalog)
		{
			if (Settings::IsPresetPackSetting(spec)) request.settings.emplace(spec.id, std::monostate{});
		}

		const auto first = directory / L"first.zip";
		const auto second = directory / L"second.zip";
		createArchive(first, request);
		createArchive(second, request);
		auto readFile = [](const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::vector<char>((std::istreambuf_iterator<char>(stream)), {});
		};
		Check(readFile(first) == readFile(second));
		Check((GetFileAttributesW(first.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);

		const auto loaded = PresetPackages::LoadArchive(first);
		Check(nlohmann::json::parse(loaded.manifestText).at("schema_version") == 3);
		Check(!loaded.legacyLicense);
		auto noHomepage = request; noHomepage.metadata.authorHomepage.clear();
		Check(PresetPackages::CreateSnapshot(noHomepage).metadata.authorHomepage.empty());
		Check(loaded.accentColor == request.accentColor);
		Check(loaded.metadata.uuid == request.metadata.uuid);
		Check(loaded.metadata.name == request.metadata.name);
		Check(loaded.metadata.authorHomepage == request.metadata.authorHomepage);
		Check(loaded.metadata.licenseName == request.metadata.licenseName);
		Check(loaded.settings.size() == Settings::PresetPackSettingCount());
		Check(loaded.catalogVersion == Settings::CatalogVersion);
		Check(loaded.assets.empty());
		Check(!loaded.digest.empty());
		Check(PresetPackages::InferLicenseName("SPDX-License-Identifier: Apache-2.0\n") == L"Apache-2.0");
		Check(PresetPackages::InferLicenseName("GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n") == L"GPL-3.0");
		Check(PresetPackages::InferLicenseName(
			"GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n"
			+ std::string(5000, 'x')
			+ "GNU Affero General Public License\n"
		) == L"GPL-3.0");
		Check(PresetPackages::InferLicenseName("GNU AFFERO GENERAL PUBLIC LICENSE\nVersion 3, 19 November 2007\n") == L"AGPL-3.0");
		Check(PresetPackages::InferLicenseName("Terms for this package.\n") == L"Custom license");

		auto unlicensedRequest = request;
		unlicensedRequest.metadata.uuid = "10112233-4455-6677-8899-aabbccddeeff";
		unlicensedRequest.metadata.licenseName.clear();
		unlicensedRequest.licenseText.clear();
		const auto unlicensedArchive = directory / L"unlicensed.zip";
		createArchive(unlicensedArchive, unlicensedRequest);
		const auto unlicensed = PresetPackages::LoadArchive(unlicensedArchive);
		Check(unlicensed.licenseText.empty());
		Check(unlicensed.metadata.licenseName.empty());

		auto undescribedRequest = request;
		undescribedRequest.metadata.uuid = "20112233-4455-6677-8899-aabbccddeeff";
		undescribedRequest.metadata.description.clear();
		const auto undescribedArchive = directory / L"undescribed.zip";
		createArchive(undescribedArchive, undescribedRequest);
		const auto undescribed = PresetPackages::LoadArchive(undescribedArchive);
		Check(undescribed.metadata.description.empty());

		const auto& png = ValidPng();
		const auto validPng = directory / L"reflection.png";
		{
			std::ofstream output(validPng, std::ios::binary);
			output.write(reinterpret_cast<const char*>(png.data()), png.size());
		}
		auto assetRequest = request;
		assetRequest.metadata.uuid = "11112233-4455-6677-8899-aabbccddeeff";
		assetRequest.settings[Settings::Id::CustomThemeReflection] = PresetPackages::AssetReference{ "assets/reflection.png" };
		assetRequest.assetSources.emplace("assets/reflection.png", validPng);
		const auto assetArchive = directory / L"asset.zip";
		createArchive(assetArchive, assetRequest);
		const auto loadedAsset = PresetPackages::LoadArchive(assetArchive);
		Check(loadedAsset.assets.contains("assets/reflection.png"));
		Check(std::get<PresetPackages::AssetReference>(loadedAsset.settings.at(Settings::Id::CustomThemeReflection)).path == "assets/reflection.png");
		const auto deployed = directory / L"deployed";
		std::filesystem::create_directories(deployed / L"assets");
		auto writeBytes = [](const std::filesystem::path& path, std::span<const std::byte> bytes)
		{
			std::ofstream output(path, std::ios::binary);
			output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		};
		writeBytes(deployed / L"manifest.json", { reinterpret_cast<const std::byte*>(loadedAsset.manifestText.data()), loadedAsset.manifestText.size() });
		writeBytes(deployed / L"LICENSE", { reinterpret_cast<const std::byte*>(loadedAsset.licenseText.data()), loadedAsset.licenseText.size() });
		for (const auto& [name, bytes] : loadedAsset.assets)
		{
			writeBytes(deployed / wxString::FromUTF8(name).ToStdWstring(), bytes);
		}
		const auto loadedDeployment = PresetPackages::LoadDeployed(deployed);
		Check(loadedDeployment.digest == loadedAsset.digest);
		Check(loadedDeployment.deployed);

		const auto validLayout = directory / L"theme-atlas.png.layout";
		{
			std::ofstream output(validLayout, std::ios::binary);
			output << "RS1Compatibility=1\n12;1;3602=1,2,3,4\nCaptionHeight=22\n";
		}
		auto atlasRequest = request;
		atlasRequest.metadata.uuid = "31112233-4455-6677-8899-aabbccddeeff";
		atlasRequest.settings[Settings::Id::CustomThemeAtlas] = PresetPackages::AssetReference{ "assets/theme-atlas.png" };
		atlasRequest.assetSources.emplace("assets/theme-atlas.png", validPng);
		atlasRequest.assetSources.emplace("assets/theme-atlas.png.layout", validLayout);
		const auto atlasArchive = directory / L"atlas.zip";
		createArchive(atlasArchive, atlasRequest);
		const auto loadedAtlas = PresetPackages::LoadArchive(atlasArchive);
		Check(loadedAtlas.assets.contains("assets/theme-atlas.png.layout"));

		const auto invalidLayout = directory / L"invalid.layout";
		{
			std::ofstream output(invalidLayout, std::ios::binary);
			output << "12;1;3602=1,2,3,4,5\n";
		}
		atlasRequest.metadata.uuid = "41112233-4455-6677-8899-aabbccddeeff";
		atlasRequest.assetSources["assets/theme-atlas.png.layout"] = invalidLayout;
		bool rejectedLayout{};
		try { createArchive(directory / L"bad-layout.zip", atlasRequest); }
		catch (...) { rejectedLayout = true; }
		Check(rejectedLayout);

		bool rejectedOverwrite{};
		try { createArchive(first, request); }
		catch (...) { rejectedOverwrite = true; }
		Check(rejectedOverwrite);

		auto invalid = request;
		invalid.metadata.authorHomepage = L"file:///not-allowed";
		bool rejectedUrl{};
		try { createArchive(directory / L"bad-url.zip", invalid); }
		catch (...) { rejectedUrl = true; }
		Check(rejectedUrl);

		invalid = request;
		invalid.settings.erase(Settings::Id::GlassType);
		bool rejectedIncompleteCatalog{};
		try { createArchive(directory / L"bad-catalog.zip", invalid); }
		catch (...) { rejectedIncompleteCatalog = true; }
		Check(rejectedIncompleteCatalog);

		// A library entry owns only its current version; fixtures never touch ProgramData.
		const auto library = directory / L"library";
		const auto snapshot = PresetPackages::CreateSnapshot(assetRequest);
		const auto originalImage = readFile(validPng);
		{ std::ofstream changed(validPng, std::ios::binary | std::ios::trunc); changed << "changed after capture"; }
		const auto local = PresetPackages::Publish(snapshot, "local", {}, library);
		Check(local.trusted && PresetPackages::LoadTrusted(local, library).assets == snapshot.assets);
		{ std::ofstream restored(validPng, std::ios::binary | std::ios::trunc); restored.write(originalImage.data(), originalImage.size()); }
		Check(!std::filesystem::exists(library / L"library.json"));
		{
			wil::unique_hfile writer(CreateFileW((library / L"library.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(writer));
			bool busy{}; try { (void)PresetPackages::EnumerateInstalled(library); } catch (...) { busy = true; }
			Check(busy);
		}
		// Interrupted directory replacement restores the old accepted entry; abandoned staging is removed.
		const auto previousEntry = local.source.wstring() + L".previous";
		std::filesystem::rename(local.source, previousEntry);
		const auto abandoned = local.source.wstring() + L".staging";
		std::filesystem::create_directory(abandoned);
		Check(PresetPackages::EnumerateInstalled(library).front().digest == local.digest);
		Check(!std::filesystem::exists(previousEntry) && !std::filesystem::exists(abandoned));
		Check(PresetPackages::EnumerateInstalled(library).size() == 1);
		// A pending replacement backup must never resurrect a deliberately deleted entry.
		const auto deleteLibrary = directory / L"delete-library";
		const auto deleting = PresetPackages::Publish(snapshot, "local", {}, deleteLibrary);
		const std::filesystem::path deletePrevious = deleting.source.wstring() + L".previous";
		std::filesystem::copy(deleting.source, deletePrevious, std::filesystem::copy_options::recursive);
		{
			wil::unique_hfile held(CreateFileW((deletePrevious / L"manifest.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(held));
			bool blocked{};
			try { PresetPackages::RemoveLibraryEntry(deleting.libraryId, deleteLibrary); } catch (...) { blocked = true; }
			Check(blocked && std::filesystem::exists(deleting.source));
			Check(PresetPackages::LoadTrusted(deleting, deleteLibrary).digest == deleting.digest);
		}
		PresetPackages::RemoveLibraryEntry(deleting.libraryId, deleteLibrary);
		Check(!std::filesystem::exists(deletePrevious) && PresetPackages::EnumerateInstalled(deleteLibrary).empty());
		// Cleanup does not require the damaged current entry to validate first.
		const auto damaged = PresetPackages::Publish(snapshot, "local", {}, deleteLibrary);
		const std::filesystem::path damagedPrevious = damaged.source.wstring() + L".previous";
		std::filesystem::copy(damaged.source, damagedPrevious, std::filesystem::copy_options::recursive);
		std::filesystem::remove(damaged.source / L"manifest.json");
		PresetPackages::RemoveLibraryEntry(damaged.libraryId, deleteLibrary);
		Check(!std::filesystem::exists(damagedPrevious) && PresetPackages::EnumerateInstalled(deleteLibrary).empty());
		Check(PresetPackages::Publish(snapshot, "imported", {}, library).libraryId == local.libraryId);
		auto revisedRequest = assetRequest;
		revisedRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		revisedRequest.metadata.authorName = L"Updated rights holder";
		revisedRequest.licenseText = "Own replacement terms";
		revisedRequest.settings[Settings::Id::GlassOpacity] = 31u;
		const auto revised = PresetPackages::Publish(PresetPackages::CreateSnapshot(revisedRequest), "local", local.libraryId, library, local.digest);
		Check(revised.libraryId == local.libraryId && revised.source == local.source && revised.digest != local.digest);
		Check(PresetPackages::LoadDeployed(local.source).digest == revised.digest);
		bool stale{};
		try { (void)PresetPackages::Publish(snapshot, "local", local.libraryId, library, local.digest); } catch (...) { stale = true; }
		Check(stale && PresetPackages::LoadTrusted(revised, library).digest == revised.digest);
		PresetPackages::ExportArchive(directory / L"saved-entry.zip", PresetPackages::LoadTrusted(revised, library));
		Check(PresetPackages::LoadArchive(directory / L"saved-entry.zip").digest == revised.digest);
		const auto imported = PresetPackages::Publish(loaded, "imported", {}, library);
		auto importUpdate = request; importUpdate.metadata.uuid = PresetPackages::GeneratePackageUuid(); importUpdate.metadata.authorName = L"Editable name";
		PresetPackages::PreserveRevisionProvenance(importUpdate, &imported, &imported);
		const auto updatedImport = PresetPackages::Publish(PresetPackages::CreateSnapshot(importUpdate), "local", imported.libraryId, library, imported.digest);
		Check(updatedImport.libraryId == imported.libraryId && updatedImport.metadata.authorName == L"Editable name");
		Check(!updatedImport.attribution.empty() && updatedImport.licenseText.find(imported.licenseText) != std::string::npos);
		// Publication failure does not replace the accepted directory.
		{
			wil::unique_hfile held(CreateFileW((revised.source / L"manifest.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			auto candidate = revisedRequest; candidate.metadata.uuid = PresetPackages::GeneratePackageUuid();
			bool blocked{}; try { (void)PresetPackages::Publish(PresetPackages::CreateSnapshot(candidate), "local", revised.libraryId, library, revised.digest); } catch (...) { blocked = true; }
			Check(blocked && PresetPackages::LoadTrusted(revised, library).digest == revised.digest);
		}
		// Changed accepted content is not automatically trusted, even if still valid.
		const auto admission = ManagedFiles::Read(updatedImport.source / L".accepted.json");
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		const std::string changedAdmission = R"({"version":1,"digest":"changed"})";
		ManagedFiles::Write(updatedImport.source / L".accepted.json", { reinterpret_cast<const std::byte*>(changedAdmission.data()), changedAdmission.size() });
		bool mismatch{}; try { (void)PresetPackages::LoadTrusted(updatedImport, library); } catch (...) { mismatch = true; }
		Check(mismatch);
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		ManagedFiles::Write(updatedImport.source / L".accepted.json", admission);
		// Admission cannot be forged by changing package metadata or omitting the local record.
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		bool untrusted{}; try { (void)PresetPackages::LoadTrusted(updatedImport, library); } catch (...) { untrusted = true; }
		Check(untrusted);
		Check(std::ranges::any_of(PresetPackages::EnumerateInstalled(library), [](const auto& entry) { return !entry.trusted; }));
		// Broken entries remain independently diagnosable.
		const auto invalidId = PresetPackages::GeneratePackageUuid();
		std::filesystem::create_directory(library / L"Library" / invalidId);
		Check(std::ranges::any_of(PresetPackages::EnumerateInstalled(library), [](const auto& entry) { return !entry.loadError.empty(); }));
		PresetPackages::RemoveLibraryEntry(invalidId, library);

		ConfigurationResources resources;
		const auto resourceRoot = directory / L"Configuration";
		resources.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
		const auto reflection = resources.Path(Settings::Scope::Machine, Settings::Id::CustomThemeReflection);
		const auto userReflection = resources.Path(Settings::Scope::User, Settings::Id::CustomThemeReflection);
		Check(reflection != userReflection && resources.IsManaged(reflection));
		Check(!resources.NeedsImport(Settings::Scope::Machine, Settings::Id::CustomThemeReflection, reflection));
		Check(resources.NeedsImport(Settings::Scope::User, Settings::Id::CustomThemeReflection, reflection));
		Check(!resources.NeedsImport(Settings::Scope::Machine, Settings::Id::CustomThemeReflection, validPng));
		{
			auto writer = resources.AcquireWriter();
			ConfigurationResources anotherSession; anotherSession.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1002");
			bool busy{}; try { auto conflicting = anotherSession.AcquireWriter(); } catch (...) { busy = true; }
			Check(busy); writer.reset();
			Check(static_cast<bool>(anotherSession.AcquireWriter()));
		}
		const auto prepared = resources.Prepare(revised, Settings::Scope::Machine);
		resources.Begin(); resources.Reconcile();
		Check(!resources.HasAttemptChanges() && !resources.IsDirty());
		{
			ConfigurationResources emptyRecovery; emptyRecovery.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
			Check(emptyRecovery.HasRecovery());
			unsigned callbacks{};
			Check(emptyRecovery.Recover([&](auto, auto, const auto&) { ++callbacks; return false; },
				[&](const auto&) { ++callbacks; return false; }));
			Check(callbacks == 0 && !emptyRecovery.HasRecovery());
		}
		resources.CommitAttempt();
		resources.Begin(); resources.Install(*prepared); Check(resources.HasAttemptChanges()); resources.CommitAttempt();
		Check(resources.IsDirty() && ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(*prepared);
		Check(!resources.HasAttemptChanges() && resources.IsDirty());
		resources.CommitAttempt(); Check(!resources.HasRecovery());
		resources.Accept(); Check(!resources.IsDirty());
		resources.Begin(); resources.Install(*prepared); resources.CommitAttempt(); Check(!resources.IsDirty());
		// Independently linked resources survive deletion of their source preset.
		PresetPackages::RemoveLibraryEntry(revised.libraryId, library);
		Check(!std::filesystem::exists(revised.source) && std::filesystem::exists(reflection));
		PresetPackages::CreateRequest extracted = request;
		resources.PreserveSource(extracted, reflection);
		Check(extracted.licenseText.find(revised.licenseText) != std::string::npos);
		// The file transaction works on exact bytes, including same-path changes and absence.
		const auto alternate = directory / L"alternate-resource";
		ManagedFiles::Write(alternate, { reinterpret_cast<const std::byte*>("alternate"), 9 });
		ConfigurationResources::Preparation replacement;
		replacement.files[reflection] = alternate;
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt(); Check(resources.IsDirty());
		Check(ManagedFiles::Read(reflection) != snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(*prepared);
		Check(resources.HasAttemptChanges() && !resources.IsDirty()); // Same path returned to baseline still needs a refresh.
		resources.CommitAttempt();
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		Check(resources.Revert()); resources.Accept();
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(replacement);
		Check(resources.RevertAttemptFiles() && resources.RollbackAttempt(true) && !resources.IsDirty());
		// Restore failure retains the baseline and can be retried.
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		{
			wil::unique_hfile held(CreateFileW(reflection.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(held) && !resources.Revert() && resources.IsDirty());
		}
		Check(resources.Revert()); resources.Accept();
		// Interrupted attempt is recoverable by a new instance with callbacks, without real registry writes.
		ColorPreference::Snapshot recoveryColor{ DWORD{0}, DWORD{0x123456} };
		recoveryColor.accent = RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } };
		resources.Begin(recoveryColor);
		resources.TrackRegistry(Settings::Scope::Machine, Settings::Id::GlassOpacity, { true, REG_DWORD, { 42, 0, 0, 0 } });
		resources.Install(replacement);
		ConfigurationResources recovered; recovered.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
		Check(recovered.HasRecovery());
		ConfigurationResources foreign; foreign.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1002");
		Check(foreign.HasForeignRecovery() && !foreign.HasRecovery());
		unsigned registryCalls{}, colorCalls{};
		const auto recoveryPath = resourceRoot / L".operation-S-1-5-21-1-2-3-1001" / L"state.json";
		const auto recoveryBytes = ManagedFiles::Read(recoveryPath);
		auto invalidRecovery = nlohmann::json::parse(reinterpret_cast<const char*>(recoveryBytes.data()), reinterpret_cast<const char*>(recoveryBytes.data()) + recoveryBytes.size());
		invalidRecovery["registry"][0]["name"] = "AnotherSetting";
		std::filesystem::remove(recoveryPath);
		const auto invalidText = invalidRecovery.dump();
		ManagedFiles::Write(recoveryPath, { reinterpret_cast<const std::byte*>(invalidText.data()), invalidText.size() });
		bool rejectedRecovery{};
		try { (void)recovered.Recover([&](auto, auto, const auto&) { ++registryCalls; return true; }, [&](const auto&) { ++colorCalls; return true; }); } catch (...) { rejectedRecovery = true; }
		Check(rejectedRecovery && registryCalls == 0 && colorCalls == 0 && ManagedFiles::Read(reflection) == ManagedFiles::Read(alternate));
		std::filesystem::remove(recoveryPath); ManagedFiles::Write(recoveryPath, recoveryBytes);
		Check(recovered.Recover([&](auto scope, auto id, const auto& value) { ++registryCalls; return scope == Settings::Scope::Machine && id == Settings::Id::GlassOpacity && value.bytes[0] == 42; },
			[&](const auto& choice) { ++colorCalls; return choice == recoveryColor; }));
		Check(registryCalls == 1 && colorCalls == 1 && !recovered.HasRecovery());
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Accept();
		// Round-trip independent Explorer/DWM Accent identities, including absence and raw types.
		for (const bool dwm : { false, true })
		for (const auto& accent : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_DWORD, { 1, 2, 3, 255 } },
			RegistryConfig::RawValue{ true, REG_BINARY, { 4, 5, 6 } } })
		{
			ColorPreference::Snapshot accentOnly;
			accentOnly.applyChoice = false;
			(dwm ? accentOnly.dwmAccent : accentOnly.accent) = accent;
			resources.Begin(accentOnly);
			const auto bytes = ManagedFiles::Read(recoveryPath);
			const auto record = nlohmann::json::parse(reinterpret_cast<const char*>(bytes.data()), reinterpret_cast<const char*>(bytes.data()) + bytes.size());
			Check(!record["color"]["apply_choice"].get<bool>() && record["color"]["rgb"].is_null());
			const auto& saved = record["color"][dwm ? "dwm_accent" : "accent"];
			Check(saved["present"] == accent.present && saved["type"] == accent.type
				&& saved["bytes"].get<std::vector<BYTE>>() == accent.bytes);
			registryCalls = colorCalls = 0;
			Check(recovered.Recover([&](auto, auto, const auto&) { ++registryCalls; return false; },
				[&](const auto& choice) { ++colorCalls; return choice == accentOnly; }));
			Check(registryCalls == 0 && colorCalls == 1 && !recovered.HasRecovery());
			resources.Accept();
		}
		// A failed raw DWM restore retains the record for retry.
		resources.Begin(ColorPreference::Snapshot{ 0u, 0x112233u });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationAfterglow, { true, REG_DWORD, { 0x66, 0x55, 0x44, 0xAA } });
		std::vector<int> recoveryOrder;
		auto restoreColor = [&](const auto&) { recoveryOrder.push_back(1); return true; };
		bool allowRegistryRestore{};
		auto restoreRegistry = [&](auto, auto, const auto& value) { recoveryOrder.push_back(2); return allowRegistryRestore && value.bytes[0] == 0x66; };
		Check(!recovered.Recover(restoreRegistry, restoreColor));
		Check(recoveryOrder == std::vector<int>{ 1, 2 } && recovered.HasRecovery());
		recoveryOrder.clear();
		allowRegistryRestore = true;
		Check(recovered.Recover(restoreRegistry, restoreColor));
		Check(recoveryOrder == std::vector<int>{ 1, 2 } && !recovered.HasRecovery());
		resources.Accept();
		// Automatic recovery must not overwrite freshly recomputed RGB with its old derived values.
		resources.Begin();
		resources.TrackColor(ColorPreference::Snapshot{ 1u, std::nullopt });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationColor, { true, REG_DWORD, { 1, 2, 3, 4 } });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationAfterglow, { true, REG_DWORD, { 5, 6, 7, 8 } });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::GlassOpacity, { true, REG_DWORD, { 42, 0, 0, 0 } });
		registryCalls = colorCalls = 0;
		Check(recovered.Recover([&](auto, auto id, const auto&) { ++registryCalls; return id == Settings::Id::GlassOpacity; },
			[&](const auto& choice) { ++colorCalls; return choice.RestoresAutomatic(); }));
		Check(registryCalls == 1 && colorCalls == 1);
		resources.Accept();
		// Revert itself publishes a durable restore target before modifying files.
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		resources.PrepareRevert({ { Settings::Scope::User, Settings::Id::GlassOpacity, { false, 0, {} } } }, std::nullopt);
		Check(recovered.HasRecovery());
		Check(recovered.Recover([](auto scope, auto id, const auto& value) { return scope == Settings::Scope::User && id == Settings::Id::GlassOpacity && !value.present; }, [](const auto&) { return false; }));
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Accept();
		// Atlas and layout are one transaction; a layout-less replacement removes the old sidecar.
		const auto atlas = resources.Path(Settings::Scope::User, Settings::Id::CustomThemeAtlas);
		const auto preparedAtlas = resources.Prepare(loadedAtlas, Settings::Scope::User);
		resources.Begin(); resources.Install(*preparedAtlas); resources.CommitAttempt(); resources.Accept();
		atlasRequest.assetSources.erase("assets/theme-atlas.png.layout");
		atlasRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		const auto withoutLayout = resources.Prepare(PresetPackages::CreateSnapshot(atlasRequest), Settings::Scope::User);
		resources.Begin(); resources.Install(*withoutLayout); resources.CommitAttempt();
		Check(resources.IsDirty() && !std::filesystem::exists(atlas.wstring() + L".layout"));
		Check(resources.Revert()); resources.Accept();
		Check(std::filesystem::exists(atlas.wstring() + L".layout") && !std::filesystem::exists(resources.Path(Settings::Scope::Machine, Settings::Id::CustomThemeAtlas)));
		// File sharing and replacement never mutate the other hard-link directory entry.
		const auto link = resourceRoot / L"linked.png";
		Check(ManagedFiles::LinkOrCopy(reflection, link, resourceRoot));
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt(); resources.Accept();
		Check(ManagedFiles::Read(link) == snapshot.assets.at("assets/reflection.png"));
		const auto external = directory / L"external-copy";
		Check(!ManagedFiles::LinkOrCopy(validPng, external, resourceRoot));
		Check(!ManagedFiles::LinkOrCopy(reflection.wstring() + L".source.json", resourceRoot / L"notice-copy.json", resourceRoot)); // Notices never share a file identity.
		// A missing captured source cannot invalidate the already validated snapshot.
		auto detached = snapshot; detached.assetSources["assets/reflection.png"] = directory / L"missing-source.png";
		const auto detachedRoot = directory / L"detached-library";
		Check(PresetPackages::Publish(detached, "local", {}, detachedRoot).trusted);
		// Legacy library migration retains existing paths but never resurrects a deleted new entry.
		const auto oldRoot = directory / L"old-library";
		const auto oldDeployment = oldRoot / snapshot.metadata.uuid;
		ManagedFiles::Directory(oldDeployment);
		auto writeLegacyText = [&](const auto& name, const std::string& text)
		{
			ManagedFiles::Write(oldDeployment / name, { reinterpret_cast<const std::byte*>(text.data()), text.size() });
		};
		writeLegacyText(L"manifest.json", snapshot.manifestText);
		if (!snapshot.licenseText.empty()) writeLegacyText(L"LICENSE", snapshot.licenseText);
		for (const auto& [name, bytes] : snapshot.assets)
		{
			ManagedFiles::Directory((oldDeployment / name).parent_path());
			ManagedFiles::Write(oldDeployment / name, bytes);
		}
		// Older deployments used read-only file attributes as well as ACLs.
		for (const auto& item : std::filesystem::recursive_directory_iterator(oldDeployment))
			if (item.is_regular_file()) Check(SetFileAttributesW(item.path().c_str(), FILE_ATTRIBUTE_READONLY) != FALSE);
		const auto oldId = PresetPackages::GeneratePackageUuid();
		{ std::ofstream file(oldRoot / L"library.json"); file << nlohmann::json{ { "version", 1 }, { "entries", nlohmann::json::array({ { { "id", oldId }, { "revision", snapshot.metadata.uuid }, { "digest", snapshot.digest }, { "source_type", "imported" }, { "source_digest", snapshot.digest } } }) } }.dump(); }
		const auto migrated = PresetPackages::EnumerateInstalled(oldRoot);
		Check(migrated.size() == 1 && migrated.front().libraryId == oldId && migrated.front().trusted);
		Check((GetFileAttributesW((oldDeployment / L"assets/reflection.png").c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
		PresetPackages::RemoveLibraryEntry(oldId, oldRoot);
		Check(PresetPackages::EnumerateInstalled(oldRoot).empty() && std::filesystem::exists(oldDeployment));

		auto writeRawZip = [](const std::filesystem::path& path, const std::vector<std::pair<wxString, std::string>>& entries)
		{
			wxFFileOutputStream output(path.wstring());
			wxZipOutputStream zip(output, 9);
			for (const auto& [name, content] : entries)
			{
				zip.PutNextEntry(name);
				zip.Write(content.data(), content.size());
			}
			zip.Close();
			output.Close();
		};
		auto rejectedArchive = [](const std::filesystem::path& path)
		{
			try { static_cast<void>(PresetPackages::LoadArchive(path)); }
			catch (...) { return true; }
			return false;
		};

		// Unpublished development formats are not compatibility contracts.
		{
			auto unsupported = nlohmann::ordered_json::parse(loaded.manifestText);
			unsupported["schema_version"] = 4;
			const auto path = directory / L"unsupported-schema.zip";
			writeRawZip(path, { { L"manifest.json", unsupported.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
			unsupported["schema_version"] = 3;
			unsupported.erase("rights");
			writeRawZip(path, { { L"manifest.json", unsupported.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
		}

		{
			auto modern = nlohmann::ordered_json::parse(loaded.manifestText);
			Check(!modern["settings"].contains("ColorizationColorOverride") && !modern["settings"].contains("ColorizationAfterglowOverride"));
			modern["settings"]["ColorizationColorOverride"] = 0xAA112233u;
			modern["settings"]["ColorizationAfterglowOverride"] = 0xBB445566u;
			const auto path = directory / L"ignored-color-overrides.zip";
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			const auto ignored = PresetPackages::LoadArchive(path);
			Check(ignored.ignoredSettingCount == 2 && !ignored.settings.contains(Settings::Id::ColorizationColorOverride)
				&& !ignored.settings.contains(Settings::Id::ColorizationAfterglowOverride));
			modern["settings"]["ColorizationColorOverride"] = 0xAA778899u;
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			Check(PresetPackages::LoadArchive(path).digest != ignored.digest); // Ignored input remains authenticated.
		}
		for (const unsigned catalog : { 1u, Settings::CatalogVersion, Settings::CatalogVersion + 1 })
		{
			auto modern = nlohmann::ordered_json::parse(loaded.manifestText);
			modern["catalog_version"] = catalog;
			for (const auto id : { Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationBlurBalance })
			{
				const auto name = wxString(Settings::Get(id).name.data()).ToStdString(wxConvUTF8);
				Check(!modern["settings"].contains(name));
				modern["settings"][name] = 123u;
				modern["settings"][name + "Override"] = { { "state", "default" } };
			}
			modern["settings"]["ColorizationBlurBalanceOverride"] = 77u;
			const auto path = directory / (L"ignored-base-balances-" + std::to_wstring(catalog) + L".zip");
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			if (catalog < 2) { Check(rejectedArchive(path)); continue; } // Invalid schema/catalog pairing stays rejected.
			const auto ignored = PresetPackages::LoadArchive(path);
			Check(ignored.ignoredSettingCount == 3 && ignored.ignoredSettingNames.size() == 3);
			for (const auto id : { Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationBlurBalance })
				Check(!ignored.settings.contains(id));
			Check(std::holds_alternative<std::monostate>(ignored.settings.at(Settings::Id::ColorizationColorBalanceOverride)));
			Check(std::holds_alternative<std::monostate>(ignored.settings.at(Settings::Id::ColorizationAfterglowBalanceOverride)));
			Check(std::get<DWORD>(ignored.settings.at(Settings::Id::ColorizationBlurBalanceOverride)) == 77);
			modern["settings"]["ColorizationColorBalance"] = 456u;
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			Check(PresetPackages::LoadArchive(path).digest != ignored.digest);
		}
		for (const unsigned schema : { 1u, 2u })
		{
			auto legacy = nlohmann::ordered_json::parse(loaded.manifestText);
			legacy["schema_version"] = schema;
			legacy.erase("rights");
			for (auto& value : legacy["settings"]) if (value.is_object() && value.value("state", std::string{}) == "default") value = nullptr;
			legacy["catalog_version"] = 1;
			legacy.erase("accent_color");
			if (schema == 1) legacy["license"] = { { "name", "MIT" }, { "file", "LICENSE" } };
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			legacy["settings"]["ColorizationColor"] = 0x8074B8FCu;
			legacy["settings"]["ColorizationAfterglow"] = 0x80553311u;
			legacy["settings"]["ColorizationColorBalance"] = 42u;
			legacy["settings"]["ColorizationBlurBalance"] = 60u;
			legacy["settings"]["ColorizationBlurBalanceOverride"] = 77u;
			legacy["settings"]["GlassType"] = 1u;
			const auto path = directory / (L"legacy-" + std::to_wstring(schema) + L".zip");
			const auto text = legacy.dump();
			writeRawZip(path, { { L"manifest.json", text }, { L"LICENSE", request.licenseText } });
			const auto converted = PresetPackages::LoadArchive(path);
			Check(converted.manifestText == text);
			Check(converted.legacyLicense);
			Check(converted.accentColor == 0x74B8FCu);
			Check(!converted.settings.contains(Settings::Id::ColorizationColor));
			Check(!converted.settings.contains(Settings::Id::ColorizationAfterglowOverride) && converted.ignoredSettingCount > 0);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::ColorizationColorBalanceOverride)) == 42);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::ColorizationBlurBalanceOverride)) == 77);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::GlassOpacity)) == ColorizationPresets::CalculateVistaOpacity(0x8074B8FC));
			Check(!converted.conversions.empty());
			legacy["settings"]["GlassOpacity"] = 37u;
			legacy["settings"]["ColorizationColorOverride"] = 0xAA112233u;
			legacy["settings"]["ColorizationAfterglowOverride"] = 0xBB445566u;
			const auto explicitPath = directory / (L"legacy-explicit-" + std::to_wstring(schema) + L".zip");
			writeRawZip(explicitPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto explicitColors = PresetPackages::LoadArchive(explicitPath);
			Check(std::get<DWORD>(explicitColors.settings.at(Settings::Id::GlassOpacity)) == 37);
			Check(!explicitColors.settings.contains(Settings::Id::ColorizationColorOverride));
			Check(!explicitColors.settings.contains(Settings::Id::ColorizationAfterglowOverride));
			Check(explicitColors.accentColor == 0x74B8FCu && explicitColors.ignoredSettingCount >= 3);
			legacy["settings"]["GlassOpacity"] = nullptr;
			writeRawZip(explicitPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			Check(std::get<DWORD>(PresetPackages::LoadArchive(explicitPath).settings.at(Settings::Id::GlassOpacity)) == ColorizationPresets::CalculateVistaOpacity(0x8074B8FC));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			const auto emptyPath = directory / (L"legacy-no-base-" + std::to_wstring(schema) + L".zip");
			writeRawZip(emptyPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto empty = PresetPackages::LoadArchive(emptyPath);
			Check(!empty.accentColor && std::holds_alternative<std::monostate>(empty.settings.at(Settings::Id::GlassOpacity)));
		}
		{
			auto legacy = nlohmann::ordered_json::parse(loaded.manifestText);
			legacy["schema_version"] = 2; legacy.erase("rights");
			legacy.erase("accent_color");
			legacy["catalog_version"] = 1;
			for (auto& value : legacy["settings"]) if (value.is_object() && value.value("state", std::string{}) == "default") value = nullptr;
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			const auto path = directory / L"legacy-rights.zip";
			writeRawZip(path, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto old = PresetPackages::LoadArchive(path);
			Check(old.legacyLicense && !old.conversions.empty());
			auto derived = request; derived.legacyLicense = true;
			derived.attribution = { L"Original author: https://example.com/author" };
			const auto saved = PresetPackages::CreateSnapshot(derived);
			Check(saved.legacyLicense && saved.licenseText == old.licenseText && saved.attribution == derived.attribution);
			const auto legacyLibrary = directory / L"legacy-library";
			const auto entry = PresetPackages::Publish(saved, "local-copy", {}, legacyLibrary);
			derived.metadata.uuid = PresetPackages::GeneratePackageUuid(); derived.legacyLicense = false;
			bool refusedRelicense{};
			try { (void)PresetPackages::Publish(PresetPackages::CreateSnapshot(derived), "local-copy", entry.libraryId, legacyLibrary, entry.digest); }
			catch (...) { refusedRelicense = true; }
			Check(refusedRelicense && PresetPackages::EnumerateInstalled(legacyLibrary).front().legacyLicense);

		}


		auto noColor = request;
		noColor.accentColor.reset();
		createArchive(directory / L"no-color.zip", noColor);
		Check(!PresetPackages::LoadArchive(directory / L"no-color.zip").accentColor);
		auto missingColor = nlohmann::ordered_json::parse(loaded.manifestText);
		missingColor.erase("accent_color");
		writeRawZip(directory / L"automatic-color.zip", { { L"manifest.json", missingColor.dump() }, { L"LICENSE", request.licenseText } });
		Check(!PresetPackages::LoadArchive(directory / L"automatic-color.zip").accentColor);
		auto blackColor = request;
		blackColor.accentColor = 0; // Black is manual RGB, not the automatic sentinel.
		createArchive(directory / L"black-color.zip", blackColor);
		Check(PresetPackages::LoadArchive(directory / L"black-color.zip").accentColor == std::optional<DWORD>{ 0 });
		for (const auto rgb : { "#GG1122", "#12345", "112233", "#11223344" })
		{
			auto malformed = nlohmann::ordered_json::parse(loaded.manifestText);
			malformed["accent_color"] = { { "rgb", rgb } };
			const auto path = directory / (L"invalid-rgb-" + std::to_wstring(std::hash<std::string_view>{}(rgb)) + L".zip");
			writeRawZip(path, { { L"manifest.json", malformed.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
		}
		auto extra = nlohmann::ordered_json::parse(loaded.manifestText);
		extra["future_metadata"] = "retained in digest";
		const auto extraPath = directory / L"future-metadata.zip";
		writeRawZip(extraPath, { { L"manifest.json", extra.dump() }, { L"LICENSE", request.licenseText } });
		const auto extraLoaded = PresetPackages::LoadArchive(extraPath);
		Check(extraLoaded.digest != loaded.digest && extraLoaded.settings == loaded.settings);

		auto earlierManifest = nlohmann::ordered_json::parse(loaded.manifestText);
		earlierManifest["catalog_version"] = Settings::CatalogVersion + 1;
		earlierManifest["settings"]["MINMAXBUTTONGLOWid"] = 93;
		earlierManifest["settings"]["CLOSEBUTTONGLOWid"] = 92;
		earlierManifest["settings"]["TOOLCLOSEBUTTONGLOWid"] = 94;
		earlierManifest["settings"]["FutureRenderingMode"] = { { "mode", "future" } };
		const auto earlierArchive = directory / L"earlier-preset-catalog.zip";
		writeRawZip(earlierArchive, {
			{ L"manifest.json", earlierManifest.dump(2) + "\n" },
			{ L"LICENSE", request.licenseText }
		});
		const auto earlierLoaded = PresetPackages::LoadArchive(earlierArchive);
		Check(earlierLoaded.settings.size() == Settings::PresetPackSettingCount());
		Check(!earlierLoaded.settings.contains(Settings::Id::MinMaxButtonGlowId));
		Check(!earlierLoaded.settings.contains(Settings::Id::CloseButtonGlowId));
		Check(!earlierLoaded.settings.contains(Settings::Id::ToolCloseButtonGlowId));
		Check(earlierLoaded.catalogVersion == Settings::CatalogVersion + 1);
		Check(earlierLoaded.ignoredSettingCount == 4);
		Check(earlierLoaded.ignoredSettingNames.size() == 4);

		const auto traversal = directory / L"traversal.zip";
		writeRawZip(traversal, { { L"../manifest.json", "{}" }, { L"LICENSE", "license" } });
		Check(rejectedArchive(traversal));

		const auto duplicate = directory / L"duplicate.zip";
		writeRawZip(duplicate, { { L"manifest.json", "{}" }, { L"LICENSE", "license" }, { L"license", "license" } });
		Check(rejectedArchive(duplicate));

		const auto excessiveRatio = directory / L"excessive-ratio.zip";
		writeRawZip(excessiveRatio, { { L"manifest.json", std::string(1024 * 1024, '0') }, { L"LICENSE", "license" } });
		Check(rejectedArchive(excessiveRatio));

		const auto truncated = directory / L"truncated.zip";
		auto truncatedBytes = readFile(first);
		truncatedBytes.resize(truncatedBytes.size() - std::min<std::size_t>(32, truncatedBytes.size()));
		{
			std::ofstream output(truncated, std::ios::binary);
			output.write(truncatedBytes.data(), truncatedBytes.size());
		}
		Check(rejectedArchive(truncated));

		invalid = request;
		invalid.licenseText = std::string("invalid-") + static_cast<char>(0xff);
		bool rejectedLicense{};
		try { createArchive(directory / L"bad-license.zip", invalid); }
		catch (...) { rejectedLicense = true; }
		Check(rejectedLicense);

		const auto fakePng = directory / L"fake.png";
		{
			std::ofstream output(fakePng, std::ios::binary);
			output << "not a PNG";
		}
		invalid = request;
		invalid.settings[Settings::Id::CustomThemeReflection] = PresetPackages::AssetReference{ "assets/reflection.png" };
		invalid.assetSources.emplace("assets/reflection.png", fakePng);
		bool rejectedImage{};
		try { createArchive(directory / L"bad-image.zip", invalid); }
		catch (...) { rejectedImage = true; }
		Check(rejectedImage);
	}
}

int OpenGlassTests::Target(int value)
{
	return value + 10;
}

int OpenGlassTests::Replacement(int value)
{
	return value + 20;
}

int OpenGlassTests::Replacement2(int value)
{
	return value + 30;
}

int main()
{
	using OpenGlass::Settings::ParseEditorScope;
	using OpenGlass::Settings::Scope;
	Check(ParseEditorScope({}) == Scope::Machine);
	const std::wstring_view userArgs[]{ L"--scope=HKCU" };
	const std::wstring_view conflictArgs[]{ L"--scope=hklm", L"--scope=hkcu" };
	const std::wstring_view badArgs[]{ L"--scope=invalid" };
	const std::wstring_view missingArgs[]{ L"--scope" };
	const std::wstring_view repeatArgs[]{ L"--scope", L"HKLM", L"--scope=hklm" };
	const std::wstring_view emptyArgs[]{ L"--scope=" };
	const std::wstring_view splitEmptyArgs[]{ L"--scope", L"" };
	const std::wstring_view mixedArgs[]{ L"--scope", L"hKcU", L"--scope=HkCu" };
	const std::wstring_view flagAsValueArgs[]{ L"--scope", L"--scope=hkcu" };
	Check(ParseEditorScope(userArgs) == Scope::User);
	Check(!ParseEditorScope(conflictArgs));
	Check(!ParseEditorScope(badArgs));
	Check(!ParseEditorScope(missingArgs));
	Check(ParseEditorScope(repeatArgs) == Scope::Machine);
	Check(!ParseEditorScope(emptyArgs) && !ParseEditorScope(splitEmptyArgs));
	Check(ParseEditorScope(mixedArgs) == Scope::User && !ParseEditorScope(flagAsValueArgs));
	TestPixelAlign();
	TestTransform2DBounds();
	TestHookRundown();
	TestImportHooks();
	TestVersionsAndFields();
	TestCompleteNameResolutionAndFallback();
	TestSymbolCatalogCollection();
	TestAtomicCommitAndDetourStorage();
	TestDisjointProjectedBindings();
	TestLogicalSymbolBindings();
	TestInvalidMetadata();
	TestPngAssetValidation();
	TestThemeAtlasLayoutParser();
	TestDwmSettings();
	TestOverridableRegistryValueResolution();
	TestColorizationPresets();
	TestBlurSettings();
	TestSettingsCatalog();
	TestColorPolicy();
	TestConfigurationMergePolicy();
	TestPreviewJournal();
	TestNetPreviewJournal();
	TestShellColorRefresh();
	TestAutomaticColorPreview();
	TestEffectiveConfiguration();
	TestConfigurationStrings();
	TestConfigurationReset();
	g_failures += TestWrappingTextLayout();
	TestPresetProvenance();
	try { TestPresetPackageRoundTrip(); }
	catch (const std::exception& error) { fprintf(stderr, "Preset test exception: %s\n", error.what()); ++g_failures; }
	catch (...) { fprintf(stderr, "Preset test failure: 0x%08lX\n", wil::ResultFromCaughtException()); ++g_failures; }
	return g_failures;
}
