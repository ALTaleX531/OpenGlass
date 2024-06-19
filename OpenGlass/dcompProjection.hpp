#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "wil.hpp"
#include "OSHelper.hpp"
#include "HookHelper.hpp"

namespace ABI::Windows::UI::Composition
{
	struct ICompositionInteractionPartner;
	struct ICompositionManipulationTransformPartner;
}
namespace OpenGlass::dcomp
{
	inline const auto DCompositionCreateTargetForHandle{ reinterpret_cast<HRESULT(WINAPI*)(HANDLE, IDCompositionTarget**)>(GetProcAddress(GetModuleHandleW(L"dcomp.dll"), MAKEINTRESOURCEA(1038))) };

	DECLARE_INTERFACE_IID_(InteropCompositionTarget, IUnknown, "EACDD04C-117E-4E17-88F4-D1B12B0E3D89")
	{
		STDMETHOD(SetRoot)(THIS_ IN IDCompositionVisual * visual) PURE;
	};

	DECLARE_INTERFACE_IID_(IDCompositionDesktopDevicePartner, IDCompositionDesktopDevice, "D14B6158-C3FA-4BCE-9C1F-B61D8665EAB0")
	{
		HRESULT CreateSharedResource(REFIID riid, PVOID* ppvObject)
		{
			return std::invoke(
				HookHelper::vtbl_of<decltype(&IDCompositionDesktopDevicePartner::CreateSharedResource)>(this)[27], this, riid, ppvObject
			);
		}
		HRESULT OpenSharedResourceHandle(IUnknown* unknown, HANDLE* resourceHandle)
		{
			return std::invoke(
				HookHelper::vtbl_of<decltype(&IDCompositionDesktopDevicePartner::OpenSharedResourceHandle)>(this)[28], this, unknown, resourceHandle
			);
		}
	};

	struct IDCompositionVisualPartnerWinRTInterop : IDCompositionVisual3
	{
		auto GetVisualCollection()
		{
			using GetVisualCollection_t = HRESULT(STDMETHODCALLTYPE IDCompositionVisualPartnerWinRTInterop::*)(PVOID* collection);

			wuc::VisualCollection collection{ nullptr };

			if (os::buildNumber < os::build_w10_1903)
			{
				THROW_IF_FAILED(
					std::invoke(
						HookHelper::vtbl_of<GetVisualCollection_t>(this)[48], this, winrt::put_abi(collection)
					)
				);
			}
			else if (os::buildNumber < os::build_w10_2004)
			{
				THROW_IF_FAILED(
					std::invoke(
						HookHelper::vtbl_of<GetVisualCollection_t>(this)[50], this, winrt::put_abi(collection)
					)
				);
			}
			else if (os::buildNumber < os::build_w11_21h2)
			{
				THROW_IF_FAILED(
					std::invoke(
						HookHelper::vtbl_of<GetVisualCollection_t>(this)[44], this, winrt::put_abi(collection)
					)
				);
			}
			else
			{
				THROW_IF_FAILED(
					std::invoke(
						HookHelper::vtbl_of<GetVisualCollection_t>(this)[45], this, winrt::put_abi(collection)
					)
				);
			}

			return collection;
		}
	};
	FORCEINLINE winrt::com_ptr<IDCompositionVisualPartnerWinRTInterop> GetVisualPartnerWinRTInterop(IDCompositionVisual2* visual)
	{
		winrt::com_ptr<IDCompositionVisualPartnerWinRTInterop> visualPartnerWinRTInterop{ nullptr };

		if (os::buildNumber < os::build_w10_1903)
		{
			visual->QueryInterface(GUID{ 0xb9bc8ca1, 0xfac7, 0x4bcd, {0xb6, 0x4a, 0xd3, 0x3b, 0xbe, 0x94, 0x39, 0xbd} }, visualPartnerWinRTInterop.put_void());
		} 
		else if (os::buildNumber < os::build_w10_2004)
		{
			visual->QueryInterface(GUID{ 0x26fa1c93, 0x6db7, 0x4e13, {0x96, 0x7e, 0x55, 0x65, 0xc6, 0xc, 0x88, 0xba} }, visualPartnerWinRTInterop.put_void());
		}
		else
		{
			visual->QueryInterface(GUID{ 0xfe93b735, 0xe574, 0x4a5d, {0xa2, 0x1a, 0xf7, 0x5, 0xc2, 0x19, 0x45, 0xfa} }, visualPartnerWinRTInterop.put_void());
		}

		return visualPartnerWinRTInterop;
	}
	
	struct IVisualPartner : IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetPointerEventRouter(struct ::ABI::Windows::UI::Composition::ICompositionInteractionPartner**) PURE;
		virtual HRESULT STDMETHODCALLTYPE RemovePointerEventRouter() PURE;
		virtual HRESULT STDMETHODCALLTYPE SetTransformParent(::ABI::Windows::UI::Composition::IVisual * visual) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetWindowBackgroundTreatment(::ABI::Windows::UI::Composition::ICompositionBrush * brush) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetInteraction(struct ::ABI::Windows::UI::Composition::ICompositionInteractionPartner*) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetSharedManipulationTransform(struct ::ABI::Windows::UI::Composition::ICompositionManipulationTransformPartner*) PURE;
	};
	FORCEINLINE HRESULT SetWindowBackgroundTreatment(IUnknown* unknown, const wuc::CompositionBrush& brush)
	{
		if (os::buildNumber < os::build_w11_21h2)
		{
			// Windows 10 only, customize the content of hostbackdropbrush
			// only affect the entire tree of the applied visual
			winrt::com_ptr<IVisualPartner> visualPartner{ nullptr };
			RETURN_IF_FAILED(unknown->QueryInterface(GUID{ 0xbbed8da5, 0x977f, 0x42cb, {0x9b, 0x28, 0xf0, 0xce, 0xeb, 0xce, 0xd3, 0xa7} }, visualPartner.put_void()));
			RETURN_IF_FAILED(visualPartner->SetWindowBackgroundTreatment(brush.as<::ABI::Windows::UI::Composition::ICompositionBrush>().get()));
		}
		else
		{
			// Windows 11 only, customize the content of hostbackdropbrush
			// affect the whole window
			winrt::com_ptr<IVisualPartner> visualPartner{ nullptr };
			RETURN_IF_FAILED(unknown->QueryInterface(GUID{ 0x1dc794b, 0x4ff5, 0x4491, {0x99, 0x42, 0xb9, 0xe7, 0xb8, 0x89, 0x3b, 0xe4} }, visualPartner.put_void()));
			RETURN_IF_FAILED(visualPartner->SetWindowBackgroundTreatment(brush.as<::ABI::Windows::UI::Composition::ICompositionBrush>().get()));
		}

		return S_OK;
	}
}