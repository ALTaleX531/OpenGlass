#include "Colorization.hpp"
#include "DwmSettings.hpp"
#include <utility>
#include <wil/registry.h>

using namespace OpenGlass;

Colorization::ColorizationParameters Colorization::CalculateColorization(
	const ColorizationParameters& input,
	bool opaque
) noexcept
{
	auto result = input;
	const auto balances = CalculateWindows7Parameters(input.color, opaque);
	result.colorBalance = balances.colorBalance;
	result.afterglowBalance = balances.afterglowBalance;
	result.blurBalance = balances.blurBalance;
	return result;
}

Colorization::ColorizationParameters Colorization::ReadColorization(HKEY userDwm, HKEY machineDwm)
{
	ColorizationParameters result{};

	// Windows colors remain fixed HKCU state, as in the GUI.
	result.color = wil::reg::get_value_dword(userDwm, L"ColorizationColor");
	result.afterglow = wil::reg::get_value_dword(userDwm, L"ColorizationAfterglow");
	result.colorBalance = std::min(DwmSettings::GetOverridableDword(userDwm, machineDwm, L"ColorizationColorBalance", L"ColorizationColorBalanceOverride"), 100ul);
	result.afterglowBalance = std::min(DwmSettings::GetOverridableDword(userDwm, machineDwm, L"ColorizationAfterglowBalance", L"ColorizationAfterglowBalanceOverride"), 100ul);
	result.blurBalance = std::min(DwmSettings::GetOverridableDword(userDwm, machineDwm, L"ColorizationBlurBalance", L"ColorizationBlurBalanceOverride"), 100ul);
	wil::reg::get_value_dword_nothrow(userDwm, L"EnableWindowColorization", &result.enableWindowColorization);
	wil::reg::get_value_dword_nothrow(userDwm, L"ColorizationGlassAttribute", &result.glassAttribute);

	const auto opacity = std::min(DwmSettings::GetDword(userDwm, machineDwm, L"GlassOpacity", 63), 100ul);
	// GUI intensity is independent of Windows alpha. Retain the exact incoming
	// byte when it already represents this percentage, avoiding Cancel drift.
	if (CalculateVistaOpacity(result.color) != opacity)
	{
		result.color = (result.color & 0x00FFFFFF) | (CalculateIntensityAlpha(opacity) << 24);
	}
	// These theme DLLs decode alpha FF as ((ColorBalance - 10) * .75 / 100 + .1).
	// 130 round-trips FF. This is only their getter encoding, never a stored weight.
	if ((result.color >> 24) == 255) result.colorBalance = 130;

	return result;
}

void Colorization::WriteColorization(HKEY userDwm, HKEY userAccent, const ColorizationParameters& parameters)
{
	// Windows Accent values use opaque COLORREF, unlike the two Colorization ARGBs.
	const DWORD accent = RGB((parameters.color >> 16) & 0xFF, (parameters.color >> 8) & 0xFF, parameters.color & 0xFF) | 0xFF000000;

	const std::pair<PCWSTR, DWORD> values[]
	{
		{ L"ColorizationColor", parameters.color },
		{ L"ColorizationAfterglow", parameters.afterglow },
		{ L"AccentColor", accent },
		// Match the GUI's balance destinations; these also supersede machine Overrides.
		{ L"ColorizationColorBalanceOverride", parameters.colorBalance },
		{ L"ColorizationAfterglowBalanceOverride", parameters.afterglowBalance },
		{ L"ColorizationBlurBalanceOverride", parameters.blurBalance },
		{ L"GlassOpacity", CalculateVistaOpacity(parameters.color) }
	};
	for (const auto& [name, value] : values)
	{
		wil::reg::set_value_dword(userDwm, name, value);
	}
	if (userAccent)
	{
		wil::reg::set_value_dword(userAccent, L"AccentColorMenu", accent);
	}
}
