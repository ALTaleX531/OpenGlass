#include "pch.h"
#include "TestHarness.hpp"
#include "RegistryValueResolver.hpp"
#include <array>

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestOverridableRegistryValueResolution()
	{
		struct Case
		{
			std::optional<DWORD> userOverride, userBase, machineOverride, machineBase;
			DWORD expected;
		};
		const auto absent = std::nullopt;
		const std::array cases
		{
			Case{11, 22, 33, 44, 11},
			Case{absent, 22, 33, 44, 33},
			Case{absent, 22, absent, 44, 22},
			Case{absent, absent, absent, 44, 44},
			Case{absent, absent, absent, absent, 55},
			Case{0, 22, 33, 44, 0}
		};
		for (const auto& item : cases)
		{
			Check(ResolveOverridableRegistryValue(item.userOverride, item.userBase,
				item.machineOverride, item.machineBase, DWORD{55}) == item.expected);
		}
		Check(GetEditorRegistryNotice(true, false, true) == EditorRegistryNotice::Inherited);
		Check(GetEditorRegistryNotice(true, true, true) == EditorRegistryNotice::None);
		Check(GetEditorRegistryNotice(false, true, true) == EditorRegistryNotice::Overridden);
		Check(GetEditorRegistryNotice(false, true, false) == EditorRegistryNotice::Overridden);
	}
}
