#include "pch.h"
#include "ProjectionFixture.hpp"
#include "RegistryValueResolver.hpp"
#include "BlurSettings.hpp"
#include "PngAssetValidation.hpp"
#include "ThemeAtlasLayout.hpp"
#include "../OpenGlassGUI/ColorizationPresets.hpp"
#include "../Common/SettingsCatalog.hpp"
#include "../Common/ConfigurationMigrationPolicy.hpp"
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

	void Check(bool condition)
	{
		if (!condition)
		{
			g_failures++;
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
				Check(resolved.value == 11);
				Check(resolved.source == RegistryValueSource::UserOverride);
			}
			else if (userBase)
			{
				Check(resolved.value == 22);
				Check(resolved.source == RegistryValueSource::UserBase);
			}
			else if (machineOverride)
			{
				Check(resolved.value == 33);
				Check(resolved.source == RegistryValueSource::MachineOverride);
			}
			else if (machineBase)
			{
				Check(resolved.value == 44);
				Check(resolved.source == RegistryValueSource::MachineBase);
			}
			else
			{
				Check(resolved.value == defaultValue);
				Check(resolved.source == RegistryValueSource::Default);
			}
			Check(resolved.IsOverride() == (
				resolved.source == RegistryValueSource::UserOverride
				|| resolved.source == RegistryValueSource::MachineOverride
			));
		}

		const std::map<std::wstring_view, DWORD> userValues
		{
			{ L"Base", 22 },
			{ L"Override", 11 }
		};
		const std::map<std::wstring_view, DWORD> machineValues
		{
			{ L"Base", 44 },
			{ L"Override", 33 }
		};
		auto reader = [](const auto& values)
		{
			return [&values](std::wstring_view name) -> std::optional<DWORD>
			{
				const auto it = values.find(name);
				return it == values.end() ? std::nullopt : std::optional<DWORD>{ it->second };
			};
		};
		const auto fromReaders = ResolveOverridableRegistryValueFromReaders(
			std::wstring_view{ L"Base" },
			std::wstring_view{ L"Override" },
			defaultValue,
			reader(userValues),
			reader(machineValues)
		);
		Check(fromReaders.value == 11);
		Check(fromReaders.source == RegistryValueSource::UserOverride);
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

		std::vector<std::wstring_view> ids;
		for (const auto family : { Family::Vista, Family::Windows7 })
		{
			for (const auto& preset : Get(family))
			{
				Check(preset.family == family);
				Check(std::find(ids.begin(), ids.end(), preset.id) == ids.end());
				ids.push_back(preset.id);
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

		const auto vistaApplication = BuildApplication(Vista.front(), false);
		Check(vistaApplication.color == Vista.front().argb);
		Check(vistaApplication.vistaOpacity == 27u);
		Check(!vistaApplication.windows7);

		const auto windows7Application = BuildApplication(Windows7.front(), false);
		Check(windows7Application.color == Windows7.front().argb);
		Check(!windows7Application.vistaOpacity);
		Check(windows7Application.windows7 == sky);

		const auto customApplication = BuildApplication(0x804080C0, Family::Windows7, false);
		Check(customApplication.color == 0x804080C0);
		Check(!customApplication.vistaOpacity);
		Check(customApplication.windows7 == CalculateWindows7Parameters(0x804080C0, false));
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

	void TestSettingsCatalog()
	{
		std::vector<std::wstring_view> names;
		std::size_t userSettings{};
		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			Check(static_cast<std::size_t>(spec.id) == index);
			Check(!spec.name.empty());
			Check(spec.introducedIn > 0 && spec.introducedIn <= Settings::CatalogVersion);
			Check(std::find(names.begin(), names.end(), spec.name) == names.end());
			names.push_back(spec.name);
			Check(Settings::Find(spec.name) == &spec);
			if (spec.scope == Settings::Scope::User)
			{
				++userSettings;
				Check(spec.type == Settings::ValueType::Dword);
				Check(spec.assetRole == Settings::AssetRole::None);
				Check(spec.name == L"ColorizationColor"
					|| spec.name == L"ColorizationColorOverride"
					|| spec.name == L"ColorizationAfterglow"
					|| spec.name == L"ColorizationAfterglowOverride"
					|| spec.name == L"ColorizationColorBalance"
					|| spec.name == L"ColorizationColorBalanceOverride"
					|| spec.name == L"ColorizationAfterglowBalance"
					|| spec.name == L"ColorizationAfterglowBalanceOverride"
					|| spec.name == L"ColorizationBlurBalance"
					|| spec.name == L"ColorizationBlurBalanceOverride");
			}
			if (spec.type == Settings::ValueType::String)
			{
				Check(spec.scope == Settings::Scope::Machine);
				Check(spec.assetRole != Settings::AssetRole::None);
				Check(spec.sensitive);
			}
		}
		Check(userSettings == 10);
		Check(!Settings::Get(Settings::Id::DisableGlassOnBattery).sensitive);
		Check(Settings::Get(Settings::Id::GlassOverrideAccent).impact == Settings::UpdateImpact::Colorization);
		Check(Settings::Get(Settings::Id::GlassSafetyZoneMode).impact == Settings::UpdateImpact::Colorization);
		Check(Settings::Get(Settings::Id::GlassSafetyZoneMode).sensitive);
		Check(Settings::Get(Settings::Id::UseDirect3DRendering).impact == Settings::UpdateImpact::Colorization);
		Check(!Settings::Get(Settings::Id::UseDirect3DRendering).sensitive);
		Check(!Settings::Get(Settings::Id::MinMaxButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::CloseButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::ToolCloseButtonGlowId).includeInPresetPacks);
		Check(Settings::PresetPackSettingCount() + 3 == Settings::Catalog.size());
		Check(Settings::Find(L"NotAnOpenGlassSetting") == nullptr);
	}

	void TestConfigurationMigrationPolicy()
	{
		using Raw = std::variant<std::monostate, DWORD, std::wstring>;
		using ConfigurationMigrationPolicy::Canonicalize;
		using ConfigurationMigrationPolicy::HiveValues;
		const auto missing = [](const Raw& value) { return std::holds_alternative<std::monostate>(value); };

		auto result = Canonicalize(Settings::Scope::User, HiveValues<Raw>{ std::monostate{}, DWORD{ 10 } });
		Check(std::get<DWORD>(result.user) == 10 && missing(result.machine));
		result = Canonicalize(Settings::Scope::User, HiveValues<Raw>{ DWORD{ 20 }, DWORD{ 10 } });
		Check(std::get<DWORD>(result.user) == 20 && missing(result.machine));
		result = Canonicalize(Settings::Scope::Machine, HiveValues<Raw>{ DWORD{ 20 }, DWORD{ 10 } });
		Check(missing(result.user) && std::get<DWORD>(result.machine) == 20);
		result = Canonicalize(Settings::Scope::Machine, HiveValues<Raw>{ std::monostate{}, DWORD{ 10 } });
		Check(missing(result.user) && std::get<DWORD>(result.machine) == 10);
		result = Canonicalize(Settings::Scope::Machine, HiveValues<Raw>{ std::wstring(L"user"), std::wstring(L"machine") });
		Check(missing(result.user) && std::get<std::wstring>(result.machine) == L"user");
	}

	void TestPresetPackageRoundTrip()
	{
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
			for (const auto& path : std::filesystem::directory_iterator(directory)) SetFileAttributesW(path.path().c_str(), FILE_ATTRIBUTE_NORMAL);
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
		request.licenseText = "MIT License\n\nPermission is granted for this test package.\n";
		for (const auto& spec : Settings::Catalog)
		{
			if (Settings::IsPresetPackSetting(spec)) request.settings.emplace(spec.id, std::monostate{});
		}

		const auto first = directory / L"first.zip";
		const auto second = directory / L"second.zip";
		PresetPackages::CreateArchive(first, request);
		PresetPackages::CreateArchive(second, request);
		auto readFile = [](const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::vector<char>((std::istreambuf_iterator<char>(stream)), {});
		};
		Check(readFile(first) == readFile(second));
		Check((GetFileAttributesW(first.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);

		const auto loaded = PresetPackages::LoadArchive(first);
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
		PresetPackages::CreateArchive(unlicensedArchive, unlicensedRequest);
		const auto unlicensed = PresetPackages::LoadArchive(unlicensedArchive);
		Check(unlicensed.licenseText.empty());
		Check(unlicensed.metadata.licenseName.empty());

		auto undescribedRequest = request;
		undescribedRequest.metadata.uuid = "20112233-4455-6677-8899-aabbccddeeff";
		undescribedRequest.metadata.description.clear();
		const auto undescribedArchive = directory / L"undescribed.zip";
		PresetPackages::CreateArchive(undescribedArchive, undescribedRequest);
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
		PresetPackages::CreateArchive(assetArchive, assetRequest);
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
		PresetPackages::CreateArchive(atlasArchive, atlasRequest);
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
		try { PresetPackages::CreateArchive(directory / L"bad-layout.zip", atlasRequest); }
		catch (...) { rejectedLayout = true; }
		Check(rejectedLayout);

		bool rejectedOverwrite{};
		try { PresetPackages::CreateArchive(first, request); }
		catch (...) { rejectedOverwrite = true; }
		Check(rejectedOverwrite);

		auto invalid = request;
		invalid.metadata.authorHomepage = L"file:///not-allowed";
		bool rejectedUrl{};
		try { PresetPackages::CreateArchive(directory / L"bad-url.zip", invalid); }
		catch (...) { rejectedUrl = true; }
		Check(rejectedUrl);

		invalid = request;
		invalid.settings.erase(Settings::Id::GlassType);
		bool rejectedIncompleteCatalog{};
		try { PresetPackages::CreateArchive(directory / L"bad-catalog.zip", invalid); }
		catch (...) { rejectedIncompleteCatalog = true; }
		Check(rejectedIncompleteCatalog);

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
		try { PresetPackages::CreateArchive(directory / L"bad-license.zip", invalid); }
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
		try { PresetPackages::CreateArchive(directory / L"bad-image.zip", invalid); }
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
	TestPixelAlign();
	TestTransform2DBounds();
	TestHookRundown();
	TestVersionsAndFields();
	TestCompleteNameResolutionAndFallback();
	TestSymbolCatalogCollection();
	TestAtomicCommitAndDetourStorage();
	TestDisjointProjectedBindings();
	TestLogicalSymbolBindings();
	TestInvalidMetadata();
	TestPngAssetValidation();
	TestThemeAtlasLayoutParser();
	TestOverridableRegistryValueResolution();
	TestColorizationPresets();
	TestBlurSettings();
	TestSettingsCatalog();
	TestConfigurationMigrationPolicy();
	TestPresetPackageRoundTrip();
	return g_failures;
}
