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
#include "ProjectionHelper.hpp"

namespace OpenGlass
{
	namespace abi
	{
		using namespace ::ABI::Windows::Foundation;
		using namespace ::ABI::Windows::Foundation::Numerics;
		using namespace ::ABI::Windows::UI::Composition;
		using namespace ::ABI::Windows::UI::Composition::Desktop;
		using namespace ::ABI::Windows::Graphics;
		using namespace ::ABI::Windows::Graphics::DirectX;
	}
	
	DECLARE_INTERFACE_IID_(IDCompositionVisualPartner, IDCompositionVisual3, "8819f277-549c-4862-8812-b114f85d1aae") {};
	DECLARE_INTERFACE_IID_(IDCompositionVisualPartnerWinRTInterop, IDCompositionVisualPartner, "fe93b735-e574-4a5d-a21a-f705c21945fa")
	{
		DECLSPEC_DIRECT_PROJECTION HRESULT GetVisualCollection(abi::IVisualCollection** collection)
		{
			return std::invoke(
				HookHelper::get_vftable_from<decltype(&IDCompositionVisualPartnerWinRTInterop::GetVisualCollection)>(this)[45],
				this,
				collection
			);
		}
	};

	DECLARE_INTERFACE_IID_(IDCompositionRegionClipPartner, IDCompositionClip, "a71f2c6a-2077-4d0f-8dfb-951ab2838ede")
	{
		IFACEMETHOD(SetRectangles)(
			LPCRECT pRects,
			UINT count,
			int offsetX,
			int offsetY
		) PURE;
	};

	DECLARE_INTERFACE_IID_(IVisualTargetPartner, IUnknown, "DBA1813C-60C5-4A42-A4D2-3380CDDCE8A1")
	{
		IFACEMETHOD(GetRoot)(abi::IVisual** rootVisual) PURE;
		IFACEMETHOD(SetRoot)(abi::IVisual* rootVisual) PURE;
	};

	DECLARE_INTERFACE_IID_(IDCompositionDesktopDevicePartner6, IDCompositionDesktopDevice, "e01eb649-787e-4560-b398-0de7a2065d8b")
	{
		DECLSPEC_DIRECT_PROJECTION HRESULT CreateSharedResource(REFIID riid, PVOID* ppvObject)
		{
			return std::invoke(
				HookHelper::get_vftable_from<decltype(&IDCompositionDesktopDevicePartner6::CreateSharedResource)>(this)[27],
				this,
				riid,
				ppvObject
			);
		}
		DECLSPEC_DIRECT_PROJECTION HRESULT OpenSharedResourceHandle(IUnknown* unknown, HANDLE* resourceHandle)
		{
			return std::invoke(
				HookHelper::get_vftable_from<decltype(&IDCompositionDesktopDevicePartner6::OpenSharedResourceHandle)>(this)[28],
				this,
				unknown,
				resourceHandle
			);
		}
	};

	namespace dcomp
	{
		namespace Windows::UI::Composition
		{
			struct ProxyObject
			{
				DECLSPEC_DIRECT_PROJECTION UINT GetHandleId() const
				{
					return *reinterpret_cast<UINT const*>(reinterpret_cast<ULONG_PTR>(this) + 144);
				}
				DECLSPEC_DIRECT_PROJECTION UINT& GetHandleIdReference()
				{
					return *reinterpret_cast<UINT*>(reinterpret_cast<ULONG_PTR>(this) + 144);
				}
			};
		}
		namespace DirectComposition
		{
			struct CResourceProxy
			{
				DECLSPEC_DIRECT_PROJECTION UINT GetHandleId() const
				{
					return *reinterpret_cast<UINT const*>(reinterpret_cast<ULONG_PTR>(this) + 12);
				}
				DECLSPEC_DIRECT_PROJECTION UINT& GetHandleIdReference()
				{
					return *reinterpret_cast<UINT*>(reinterpret_cast<ULONG_PTR>(this) + 12);
				}
			};
		}

		DECLSPEC_DIRECT_PROJECTION DirectComposition::CResourceProxy* DCompositionObjectToResourceProxy(IUnknown* unknown)
		{
			return reinterpret_cast<DirectComposition::CResourceProxy*>(unknown);
		}
		DECLSPEC_DIRECT_PROJECTION Windows::UI::Composition::ProxyObject* GetProxyObjectFromICompositionRectangleGeometry(
			abi::ICompositionRectangleGeometry* rectangleGeometry
		)
		{
			return reinterpret_cast<Windows::UI::Composition::ProxyObject*>(reinterpret_cast<ULONG_PTR>(rectangleGeometry) - 184);
		}
	}
}
