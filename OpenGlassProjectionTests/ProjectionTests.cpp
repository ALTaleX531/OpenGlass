#include "pch.h"
#include "ProjectionFixture.hpp"
#include "RegistryValueResolver.hpp"

using namespace OpenGlass;
using namespace OpenGlassProjectionTests;

LONG OpenGlassProjectionTests::g_layoutOffsets[8]{};
bool OpenGlassProjectionTests::g_layoutSupported[8]{};

extern "C" __declspec(noinline) int ProjectionFieldReadHotPath(const std::byte* base) noexcept
{
	constexpr Projection::FieldHandle<FixtureModuleTag, 0, int> field{};
	return field.read(base);
}

namespace
{
	int g_failures{};

	void Check(bool condition)
	{
		if (!condition)
		{
			g_failures++;
		}
	}

	template <size_t SymbolCount, size_t BindingCount, size_t LayoutCount, size_t CaseCount>
	struct RegistryStorage
	{
		std::array<char, 512> strings{};
		std::array<size_t, 8> symbolNameOffsets{};
		std::array<Projection::Version, 4> versions{};
		std::array<Projection::SymbolSpec, SymbolCount> symbols{};
		std::array<PVOID, SymbolCount> candidates{};
		std::array<PVOID, SymbolCount> resolved{};
		std::array<Projection::ResolutionState, SymbolCount> resolutionStates{};
		std::array<Projection::BindingSpec, BindingCount> bindings{};
		std::array<Projection::LayoutSpec, LayoutCount> layouts{};
		std::array<Projection::LayoutCase, CaseCount> cases{};
		Projection::ModuleRegistry registry;

		RegistryStorage()
			: registry{
				"test", strings.data(), std::span{symbolNameOffsets}, std::span{versions}, std::span{symbols},
				std::span{candidates}, std::span{resolved}, std::span{resolutionStates}, std::span{bindings},
				std::span{layouts}, std::span{cases}, std::span{g_layoutOffsets}.first(LayoutCount),
				std::span{g_layoutSupported}.first(LayoutCount)
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
		storage.symbols = {{{requiredId, 0, 2, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{optionalId, 2, 1, 0, 0, Projection::Requirement::Optional, Projection::SymbolFlags::None}}};
		PVOID published{};
		storage.bindings[0] = {0, &published, Util::force_cast_from(&Replacement)};
		g_activeRegistry = &storage.registry;

		Check(storage.registry.Freeze({150, 0}));
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

		RegistryStorage<2, 0, 0, 0> overloads;
		cursor = 1;
		const auto firstId = AddString(overloads, cursor, "Overload.Int");
		overloads.symbolNameOffsets[0] = AddString(overloads, cursor, "public: int __cdecl Overload(int)");
		const auto secondId = AddString(overloads, cursor, "Overload.Double");
		overloads.symbolNameOffsets[1] = AddString(overloads, cursor, "public: int __cdecl Overload(double)");
		overloads.symbols = {{{firstId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{secondId, 1, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
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
		vtables.symbols = {{{derivedId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{siblingId, 1, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
		Check(vtables.registry.Freeze({150, 0}));
		vtables.registry.Collect("const Derived::`vftable'{for `BaseA'}", Util::force_cast_from(&Target));
		vtables.registry.Collect("const Derived::`vftable'{for `BaseB'}", Util::force_cast_from(&Replacement));
		Check(vtables.registry.ValidateSymbols());
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
		first.symbols[0] = {firstId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		second.symbols[0] = {secondId, 0, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		Check(first.registry.Freeze({150, 0}));
		Check(second.registry.Freeze({150, 0}));
		first.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(!Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == nullptr);

		second.registry.Collect("public: int __cdecl Target(int)", Util::force_cast_from(&Target));
		Check(Projection::CommitModules(first.registry, second.registry));
		Check(first.registry.SymbolAddress(0, false) == Util::force_cast_from(&Target));

		g_activeRegistry = &first.registry;
		Projection::Detour<g_symbol, TestFunction> detour;
		*detour.detour_storage() = &Replacement;
		Check(detour.original() == &Replacement);
		Check(g_symbol.get() == &Target);
		Check(g_symbol(1) == 11);
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
		storage.symbols = {{{oldId, 0, 1, 0, 1, Projection::Requirement::Required, Projection::SymbolFlags::None},
			{newId, 1, 1, 1, 0, Projection::Requirement::Required, Projection::SymbolFlags::None}}};
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

	void TestInvalidMetadata()
	{
		RegistryStorage<1, 0, 1, 0> storage;
		storage.symbols[0] = {0, 8, 1, 0, 0, Projection::Requirement::Required, Projection::SymbolFlags::None};
		storage.layouts[0] = {0, 1};
		Check(!storage.registry.Freeze({150, 0}));
		Check(storage.registry.descriptor_error());
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
	}
}

int OpenGlassProjectionTests::Target(int value)
{
	return value + 10;
}

int OpenGlassProjectionTests::Replacement(int value)
{
	return value + 20;
}

int main()
{
	TestVersionsAndFields();
	TestCompleteNameResolutionAndFallback();
	TestAtomicCommitAndDetourStorage();
	TestDisjointProjectedBindings();
	TestInvalidMetadata();
	TestOverridableRegistryValueResolution();
	return g_failures;
}
