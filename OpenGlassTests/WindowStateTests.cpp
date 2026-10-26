#include "pch.h"
#include "TestHarness.hpp"
#include "WindowState.hpp"
#include <wil/resource.h>

namespace OpenGlassTests
{
	void TestWindowStateOverrides()
	{
		using namespace OpenGlass::WindowState;
		const auto window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		Check(window != nullptr);
		if (!window) return;
		const auto cleanup = wil::scope_exit([&] { DestroyWindow(window); });

		for (const auto property : { ActivationOverrideProperty, MaximizationOverrideProperty })
		{
			Check(!Resolve(window, property, false));
			Check(Resolve(window, property, true));
			Check(SetPropW(window, property, reinterpret_cast<HANDLE>(ForceFalse)) != FALSE);
			Check(!Resolve(window, property, false));
			Check(!Resolve(window, property, true));
			Check(SetPropW(window, property, reinterpret_cast<HANDLE>(ForceTrue)) != FALSE);
			Check(Resolve(window, property, false));
			Check(Resolve(window, property, true));
			Check(SetPropW(window, property, nullptr) != FALSE);
			Check(!Resolve(window, property, false));
			Check(Resolve(window, property, true));
			Check(SetPropW(window, property, reinterpret_cast<HANDLE>(3)) != FALSE);
			Check(!Resolve(window, property, false));
			Check(Resolve(window, property, true));
			RemovePropW(window, property);
			Check(!Resolve(window, property, false));
			Check(Resolve(window, property, true));
		}

		Check(SetPropW(window, ActivationOverrideProperty, reinterpret_cast<HANDLE>(ForceTrue)) != FALSE);
		Check(SetPropW(window, MaximizationOverrideProperty, reinterpret_cast<HANDLE>(ForceFalse)) != FALSE);
		Check(Resolve(window, ActivationOverrideProperty, false));
		Check(!Resolve(window, MaximizationOverrideProperty, true));
		RemovePropW(window, ActivationOverrideProperty);
		Check(!Resolve(window, ActivationOverrideProperty, false));
		Check(!Resolve(window, MaximizationOverrideProperty, true));
		RemovePropW(window, MaximizationOverrideProperty);

		// Window property keys can also be supplied as global atoms.
		const auto atom = GlobalAddAtomW(ActivationOverrideProperty);
		Check(atom != 0);
		const auto cleanupAtom = wil::scope_exit([&] { GlobalDeleteAtom(atom); });
		Check(SetPropW(window, MAKEINTATOM(atom), reinterpret_cast<HANDLE>(ForceFalse)) != FALSE);
		Check(!Resolve(window, ActivationOverrideProperty, true));
		RemovePropW(window, MAKEINTATOM(atom));
		Check(SetPropW(window, L"openglass.activationoverride", reinterpret_cast<HANDLE>(ForceFalse)) != FALSE);
		Check(!Resolve(window, ActivationOverrideProperty, true));
		RemovePropW(window, ActivationOverrideProperty);
	}
}
