#include "HookHelper.hpp"
#include "SystemSettings.hpp"
#include "DwmSettings.hpp"
#include "ColorHooks.hpp"
#include "ColorHooks.Shell.hpp"
#include "Colorization.hpp"
#include <wil/registry.h>
#include <wil/win32_helpers.h>
#include <array>

using namespace OpenGlass;

namespace OpenGlass::ColorHooks
{
	HRESULT WINAPI MySetUserColorPreference(const void*, bool);

	struct ModuleHooks
	{
		PCWSTR m_moduleName{};
		PVOID m_DwmpSetColorizationParameters_Org{};
		PVOID m_DwmpGetColorizationParameters_Org{};
		PVOID m_SetUserColorPreference_Org{};
		HookHelper::ImportHook m_importHook;
	};

	std::array g_moduleHooks
	{
		ModuleHooks
		{
			L"themecpl.dll",
			&MyDwmpSetColorizationParameters,
			&MyDwmpGetColorizationParameters,
			&MySetUserColorPreference
		},
		ModuleHooks
		{
			L"themeui.dll",
			&MyDwmpSetColorizationParameters,
			&MyDwmpGetColorizationParameters,
			&MySetUserColorPreference
		}
	};
}

HRESULT WINAPI ColorHooks::MyDwmpSetColorizationParameters(const Colorization::ColorizationParameters* input, BOOL skipPersistence)
{
	if (!input) return E_INVALIDARG;

	try
	{
		const auto key = wil::reg::create_unique_key(HKEY_CURRENT_USER, DwmSettings::g_keyPath, wil::reg::key_access::readwrite);
		wil::unique_hkey machineDwm{};
		wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, DwmSettings::g_keyPath, machineDwm);
		const auto opaque = DwmSettings::IsTransparencyDisabled(
			DwmSettings::GetDword(key.get(), machineDwm.get(), L"ColorizationOpaqueBlend") != 0,
			Util::IsTransparencyEnabled(),
			Util::IsEnergySaverEnabled(),
			DwmSettings::GetDword(key.get(), machineDwm.get(), L"DisableGlassOnBattery", 1) != 0
		);

		const auto parameters = Colorization::CalculateColorization(*input, opaque);
		Colorization::WriteColorization(
			key.get(),
			skipPersistence ?
			nullptr : wil::reg::create_unique_key(
				HKEY_CURRENT_USER,
				LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\Accent)",
				wil::reg::key_access::readwrite
			).get(),
			parameters
		);

		DwmSettings::NotifyChange(FindWindowW(L"Dwm", nullptr), DwmSettings::UpdateType::Backdrop);

		return S_OK;
	}
	CATCH_RETURN();
}

HRESULT WINAPI ColorHooks::MyDwmpGetColorizationParameters(Colorization::ColorizationParameters* output)
{
	if (!output) return E_INVALIDARG;

	try
	{
		const auto userDwm = wil::reg::open_unique_key(HKEY_CURRENT_USER, DwmSettings::g_keyPath, wil::reg::key_access::read);
		wil::unique_hkey machineDwm{};
		wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, DwmSettings::g_keyPath, machineDwm, wil::reg::key_access::read);
		*output = Colorization::ReadColorization(userDwm.get(), machineDwm.get());
		return S_OK;
	}
	CATCH_RETURN();
}

HRESULT WINAPI ColorHooks::MySetUserColorPreference(const void*, bool)
{
	// Both callers would otherwise overwrite the parameters through another DWM setter call.
	return S_OK;
}

HookHelper::ImportHook* ColorHooks::Prepare(HookType type)
{
	auto& hooks = g_moduleHooks[static_cast<size_t>(type)];
	if (hooks.m_importHook.IsInstalled()) return nullptr;

	THROW_IF_FAILED(hooks.m_importHook.Prepare(
		GetModuleHandleW(hooks.m_moduleName),
		std::initializer_list<HookHelper::ImportDllDetourInfo>
		{
			{
				"dwmapi.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					{ MAKEINTRESOURCEA(131), &hooks.m_DwmpSetColorizationParameters_Org, hooks.m_DwmpSetColorizationParameters_Org },
					{ MAKEINTRESOURCEA(127), &hooks.m_DwmpGetColorizationParameters_Org, hooks.m_DwmpGetColorizationParameters_Org }
				}
			},
			{
				"uxtheme.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					{ MAKEINTRESOURCEA(122), &hooks.m_SetUserColorPreference_Org, hooks.m_SetUserColorPreference_Org }
				}
			},
			{
				"api-ms-win-core-com-l1-1-0.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					{ type == HookType::CPL ? "CoCreateInstance" : nullptr, reinterpret_cast<PVOID*>(&g_CoCreateInstance_Org), g_CoCreateInstance_Org }
				}
			}
		},
		// themecpl uses normal imports, themeui uses delay imports.
		type == HookType::UI
	));
	return &hooks.m_importHook;
}
