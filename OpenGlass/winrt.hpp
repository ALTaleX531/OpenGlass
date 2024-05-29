#pragma once
#include <windows.foundation.h>
#include <windows.foundation.collections.h>

#include <windows.ui.composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.interop.h>
#include <windows.ui.composition.effects.h>
#include <windows.ui.composition.interactions.h>

#include <windows.graphics.h>
#include <windows.graphics.interop.h>
#include <windows.graphics.directx.h>
#include <windows.graphics.effects.h>
#include <windows.graphics.effects.interop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.effects.h>
#include <winrt/Windows.UI.Composition.interactions.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.h>

#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.Graphics.directx.h>
#pragma comment(lib, "windowsapp")

namespace OpenGlass
{
	namespace wu = winrt::Windows::UI;
	namespace wuc = winrt::Windows::UI::Composition;
	namespace wf = winrt::Windows::Foundation;
	namespace wfn = winrt::Windows::Foundation::Numerics;
	namespace wg = winrt::Windows::Graphics;
	namespace wge = winrt::Windows::Graphics::Effects;
	namespace wgd = winrt::Windows::Graphics::DirectX;
}