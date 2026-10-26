#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/SettingsCatalog.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
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
		Check(Settings::PresetPackSettingCount(1) == Settings::PresetPackSettingCount(2) + 5);
		Check(Settings::PresetPackSettingCount(2) + 1 == Settings::PresetPackSettingCount());
		Check(!Settings::IsPresetPackSetting(Settings::Get(Settings::Id::EnableUxMitigation), 2));
		Check(Settings::Get(Settings::Id::EnableUxMitigation).impact == Settings::UpdateType::Backdrop);
		Check(Settings::Get(Settings::Id::GlassOverrideAccent).impact == Settings::UpdateType::Backdrop);
		Check(Settings::Get(Settings::Id::GlassSafetyZoneMode).impact == Settings::UpdateType::Backdrop);
		Check(Settings::Get(Settings::Id::UseDirect3DRendering).impact == Settings::UpdateType::Backdrop);
		Check(!Settings::Get(Settings::Id::MinMaxButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::CloseButtonGlowId).includeInPresetPacks);
		Check(!Settings::Get(Settings::Id::ToolCloseButtonGlowId).includeInPresetPacks);
		Check(!Settings::IsPresetPackSetting(Settings::Get(Settings::Id::EnableTransparency)));
		Check(Settings::PresetPackSettingCount() + 11 == Settings::Catalog.size());
		Check(Settings::Find(L"NotAnOpenGlassSetting") == nullptr);
	}
}
