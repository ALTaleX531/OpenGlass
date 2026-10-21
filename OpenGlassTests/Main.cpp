#include "pch.h"
#include "TestHarness.hpp"
#include "TestCases.hpp"
#include <wx/app.h>
#include <cstdio>
#include <string_view>
#include <wil/result.h>

wxIMPLEMENT_APP_NO_MAIN(wxApp);

namespace
{
	int g_failures{};
	struct TestCase
	{
		std::string_view suite;
		const char* name;
		void (*run)();
	};
	using namespace OpenGlassTests;
	constexpr TestCase g_tests[]
	{
		{ "EditorScope", "EditorScope.EditorScope", &TestEditorScope },
		{ "RectF", "RectF.PixelAlign", &TestPixelAlign },
		{ "RectF", "RectF.Transform2DBounds", &TestTransform2DBounds },
		{ "HookHelper", "HookHelper.HookRundown", &TestHookRundown },
		{ "HookHelper", "HookHelper.ImportHooks", &TestImportHooks },
		{ "ProjectionHelper", "ProjectionHelper.VersionsAndFields", &TestVersionsAndFields },
		{ "ProjectionRuntime", "ProjectionRuntime.CompleteNameResolutionAndFallback", &TestCompleteNameResolutionAndFallback },
		{ "SymbolCatalog", "SymbolCatalog.SymbolCatalogCollection", &TestSymbolCatalogCollection },
		{ "ProjectionHelper", "ProjectionHelper.AtomicCommitAndDetourStorage", &TestAtomicCommitAndDetourStorage },
		{ "ProjectionRuntime", "ProjectionRuntime.LogicalSymbolBindings", &TestLogicalSymbolBindings },
		{ "ProjectionRuntime", "ProjectionRuntime.GroupedBindingPublication", &TestGroupedBindingPublication },
		{ "PngAssetValidation", "PngAssetValidation.PngAssetValidation", &TestPngAssetValidation },
		{ "ThemeAtlasLayout", "ThemeAtlasLayout.ThemeAtlasLayoutParser", &TestThemeAtlasLayoutParser },
		{ "DwmSettings", "DwmSettings.DwmSettings", &TestDwmSettings },
		{ "RegistryValueResolver", "RegistryValueResolver.OverridableRegistryValueResolution", &TestOverridableRegistryValueResolution },
		{ "ColorizationPresets", "ColorizationPresets.ColorizationPresets", &TestColorizationPresets },
		{ "BlurSettings", "BlurSettings.BlurSettings", &TestBlurSettings },
		{ "SettingsCatalog", "SettingsCatalog.SettingsCatalog", &TestSettingsCatalog },
		{ "RegistryConfig", "RegistryConfig.TransparencyRegistry", &TestTransparencyRegistry },
		{ "ConfigurationMigration", "ConfigurationMigration.ConfigurationMergePolicy", &TestConfigurationMergePolicy },
		{ "PreviewJournal", "PreviewJournal.PreviewJournal", &TestPreviewJournal },
		{ "PreviewJournal", "PreviewJournal.NetPreviewJournal", &TestNetPreviewJournal },
		{ "ShellColorRefresh", "ShellColorRefresh.ShellColorRefresh", &TestShellColorRefresh },
		{ "ColorPreference", "ColorPreference.AutomaticColorPreview", &TestAutomaticColorPreview },
		{ "EffectiveConfiguration", "EffectiveConfiguration.EffectiveConfiguration", &TestEffectiveConfiguration },
		{ "EffectiveConfiguration", "EffectiveConfiguration.ConfigurationStrings", &TestConfigurationStrings },
		{ "EffectiveConfiguration", "EffectiveConfiguration.ConfigurationReset", &TestConfigurationReset },
		{ "ThemeColorization", "ThemeColorization.ThemeColorization", &TestThemeColorization },
		{ "PresetPackage", "PresetPackage.PresetProvenance", &TestPresetProvenance },
		{ "ConfigurationResources", "ConfigurationResources.ConfigurationResources", &TestConfigurationResources },
		{ "PresetPackage", "PresetPackage.PresetPackageRoundTrip", &TestPresetPackageRoundTrip },
	};

	void PrintUsage()
	{
		puts("Usage: OpenGlassTests.exe [--list | --test SUITE.CASE | --suite SUITE | --help]\n"
			"Without arguments, runs every test. Names are case-sensitive.");
	}
}

void OpenGlassTests::Check(bool condition, const std::source_location& location)
{
	if (!condition)
	{
		++g_failures;
		fprintf(stderr, "Check failed at %s:%u\n", location.file_name(), location.line());
	}
}

int main(int argc, char** argv)
{
	std::string_view mode = argc > 1 ? argv[1] : "";
	if (argc == 2 && (mode == "--help" || mode == "--list"))
	{
		if (mode == "--help") PrintUsage();
		else for (const auto& test : g_tests) puts(test.name);
		return 0;
	}
	if (argc != 1 && !(argc == 3 && (mode == "--test" || mode == "--suite")))
	{
		PrintUsage();
		return 2;
	}
	const std::string_view selection = argc == 3 ? argv[2] : "";
	unsigned executed{}, failed{};
	for (const auto& test : g_tests)
	{
		if (argc != 1 && (mode == "--test" ? selection != test.name : selection != test.suite)) continue;
		printf("[RUN ] %s\n", test.name);
		fflush(stdout);
		const auto before = g_failures;
		try { test.run(); }
		catch (const std::exception& error)
		{
			fprintf(stderr, "%s: %s\n", test.name, error.what());
			++g_failures;
		}
		catch (...)
		{
			fprintf(stderr, "%s: 0x%08lX\n", test.name, wil::ResultFromCaughtException());
			++g_failures;
		}
		const bool passed = g_failures == before;
		printf("[%s] %s\n", passed ? "PASS" : "FAIL", test.name);
		++executed;
		if (!passed) ++failed;
	}
	if (!executed)
	{
		fprintf(stderr, "Unknown test selection: %s. Use --list to see available tests.\n", argv[2]);
		return 2;
	}
	printf("%u tests run, %u failed.\n", executed, failed);
	return failed ? 1 : 0;
}
