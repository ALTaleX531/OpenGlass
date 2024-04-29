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

	DECLARE_INTERFACE_IID_(IDCompositionVisualPartnerWinRTInterop, IDCompositionVisual3, "fe93b735-e574-4a5d-a21a-f705c21945fa")
	{
		auto GetVisualCollection()
		{
			using GetVisualCollection_t = HRESULT(STDMETHODCALLTYPE IDCompositionVisualPartnerWinRTInterop::*)(PVOID* collection);

			wuc::VisualCollection collection{ nullptr };

			if (os::buildNumber < os::build_w11_21h2)
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

	// Windows 11 only, customize the content of hostbackdropbrush
	DECLARE_INTERFACE_IID_(IVisualPartner, IUnknown, "01dc794b-4ff5-4491-9942-b9e7b8893be4")
	{
		virtual HRESULT STDMETHODCALLTYPE GetPointerEventRouter(struct ::ABI::Windows::UI::Composition::ICompositionInteractionPartner**) PURE;
		virtual HRESULT STDMETHODCALLTYPE RemovePointerEventRouter() PURE;
		virtual HRESULT STDMETHODCALLTYPE SetTransformParent(::ABI::Windows::UI::Composition::IVisual* visual) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetWindowBackgroundTreatment(::ABI::Windows::UI::Composition::ICompositionBrush* brush) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetInteraction(struct ::ABI::Windows::UI::Composition::ICompositionInteractionPartner*) PURE;
		virtual HRESULT STDMETHODCALLTYPE SetSharedManipulationTransform(struct ::ABI::Windows::UI::Composition::ICompositionManipulationTransformPartner*) PURE;
	};
}