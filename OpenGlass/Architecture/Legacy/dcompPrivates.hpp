#pragma once
#include <dcomp.h>
#include <windows.foundation.h>
#include <Windows.UI.Composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.interop.h>
#include <windows.graphics.h>
#include <windows.graphics.directx.h>
#include <windows.graphics.directx.direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>

namespace abi
{
	using namespace ::ABI::Windows::Foundation;
	using namespace ::ABI::Windows::Foundation::Numerics;
	using namespace ::ABI::Windows::UI::Composition;
	using namespace ::ABI::Windows::UI::Composition::Desktop;
	using namespace ::ABI::Windows::Graphics;
	using namespace ::ABI::Windows::Graphics::DirectX;
}

namespace OpenGlass
{
	DECLARE_INTERFACE_IID_(IDCompositionDesktopDevicePartner, IDCompositionDesktopDevice, "D14B6158-C3FA-4BCE-9C1F-B61D8665EAB0") 
	{
	};
}
