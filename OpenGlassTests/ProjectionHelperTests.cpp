#include "pch.h"
#include "TestHarness.hpp"
#include "ProjectionFixture.hpp"
#include "Util.hpp"

using namespace OpenGlass;

namespace
{
	using namespace OpenGlassTests;
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

	extern "C" __declspec(noinline) int ProjectionFieldReadHotPath(const std::byte* base) noexcept
	{
		constexpr Projection::FieldHandle<FixtureModuleTag, 0, int> field{};
		return field.read(base);
	}
}

namespace OpenGlassTests
{
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

	void TestAtomicCommitAndDetourStorage()
	{
		RegistryStorage<1, 1, 0, 0> first;
		RegistryStorage<1, 1, 0, 0> second;
		PVOID firstPublished{};
		PVOID secondPublished{};
		first.bindings[0] = {std::span{first.bindingSymbols}, &firstPublished, Util::force_cast_from(&Replacement)};
		second.bindings[0] = {std::span{second.bindingSymbols}, &secondPublished, Util::force_cast_from(&Replacement2)};
		size_t cursor{1};
		const auto firstId = AddString(first, cursor, "Target.Id");
		first.symbolNameOffsets[0] = AddString(first, cursor, "public: int __cdecl Target(int)");
		cursor = 1;
		const auto secondId = AddString(second, cursor, "Target.Id");
		second.symbolNameOffsets[0] = AddString(second, cursor, "public: int __cdecl Target(int)");
		first.symbols[0] = {0, firstId, 0, 1, 0, 0, Projection::Requirement::Required, false};
		second.symbols[0] = {0, secondId, 0, 1, 0, 0, Projection::Requirement::Required, false};
		Check(first.registry.Freeze({150, 0}));
		Check(second.registry.Freeze({150, 0}));
		first.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(!Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == nullptr);
		Check(second.registry.SymbolAddress(0, false) == nullptr);
		Check(firstPublished == Util::force_cast_from(&Replacement));
		Check(secondPublished == Util::force_cast_from(&Replacement2));

		second.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == Util::force_cast_from(&Target));
		Check(firstPublished == Util::force_cast_from(&Target));
		Check(secondPublished == Util::force_cast_from(&Target));
		second.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Replacement));
		Check(!Projection::CommitModules(first.registry, second.registry));
		Check(firstPublished == Util::force_cast_from(&Target));
		Check(secondPublished == Util::force_cast_from(&Target));

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
}
