#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/EditorScope.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestEditorScope()
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
	}
}
