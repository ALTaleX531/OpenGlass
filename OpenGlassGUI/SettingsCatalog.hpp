#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace OpenGlass::Settings
{
	inline constexpr unsigned CatalogVersion = 2;

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

	enum class UpdateImpact : std::uint8_t
	{
		None = 0,
		Colorization = 1,
		Theme = 2,
		RestartRequired = 4
	};

	constexpr UpdateImpact operator|(UpdateImpact left, UpdateImpact right) noexcept
	{
		return static_cast<UpdateImpact>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
	}

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
		Count
	};

	struct Spec
	{
		Id id;
		std::wstring_view name;
		ValueType type;
		AssetRole assetRole;
		UpdateImpact impact;
		unsigned introducedIn{ 1 };
		bool includeInPresetPacks{ true };
		unsigned retiredFromPresetsIn{};
	};

	inline constexpr std::array<Spec, static_cast<std::size_t>(Id::Count)> Catalog
	{{
		{ Id::ColorizationColor, L"ColorizationColor", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, true, 2 },
		{ Id::ColorizationColorOverride, L"ColorizationColorOverride", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, false },
		{ Id::ColorizationAfterglow, L"ColorizationAfterglow", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, true, 2 },
		{ Id::ColorizationAfterglowOverride, L"ColorizationAfterglowOverride", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, false },
		{ Id::ColorizationColorBalance, L"ColorizationColorBalance", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, true, 2 },
		{ Id::ColorizationColorBalanceOverride, L"ColorizationColorBalanceOverride", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationAfterglowBalance, L"ColorizationAfterglowBalance", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, true, 2 },
		{ Id::ColorizationAfterglowBalanceOverride, L"ColorizationAfterglowBalanceOverride", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationBlurBalance, L"ColorizationBlurBalance", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization, 1, true, 2 },
		{ Id::ColorizationBlurBalanceOverride, L"ColorizationBlurBalanceOverride", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationColorInactive, L"ColorizationColorInactive", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::GlassOpacity, L"GlassOpacity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::GlassOpacityInactive, L"GlassOpacityInactive", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationColorCaption, L"ColorizationColorCaption", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::ColorizationColorCaptionInactive, L"ColorizationColorCaptionInactive", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::ColorizationColorCaptionMaximized, L"ColorizationColorCaptionMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::ColorizationColorCaptionInactiveMaximized, L"ColorizationColorCaptionInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::ColorizationOpaqueBlend, L"ColorizationOpaqueBlend", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationBaseTransparent, L"ColorizationBaseTransparent", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationBaseMaximized, L"ColorizationBaseMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationBaseOpaque, L"ColorizationBaseOpaque", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationOpaqueBlendPriority, L"ColorizationOpaqueBlendPriority", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationOpacity, L"ColorizationOpacity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationOpacityInactive, L"ColorizationOpacityInactive", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationOpacityMaximized, L"ColorizationOpacityMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationOpacityInactiveMaximized, L"ColorizationOpacityInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::GlassType, L"GlassType", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::GlassOverrideAccent, L"GlassOverrideAccent", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::CustomThemeReflection, L"CustomThemeReflection", ValueType::String, AssetRole::Reflection, UpdateImpact::Theme },
		{ Id::ColorizationGlassReflectionIntensity, L"ColorizationGlassReflectionIntensity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionOpacity, L"ColorizationGlassReflectionOpacity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionOpacityInactive, L"ColorizationGlassReflectionOpacityInactive", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionOpacityMaximized, L"ColorizationGlassReflectionOpacityMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionOpacityInactiveMaximized, L"ColorizationGlassReflectionOpacityInactiveMaximized", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionParallaxIntensity, L"ColorizationGlassReflectionParallaxIntensity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::ColorizationGlassReflectionPolicy, L"ColorizationGlassReflectionPolicy", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::BlurDeviation, L"BlurDeviation", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::BlurOptimization, L"BlurOptimization", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::RoundRectRadius, L"RoundRectRadius", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::CustomThemeMaterial, L"CustomThemeMaterial", ValueType::String, AssetRole::Material, UpdateImpact::Theme },
		{ Id::MaterialOpacity, L"MaterialOpacity", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::UseDirect3DRendering, L"UseDirect3DRendering", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::CaptionButtons, L"CaptionButtons", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::CenterCaption, L"CenterCaption", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::TextGlowMode, L"TextGlowMode", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::CustomThemeAtlas, L"CustomThemeAtlas", ValueType::String, AssetRole::ThemeAtlas, UpdateImpact::Theme },
		{ Id::DisableModernBorders, L"DisableModernBorders", ValueType::Dword, AssetRole::None, UpdateImpact::Theme },
		{ Id::DisableGlassOnBattery, L"DisableGlassOnBattery", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::DisabledHooks, L"DisabledHooks", ValueType::Dword, AssetRole::None, UpdateImpact::RestartRequired },
		{ Id::GlassSafetyZoneMode, L"GlassSafetyZoneMode", ValueType::Dword, AssetRole::None, UpdateImpact::Colorization },
		{ Id::MinMaxButtonGlowId, L"MINMAXBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateImpact::RestartRequired, 1, false },
		{ Id::CloseButtonGlowId, L"CLOSEBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateImpact::RestartRequired, 1, false },
		{ Id::ToolCloseButtonGlowId, L"TOOLCLOSEBUTTONGLOWid", ValueType::Dword, AssetRole::None, UpdateImpact::RestartRequired, 1, false }
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

	static_assert(Get(Id::GlassOverrideAccent).impact == UpdateImpact::Colorization);
	static_assert(Get(Id::GlassSafetyZoneMode).impact == UpdateImpact::Colorization);
	static_assert(Get(Id::UseDirect3DRendering).impact == UpdateImpact::Colorization);
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
