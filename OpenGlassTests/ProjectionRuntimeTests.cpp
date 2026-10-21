#include "pch.h"
#include "TestHarness.hpp"
#include "ProjectionFixture.hpp"
#include "Util.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestCompleteNameResolutionAndFallback()
	{
		RegistryStorage<2, 1, 0, 0> storage;
		size_t cursor{1};
		const auto requiredId = AddString(storage, cursor, "Required.Id");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "public: int __cdecl Target(int)");
		storage.symbolNameOffsets[1] = AddString(storage, cursor, "public: int __cdecl TargetAlias(int)");
		const auto optionalId = AddString(storage, cursor, "Optional.Id");
		storage.symbolNameOffsets[2] = AddString(storage, cursor, "public: int __cdecl Optional(int)");
		storage.symbols = {{{0, requiredId, 0, 2, 0, 0, Projection::Requirement::Required, false},
			{1, optionalId, 2, 1, 0, 0, Projection::Requirement::Optional, false}}};
		PVOID published{};
		storage.bindings[0] = {std::span{storage.bindingSymbols}.first(1), &published, Util::force_cast_from(&Replacement)};
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
		overloads.symbols = {{{0, firstId, 0, 1, 0, 0, Projection::Requirement::Required, false},
			{1, secondId, 1, 1, 0, 0, Projection::Requirement::Required, false}}};
		Check(overloads.registry.Freeze({150, 0}));
		overloads.registry.Collect("public: int __cdecl Overload(int)", Util::force_cast_from(&Target));
		overloads.registry.Collect("public: int __cdecl Overload(double)", Util::force_cast_from(&Replacement));
		Check(overloads.registry.ValidateSymbols());


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
		storage.symbols = {{{0, id, 0, 1, 0, 1, Projection::Requirement::Required, false},
			{0, id, 1, 1, 2, 0, Projection::Requirement::Required, false}}};
		PVOID published{};
		storage.bindings[0] = {std::span{storage.bindingSymbols}.first(1), &published, Util::force_cast_from(&Replacement)};

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

	void TestGroupedBindingPublication()
	{
		RegistryStorage<4, 2, 0, 0> storage;
		size_t cursor{1};
		const auto oldFunction = AddString(storage, cursor, "Function.Old");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "FunctionOld");
		const auto oldVariable = AddString(storage, cursor, "Variable.Old");
		storage.symbolNameOffsets[1] = AddString(storage, cursor, "VariableOld");
		const auto newFunction = AddString(storage, cursor, "Function.New");
		storage.symbolNameOffsets[2] = AddString(storage, cursor, "FunctionNew");
		const auto debugVariable = AddString(storage, cursor, "Variable.Debug");
		storage.symbolNameOffsets[3] = AddString(storage, cursor, "VariableDebug");
		storage.versions[1] = {100, 0};
		storage.versions[2] = {200, 0};
		storage.symbols = {{{0, oldFunction, 0, 1, 0, 1, Projection::Requirement::Required, false},
			{1, oldVariable, 1, 1, 0, 2, Projection::Requirement::Optional, false},
			{2, newFunction, 2, 1, 2, 0, Projection::Requirement::Required, false},
			{3, debugVariable, 3, 1, 2, 0, Projection::Requirement::Optional, true}}};
		PVOID functionPublished{};
		PVOID variablePublished{};
		storage.bindingSymbols = {0, 2, 1, 3};
		storage.bindings[0] = {std::span{storage.bindingSymbols}.first(2), &functionPublished, Util::force_cast_from(&Replacement)};
		storage.bindings[1] = {std::span{storage.bindingSymbols}.subspan(2), &variablePublished, nullptr};

		Check(storage.registry.Freeze({99, 0}));
		storage.registry.Collect("FunctionOld", Util::force_cast_from(&Target));
		storage.registry.Collect("VariableOld", Util::force_cast_from(&Replacement2));
		Check(functionPublished == Util::force_cast_from(&Replacement));
		Check(variablePublished == nullptr);
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(functionPublished == Util::force_cast_from(&Target));
		Check(variablePublished == Util::force_cast_from(&Replacement2));

		Check(storage.registry.Freeze({150, 0}));
		storage.registry.Collect("VariableOld", Util::force_cast_from(&Target));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(functionPublished == Util::force_cast_from(&Replacement));
		Check(variablePublished == Util::force_cast_from(&Target));

		Check(storage.registry.Freeze({200, 0}));
		storage.registry.Collect("FunctionNew", Util::force_cast_from(&Replacement2));
		storage.registry.Collect("VariableDebug", Util::force_cast_from(&Target));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(functionPublished == Util::force_cast_from(&Replacement2));
#ifdef _DEBUG
		Check(variablePublished == Util::force_cast_from(&Target));
#else
		Check(variablePublished == nullptr);
#endif
		storage.registry.ResetSymbols();
		Check(functionPublished == Util::force_cast_from(&Replacement));
		Check(variablePublished == nullptr);
	}
}
