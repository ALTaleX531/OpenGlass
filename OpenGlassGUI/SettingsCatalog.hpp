#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include "DwmSettings.hpp"

namespace OpenGlass::Settings
{
	inline constexpr unsigned CatalogVersion = 3;

	enum class Scope : std::uint8_t
	{
		User,
		Machine
	};

	enum class ValueType : std::uint8_t
	{
		Dword,
		String
	};

	enum class AssetRole : std::uint8_t
	{
		None,
		ThemeAtlas,
		Reflection,
		Material
	};

	using DwmSettings::UpdateType;

	enum class Id : std::uint16_t
	{
		ColorizationColor,
		ColorizationColorOverride,
		ColorizationAfterglow,
		ColorizationAfterglowOverride,
		ColorizationColorBalance,
		ColorizationColorBalanceOverride,
		ColorizationAfterglowBalance,
		ColorizationAfterglowBalanceOverride,
		ColorizationBlurBalance,
		ColorizationBlurBalanceOverride,
		ColorizationColorInactive,
		GlassOpacity,
		GlassOpacityInactive,
		ColorizationColorCaption,
		ColorizationColorCaptionInactive,
		ColorizationColorCaptionMaximized,
		ColorizationColorCaptionInactiveMaximized,
		ColorizationOpaqueBlend,
		ColorizationBaseTransparent,
		ColorizationBaseMaximized,
		ColorizationBaseOpaque,
		ColorizationOpaqueBlendPriority,
		ColorizationOpacity,
		ColorizationOpacityInactive,
		ColorizationOpacityMaximized,
		ColorizationOpacityInactiveMaximized,
		GlassType,
		GlassOverrideAccent,
		CustomThemeReflection,
		ColorizationGlassReflectionIntensity,
		ColorizationGlassReflectionOpacity,
		ColorizationGlassReflectionOpacityInactive,
		ColorizationGlassReflectionOpacityMaximized,
		ColorizationGlassReflectionOpacityInactiveMaximized,
		ColorizationGlassReflectionParallaxIntensity,
		ColorizationGlassReflectionPolicy,
		BlurDeviation,
		BlurOptimization,
		RoundRectRadius,
		CustomThemeMaterial,
		MaterialOpacity,
		UseDirect3DRendering,
		CaptionButtons,
		CenterCaption,
		TextGlowMode,
		CustomThemeAtlas,
		DisableModernBorders,
		DisableGlassOnBattery,
		DisabledHooks,
		GlassSafetyZoneMode,
		MinMaxButtonGlowId,
		CloseButtonGlowId,
		ToolCloseButtonGlowId,
		EnableTransparency,
		// Append settings: interrupted-operation records persist these numeric IDs.
		EnableUxMitigation,
		Count
	};

	struct Spec
	{
		Id id;
		std::wstring_view name;
		ValueType type;
		AssetRole assetRole;
		UpdateType impact;
		unsigned introducedIn{ 1 };
		bool includeInPresetPacks{ true };
		unsigned retiredFromPresetsIn{};
	};

	inline constexpr std::array<Spec, static_cast<std::size_t>(Id::Count)> Catalog
	{{
		{ Id::ColorizationColor, L"ColorizationColor", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, true, 2 },
		{ Id::ColorizationColorOverride, L"ColorizationColorOverride", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, false },
		{ Id::ColorizationAfterglow, L"ColorizationAfterglow", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, true, 2 },
		{ Id::ColorizationAfterglowOverride, L"ColorizationAfterglowOverride", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, false },
		{ Id::ColorizationColorBalance, L"ColorizationColorBalance", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, true, 2 },
		{ Id::ColorizationColorBalanceOverride, L"ColorizationColorBalanceOverride", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationAfterglowBalance, L"ColorizationAfterglowBalance", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, true, 2 },
		{ Id::ColorizationAfterglowBalanceOverride, L"ColorizationAfterglowBalanceOverride", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationBlurBalance, L"ColorizationBlurBalance", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, true, 2 },
		{ Id::ColorizationBlurBalanceOverride, L"ColorizationBlurBalanceOverride", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationColorInactive, L"ColorizationColorInactive", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::GlassOpacity, L"GlassOpacity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::GlassOpacityInactive, L"GlassOpacityInactive", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationColorCaption, L"ColorizationColorCaption", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::ColorizationColorCaptionInactive, L"ColorizationColorCaptionInactive", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::ColorizationColorCaptionMaximized, L"ColorizationColorCaptionMaximized", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::ColorizationColorCaptionInactiveMaximized, L"ColorizationColorCaptionInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::ColorizationOpaqueBlend, L"ColorizationOpaqueBlend", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationBaseTransparent, L"ColorizationBaseTransparent", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationBaseMaximized, L"ColorizationBaseMaximized", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationBaseOpaque, L"ColorizationBaseOpaque", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationOpaqueBlendPriority, L"ColorizationOpaqueBlendPriority", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationOpacity, L"ColorizationOpacity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationOpacityInactive, L"ColorizationOpacityInactive", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationOpacityMaximized, L"ColorizationOpacityMaximized", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationOpacityInactiveMaximized, L"ColorizationOpacityInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::GlassType, L"GlassType", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::GlassOverrideAccent, L"GlassOverrideAccent", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::CustomThemeReflection, L"CustomThemeReflection", ValueType::String, AssetRole::Reflection, UpdateType::Theme },
		{ Id::ColorizationGlassReflectionIntensity, L"ColorizationGlassReflectionIntensity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionOpacity, L"ColorizationGlassReflectionOpacity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionOpacityInactive, L"ColorizationGlassReflectionOpacityInactive", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionOpacityMaximized, L"ColorizationGlassReflectionOpacityMaximized", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionOpacityInactiveMaximized, L"ColorizationGlassReflectionOpacityInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionParallaxIntensity, L"ColorizationGlassReflectionParallaxIntensity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::ColorizationGlassReflectionPolicy, L"ColorizationGlassReflectionPolicy", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::BlurDeviation, L"BlurDeviation", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::BlurOptimization, L"BlurOptimization", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::RoundRectRadius, L"RoundRectRadius", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::CustomThemeMaterial, L"CustomThemeMaterial", ValueType::String, AssetRole::Material, UpdateType::Theme },
		{ Id::MaterialOpacity, L"MaterialOpacity", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::UseDirect3DRendering, L"UseDirect3DRendering", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::CaptionButtons, L"CaptionButtons", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::CenterCaption, L"CenterCaption", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::TextGlowMode, L"TextGlowMode", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::CustomThemeAtlas, L"CustomThemeAtlas", ValueType::String, AssetRole::ThemeAtlas, UpdateType::Theme },
		{ Id::DisableModernBorders, L"DisableModernBorders", ValueType::Dword, AssetRole::None, UpdateType::Theme },
		{ Id::DisableGlassOnBattery, L"DisableGlassOnBattery", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::DisabledHooks, L"DisabledHooks", ValueType::Dword, AssetRole::None, UpdateType::RestartRequired },
		{ Id::GlassSafetyZoneMode, L"GlassSafetyZoneMode", ValueType::Dword, AssetRole::None, UpdateType::Backdrop },
		{ Id::MinMaxButtonGlowId, L"MINMAXBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateType::RestartRequired, 1, false },
		{ Id::CloseButtonGlowId, L"CLOSEBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateType::RestartRequired, 1, false },
		{ Id::ToolCloseButtonGlowId, L"TOOLCLOSEBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateType::RestartRequired, 1, false },
		// Raw Windows recovery identity; presets keep the ColorizationOpaqueBlend field.
		{ Id::EnableTransparency, L"EnableTransparency", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 1, false },
		{ Id::EnableUxMitigation, L"EnableUxMitigation", ValueType::Dword, AssetRole::None, UpdateType::Backdrop, 3 }
	}};

	consteval bool IsValidCatalog()
	{
		for (std::size_t index = 0; index < Catalog.size(); ++index)
		{
			const auto& spec = Catalog[index];
			if (static_cast<std::size_t>(spec.id) != index || spec.name.empty() || spec.introducedIn == 0 || spec.introducedIn > CatalogVersion) return false;
			if ((spec.type == ValueType::String) != (spec.assetRole != AssetRole::None)) return false;
			if (!spec.includeInPresetPacks && (spec.type != ValueType::Dword || spec.assetRole != AssetRole::None)) return false;
			for (std::size_t other = 0; other < index; ++other)
			{
				if (Catalog[other].name == spec.name) return false;
			}
		}
		return true;
	}

	static_assert(IsValidCatalog());

	[[nodiscard]] constexpr const Spec& Get(Id id) noexcept
	{
		return Catalog[static_cast<std::size_t>(id)];
	}

	static_assert(Get(Id::GlassOverrideAccent).impact == UpdateType::Backdrop);
	static_assert(Get(Id::GlassSafetyZoneMode).impact == UpdateType::Backdrop);
	static_assert(Get(Id::UseDirect3DRendering).impact == UpdateType::Backdrop);
	static_assert(!Get(Id::MinMaxButtonGlowId).includeInPresetPacks);
	static_assert(!Get(Id::CloseButtonGlowId).includeInPresetPacks);
	static_assert(!Get(Id::ToolCloseButtonGlowId).includeInPresetPacks);

	[[nodiscard]] constexpr bool IsPresetPackSetting(const Spec& spec, unsigned catalogVersion = CatalogVersion) noexcept
	{
		return spec.includeInPresetPacks && spec.introducedIn <= catalogVersion
			&& (!spec.retiredFromPresetsIn || spec.retiredFromPresetsIn > CatalogVersion || catalogVersion < spec.retiredFromPresetsIn);
	}

	[[nodiscard]] constexpr bool IsColorOverride(Id id) noexcept
	{
		return id == Id::ColorizationColorOverride || id == Id::ColorizationAfterglowOverride;
	}

	[[nodiscard]] constexpr bool IsWindowsColorBase(Id id) noexcept
	{
		return id == Id::ColorizationColor || id == Id::ColorizationAfterglow
			|| id == Id::ColorizationColorBalance || id == Id::ColorizationAfterglowBalance
			|| id == Id::ColorizationBlurBalance;
	}

	[[nodiscard]] constexpr std::size_t PresetPackSettingCount(unsigned catalogVersion = CatalogVersion) noexcept
	{
		std::size_t count{};
		for (const auto& spec : Catalog)
		{
			if (IsPresetPackSetting(spec, catalogVersion)) ++count;
		}
		return count;
	}

	[[nodiscard]] constexpr const Spec* Find(std::wstring_view name) noexcept
	{
		for (const auto& spec : Catalog)
		{
			if (spec.name == name)
			{
				return &spec;
			}
		}
		return nullptr;
	}

}
