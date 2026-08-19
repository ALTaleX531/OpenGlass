#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "ProjectionHelper.hpp"
#include "DWM.hpp"
#include "DCompProjection.hpp"
#include "uDwmProjection.Offsets.hpp"

namespace OpenGlass::uDWM
{
	using namespace DWM;
	inline const auto g_moduleHandle{ GetModuleHandleW(L"uDWM.dll") };
	inline const auto g_versionInfo{ Util::GetModuleVersionInfo(g_moduleHandle) };

	struct CBaseObject
	{
		size_t AddRef()
		{
			return InterlockedIncrement(CBaseObject_ReferenceCount.address(this));
		}
		size_t Release()
		{
			auto result = InterlockedDecrement(CBaseObject_ReferenceCount.address(this));
			if (!result)
			{
				delete this;
			}
			return result;
		}
		HRESULT QueryInterface(
			[[maybe_unused]] REFIID riid,
			[[maybe_unused]] PVOID* ppvObject
		)
		{
			return E_NOTIMPL;
		}
		protected: virtual ~CBaseObject() {}
	};

	struct CResourceProxy : CBaseObject
	{
		inline IDwmChannel* GetChannel() const
		{
			return CResourceProxy_Channel.read(this);
		}
		inline UINT GetHandleId() const
		{
			return CResourceProxy_HandleId.read(this);
		}
	};
	struct CResource : CBaseObject
	{
		inline CResourceProxy* GetProxy() const
		{
			return CResource_Proxy.read(this);
		}
	};

	struct CBaseGeometryProxy : CResourceProxy {};

	struct CRectangleGeometryProxy : CBaseGeometryProxy {};
	struct CCombinedGeometryProxy : CBaseGeometryProxy {};
	struct CRgnGeometryProxy : CBaseGeometryProxy {};
	struct CVisualProxy;
	struct CSpriteVisualProxy;

	struct VisualCollection;
	struct CVisualProxy : CResourceProxy
	{
		inline HRESULT SetSize(double cx, double cy)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisualProxy::SetSize>(this, cx, cy);
		}
		// dcomp-side DirectComposition::CVisualProxy* at offset +0x20
		// Created by channel->vtable[13] during CVisualProxy::CVisualProxy construction
		// Exposes IDCompositionVisualRestricted (IID 8819f277-549c-4862-8812-b114f85d1aae)
		IUnknown* GetDCompVisualProxy() const
		{
			return CVisualProxy_DCompVisualProxy.read(this);
		}
	};

	struct CSpriteVisualProxy : CVisualProxy
	{
		// WinRT ABI::Windows::UI::Composition::ISpriteVisual* at offset +0x28
		// QI'd from GetDCompVisualProxy() during CSpriteVisualProxy::CSpriteVisualProxy construction
		// IID 08e05581-1ad1-4f97-9757-402d76e4233b
		IUnknown* GetSpriteVisualPartner() const
		{
			return CSpriteVisualProxy_SpriteVisualPartner.read(this);
		}
	};

	struct IVisual : CBaseObject
	{
		STDMETHOD(Initialize)() PURE;
		STDMETHOD(InitializeFromSharedHandle)(HANDLE sharedHandle) PURE;
		STDMETHOD_(void, SetDirtyFlags)(ULONG flags) PURE;
		STDMETHOD(ValidateVisual)() PURE;
	};
	struct CVisual : IVisual
	{
		inline bool IsRTLMirrored() const
		{
			const auto propByte = CVisual_IsRTLMirrored_ByteOffset.address(this);
			return (*propByte & 1) != 0;
		}
		inline D2D1_SIZE_F GetScale() const
		{
			return *CVisual_GetScale.address(this);
		}

		inline const SIZE& GetSize() const
		{
			return *CVisual_GetSize.address(this);
		}
		inline LONG GetWidth() const
		{
			return GetSize().cx;
		}
		inline LONG GetHeight() const
		{
			return GetSize().cy;
		}

		inline const POINT& GetOffset() const
		{
			return *CVisual_GetOffset.address(this);
		}
		inline POINT& GetMutableOffset()
		{
			return CVisual_GetOffset.ref(this);
		}
		inline LONG GetX() const
		{
			return GetOffset().x;
		}
		inline LONG GetY() const
		{
			return GetOffset().y;
		}

		inline POINT GetLocalToParentVisualOffset(CVisual* parent = nullptr) const
		{
			auto pt = GetOffset();
			auto current = GetTransformParent();
			while (current && current != parent)
			{
				const auto& currentPt = current->GetOffset();
				pt.x += currentPt.x;
				pt.y += currentPt.y;
				current = current->GetTransformParent();
			}
			return pt;
		}
		inline VisualCollection* GetVisualCollection()
		{
			return CVisual_GetVisualCollection.address(this);
		}
		inline CVisualProxy* GetVisualProxy() const
		{
			return *CVisual_GetVisualProxy.address(this);
		}
		inline CVisual* GetTransformParent() const
		{
			return CVisual_GetTransformParent.read(HookHelper::get_vftable_from(this))(this);
		}

		inline void SetSize(const SIZE* size)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::SetSize>(this, size);
		}
		inline void SetInsetFromParent(const MARGINS& margins)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::SetInsetFromParent>(this, margins);
		}
		inline void SetInsetFromParentLeft(int left)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::SetInsetFromParentLeft>(this, left);
		}
		inline void SetDirtyFlags(int flags)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::SetDirtyFlags>(this, flags);
		}
		inline DWORD GetDirtyFlags() const
		{
			return *CVisual_GetDirtyFlags.address(this);
		}
	};
	struct CContainerVisual : CVisual
	{
		inline HRESULT STDMETHODCALLTYPE InitializeVisualTreeClone(
			CVisual* clonedVisual,
			UINT cloneOption
		)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CContainerVisual::InitializeVisualTreeClone>(this, clonedVisual, cloneOption);
		}
	};
	struct CSpriteVisual : CContainerVisual
	{
		inline CSpriteVisualProxy* GetVisualProxy() const
		{
			return static_cast<CSpriteVisualProxy*>(CVisual::GetVisualProxy());
		}
		inline static HRESULT Create(CSpriteVisual** spriteVisual)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CSpriteVisual::Create>(spriteVisual);
		}
		inline HRESULT GetBrush(abi::ICompositionBrush** brush) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CSpriteVisual::GetBrush>(this, brush);
		}
		inline HRESULT SetBrush(abi::ICompositionBrush* brush) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CSpriteVisual::SetBrush>(this, brush);
		}
	};
	struct CRectangleVisual : CSpriteVisual
	{
		const D2D1_RECT_F& GetRect() const
		{
			return *CRectangleVisual_GetRect.address(this);
		}

		inline HRESULT UpdateClip(uDWM::CBaseGeometryProxy* geometry)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CRectangleVisual::UpdateClip>(this, geometry);
		}
		inline void SetRect(const D2D1_RECT_F& rc)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CRectangleVisual::SetRect>(this, rc);
		}
	};
	struct CSolidRectangleVisual : CRectangleVisual
	{
		inline HRESULT UpdateColor(const D2D1_COLOR_F& color)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CSolidRectangleVisual::UpdateColor>(this, color);
		}
		inline static HRESULT Create(CSolidRectangleVisual** visual)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CSolidRectangleVisual::Create>(visual);
		}
	};
	struct CNineGridVisual : CRectangleVisual {};
	struct CLegacyNonClientBackground : CContainerVisual
	{
		inline void ClearAll()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CLegacyNonClientBackground::ClearAll>(this);
		}
		inline const D2D1_RECT_F& GetBorderInnerBounds() const
		{
			return *CLegacyNonClientBackground_GetBorderInnerBounds.address(this);
		}
		inline const D2D1_RECT_F& GetBorderOuterBounds() const
		{
			return *CLegacyNonClientBackground_GetBorderOuterBounds.address(this);
		}
		inline const D2D1_COLOR_F& GetBorderColor() const
		{
			return *CLegacyNonClientBackground_GetBorderColor.address(this);
		}
		inline const D2D1_COLOR_F& GetCaptionColor() const
		{
			return *CLegacyNonClientBackground_GetCaptionColor.address(this);
		}
		inline CNineGridVisual* GetBorderVisual() const
		{
			return *CLegacyNonClientBackground_GetBorderVisual.address(this);
		}
		inline CRectangleVisual* GetCaptionVisual() const
		{
			return *CLegacyNonClientBackground_GetCaptionVisual.address(this);
		}
	};

	struct VisualCollection : CResource
	{
		inline HRESULT Remove(CVisual* visual)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&VisualCollection::Remove>(this, visual);
		}
		inline HRESULT InsertRelative(
			CVisual* visual,
			CVisual* referenceVisual,
			bool insertAfter,
			bool connectNow
		)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&VisualCollection::InsertRelative>(this, visual, referenceVisual, insertAfter, connectNow);
		}
	};

	struct CCanvasVisual : CContainerVisual {};
	struct CDWriteText;
	struct IText
	{
		STDMETHODV_(void, SetColor)(COLORREF) PURE;
		STDMETHODV_(void, SetFont)(const LOGFONTW*) PURE;
		STDMETHODV_(void, SetScalingFactor)(double) PURE;
		STDMETHODV_(void, SetRTLReading)(bool) PURE;
		STDMETHODV_(void, SetBackgroundColor)(ULONG) PURE;
		STDMETHODV_(void, SetReverseAlignment)(bool) PURE;
		STDMETHODV_(ULONG_PTR, SetText)(LPCWSTR) PURE;

		inline CDWriteText* GetDWriteText() const
		{
			return IText_GetDWriteText_NegativeOffset.mutable_address(this);
		}

		inline bool IsRTLReading() const
		{
			return *IText_RTL_Index.address(this);
		}
		inline bool IsReverseAlignment() const
		{
			return *IText_Reverse_Index.address(this);
		}
	};
	struct CDWriteText : CSpriteVisual
	{
		inline IText* GetTextInterface() const
		{
			return CDWriteText_GetTextInterface.mutable_address(this);
		}
	};

	struct CImage : CNineGridVisual {};
	struct CBitmapSource : CBaseObject {};
	struct CBitmapSourceArray : DynArray<CBitmapSource*> {};

	struct CPrimitive : CBaseObject {};
	struct CNineGridImagePrimitive : CPrimitive {};
	struct CThemePartPrimitive : CNineGridImagePrimitive
	{
		inline DWORD GetPartId() const
		{
			return *CThemePartPrimitive_GetPartId.address(this);
		}
	};

	struct CButton : CCanvasVisual
	{
		inline static PVOID* vftable{ nullptr };

		inline float& GetGlyphOpacity()
		{
			return *CButton_GetGlyphOpacity.address(this);
		}
	};

	struct CWindowBorder : CContainerVisual
	{
		inline HRESULT EnableBorder(bool enable)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CWindowBorder::EnableBorder>(this, enable);
		}
	};

	struct CTopLevelWindow;
	struct CWindowData : CBaseObject
	{
		inline HWND GetHwnd() const
		{
			return *CWindowData_GetHwnd.address(this);
		}
		inline CTopLevelWindow* GetWindow() const
		{
			return *CWindowData_GetWindow_Index.address(this);
		}

		inline DWM_SYSTEMBACKDROP_TYPE& GetSystemBackdropType()
		{
			return *CWindowData_GetSystemBackdropType_Index.address(this);
		}

		inline UINT GetWindowDPI() const
		{
			return *CWindowData_GetWindowDPI.address(this);
		}

		inline DWORD GetFrameThickness()
		{
			return *CWindowData_GetFrameThickness.address(this);
		}

		inline BYTE& GetNonClientAttributeReference()
		{
			return *CWindowData_GetNonClientAttribute.address(this);
		}
		inline BYTE GetNonClientAttribute() const
		{
			return *CWindowData_GetNonClientAttribute.address(this);
		}
		inline BYTE GetClientBlurAttribute() const
		{
			return *CWindowData_GetClientBlurAttribute.address(this);
		}
		inline bool ShouldTransitionOnMaximized() const
		{
			return GetClientBlurAttribute() & 0x8;
		}
		inline MARGINS& GetExtendedFrameMargins()
		{
			return *CWindowData_GetExtendedFrameMargins.address(this);
		}
		inline bool IsSheetOfGlass()
		{
			MARGINS& margins = GetExtendedFrameMargins();

			return IsSheetOfGlass(margins);
		}
		inline static bool IsSheetOfGlass(const MARGINS& margins)
		{
			return
			margins.cxLeftWidth == 0x7FFFFFFF ||
			margins.cxRightWidth == 0x7FFFFFFF ||
			margins.cyTopHeight == 0x7FFFFFFF ||
			margins.cyBottomHeight == 0x7FFFFFFF;
		}
	};

	union GpCC
	{
		struct
		#pragma warning(suppress : 4201)
		{
			BYTE b;
			BYTE g;
			BYTE r;
			BYTE a;
		};
		UINT32 argb;
	};
	struct CGlassColorizationParameters
	{
		UINT32 color;
		UINT32 afterglow;
		UINT32 colorBalance;
		UINT32 afterglowBalance;
		UINT32 blurBalance;
		BOOL windowColorization;
		UINT32 glassAttribute;
		UINT32 reserved;
	};
	struct CGlassColorizationResources
	{
		inline const float& GetBalance() const
		{
			return CGlassColorizationResources_Balance.ref(this);
		}
		inline const D2D1_COLOR_F& GetColor() const
		{
			return CGlassColorizationResources_Color.ref(this);
		}
		inline D2D1_COLOR_F getArgbcolor() const
		{
			const auto& balance = GetBalance();
			const auto& color = GetColor();
			return D2D1::ColorF(
				color.r * balance,
				color.g * balance,
				color.b * balance,
				color.a
			);
		}
	};
	struct CTopLevelWindow : CContainerVisual
	{
		inline static PVOID* vftable{ nullptr };

		inline CWindowData* GetData() const
		{
			return *CTopLevelWindow_GetData_Index.address(this);
		}
		inline CImage* GetIconVisual() const
		{
			return *CTopLevelWindow_GetIconVisual_Index.address(this);
		}
		inline CDWriteText* GetDWriteTextVisual() const
		{
			return *CTopLevelWindow_GetDWriteTextVisual_Index.address(this);
		}
		inline CLegacyNonClientBackground* GetLegacyVisual() const
		{
			return *CTopLevelWindow_GetLegacyVisual_Index.address(this);
		}
		inline CSolidRectangleVisual* GetClientBlurVisual() const
		{
			return *CTopLevelWindow_GetClientBlurVisual_Index.address(this);
		}
		inline CWindowBorder* GetWindowBorder() const
		{
			return *CTopLevelWindow_GetWindowBorder_Index.address(this);
		}

		inline bool& GetIsBorderUpdatesSuppressed()
		{
			return *CTopLevelWindow_GetIsBorderUpdatesSuppressed_ByteIndex.address(this);
		}
		inline bool IsWindowMaximized() const
		{
			return (*CTopLevelWindow_IsWindowMaximized.address(this) & 0x4) != 0;
		}
		inline bool HasThinRenderedBorder() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 2) != 0;
		}
		inline bool IsLoneButton() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 0xB00) == 0;
		}
		bool TreatAsMaximized(const CWindowData* data) const;

		inline CGlassColorizationResources* GetCaptionColorizationParameters() const
		{
			return *CTopLevelWindow_GetCaptionColorizationParameters_Index.address(this);
		}
		inline CButton* GetButton(int index)
		{
			const auto base = CTopLevelWindow_GetButton_BasePointerIndex.address(this);
			CButton** button = reinterpret_cast<CButton**>(base + index * sizeof(void*));
			return (button && *button) ? *button : nullptr;
		}

		inline MARGINS& GetFrameOutsideMargins(bool zoomed)
		{
			if (zoomed)
			{
				return *CTopLevelWindow_GetFrameOutsideMargins_Zoomed.address(this);
			}
			return *CTopLevelWindow_GetFrameOutsideMargins_Normal.address(this);
		}
		inline MARGINS& GetFrameInsideMargins() const
		{
			return CTopLevelWindow_GetFrameInsideMargins.mutable_ref(this);
		}
		inline MARGINS& GetBorderMargins()
		{
			return *CTopLevelWindow_GetBorderMargins.address(this);
		}
		inline bool TreatAsActiveWindow() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CTopLevelWindow::TreatAsActiveWindow>(this);
		}
		inline void OnBlurBehindUpdated()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CTopLevelWindow::OnBlurBehindUpdated>(this);
		}
		inline void OnSystemBackdropUpdated()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CTopLevelWindow::OnSystemBackdropUpdated>(this);
		}
		inline RECT* GetActualWindowRect(
			RECT* rect,
			char relative,
			char respectMaximizedClip,
			bool excludeShadowMargins
		) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CTopLevelWindow::GetActualWindowRect>(this, rect, relative, respectMaximizedClip, excludeShadowMargins);
		}
	};

	struct CWindowList : CBaseObject
	{
		inline PRLIST_ENTRY GetWindowListForDesktop(ULONG_PTR desktopID)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CWindowList::GetWindowListForDesktop>(this, desktopID);
		}
	};

	struct CCompositor
	{
		IDCompositionDesktopDevicePartner6* GetInteropCompositorDCompDevicePartner() const
		{
			return *CCompositor_GetInteropCompositorDCompDevicePartner.address(this);
		}
		inline HRESULT ForceRender()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CCompositor::ForceRender>(this);
		}
	};
	struct CDesktopManager
	{
		inline static CDesktopManager** s_pDesktopManagerInstance{ nullptr };
		inline static LPCRITICAL_SECTION s_csDwmInstance{ nullptr };

		static CDesktopManager* GetInstance()
		{
			return *s_pDesktopManagerInstance;
		}
		inline CCompositor* GetCompositor() const
		{
			return *CDesktopManager_GetCompositor_Index.address(this);
		}
		inline CWindowList* GetWindowList() const
		{
			return *CDesktopManager_GetWindowList_Index.address(this);
		}
		inline IWICImagingFactory* GetWICFactory() const
		{
			return *CDesktopManager_GetWICFactory_Index.address(this);
		}
		inline ID2D1Device* GetD2DDevice() const
		{
			auto graphicsDeviceManager = *CDesktopManager_GetGraphicsDeviceManager_Index.address(this);
			return *CGraphicsDeviceManager_GetD2DDevice_Index.address(graphicsDeviceManager);
		}
		inline IDCompositionDesktopDevicePartner6* GetInteropCompositorDCompDevicePartner() const
		{
			return GetCompositor()->GetInteropCompositorDCompDevicePartner();
		}
		inline bool& GetIsHighContrastMode()
		{
			return *CDesktopManager_GetIsHighContrastMode_BoolIndex.address(this);
		}
		inline bool HasMaximizedWindows() const
		{
			return CDesktopManager_HasMaximizedWindows_BoolIndex.read(this);
		}
		inline double GetDPIValue() const
		{
			return *CDesktopManager_GetDPIValue_Index.address(this);
		}
	};
	inline bool CTopLevelWindow::TreatAsMaximized(const CWindowData* data = nullptr) const
	{
		if (!data)
		{
			data = GetData();
		}

		return
		IsWindowMaximized() ||
		(
			data &&
			data->ShouldTransitionOnMaximized() &&
			CDesktopManager::GetInstance()->HasMaximizedWindows()
		);
	}
	namespace ResourceHelper
	{
		inline HRESULT CreateGeometryFromHRGN(
			HRGN hrgn,
			CRgnGeometryProxy** geometry
		)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&ResourceHelper::CreateGeometryFromHRGN>(hrgn, geometry);
		}
		inline HRESULT CreateCombinedGeometry(
			CBaseGeometryProxy* geometry1,
			CBaseGeometryProxy* geometry2,
			D2D1_COMBINE_MODE combineMode, // this param is always ignored in udwm implementation
			CCombinedGeometryProxy** geometry
		)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&ResourceHelper::CreateCombinedGeometry>(geometry1, geometry2, combineMode, geometry);
		}
	}

	inline uDWM::CTopLevelWindow* TryGetWindowFromVisual(uDWM::CVisual* visual)
	{
		auto current = visual->GetTransformParent();

		while (current && HookHelper::get_vftable_from(current) != uDWM::CTopLevelWindow::vftable)
		{
			current = current->GetTransformParent();
		}

		return static_cast<uDWM::CTopLevelWindow*>(current);
	}

	using CTopLevelWindow_EdgeBorderMustBeOpaque_t = int(*)(CTopLevelWindow*);
	using CGlassColorizationParameters_AdjustWindowColorization_t = void(*)(
		CGlassColorizationParameters*,
		const GpCC*,
		float,
		UINT
	);
	struct CGraphicsDeviceManager;
	using CVisual_UpdateOffset_t = HRESULT(*)(CVisual*);
	using CVisual_SendSetOffset_t = HRESULT(*)(CVisual*, const POINT&);
	using CSpriteVisual_SendSetSize_t = HRESULT(*)(CSpriteVisual*, const SIZE&);
	using CSpriteVisual_CloneVisualTree_t = HRESULT(*)(CSpriteVisual*, CVisual**, UINT);
	using CDWriteText_ValidateVisual_t = HRESULT(*)(CDWriteText*);
	using CDWriteText_InitializeVisualTreeClone_t = HRESULT(*)(CDWriteText*, CDWriteText*, UINT);
	using CThemePartPrimitive_ShouldClone_t = bool(*)(CThemePartPrimitive*, BYTE);
	using CLegacyNonClientBackground_HasSomethingToRender_t = bool(*)(CLegacyNonClientBackground*);
	using CLegacyNonClientBackground_SetCaptionRect_t = HRESULT(*)(CLegacyNonClientBackground*, LPCRECT);
	using CLegacyNonClientBackground_SetBorderRects_t = HRESULT(*)(CLegacyNonClientBackground*, LPCRECT, LPCRECT);
	using CLegacyNonClientBackground_SetCaptionColor_t = HRESULT(*)(CLegacyNonClientBackground*, const D2D1_COLOR_F&);
	using CLegacyNonClientBackground_Destructor_t = void(*)(CLegacyNonClientBackground*);
	using CTopLevelWindow_UpdateNCAreaBackground_t = HRESULT(*)(CTopLevelWindow*);
	using CTopLevelWindow_UpdateNCAreaPositionsAndSizes_t = HRESULT(*)(CTopLevelWindow*);
	using CTopLevelWindow_UpdateClientBlur_t = HRESULT(*)(CTopLevelWindow*);
	using CTopLevelWindow_ValidateVisual_t = HRESULT(*)(CTopLevelWindow*);
	using CTopLevelWindow_UpdateWindowVisuals_t = HRESULT(*)(CTopLevelWindow*);
	using CTopLevelWindow_Destructor_t = void(*)(CTopLevelWindow*);
	using CTopLevelWindow_IsShadowNCAreaPart_t = bool(*)(UINT);
	using SetMargin_t = bool(WINAPI*)(MARGINS*, int, int, int, int, const MARGINS*);
	using CDesktopManager_IsHighContrastMode_t = bool(*)();
	using CGraphicsDeviceManager_ReleaseGraphicsDevice_t = HRESULT(*)(CGraphicsDeviceManager*);

} // namespace OpenGlass::uDWM

#include "udwm.Symbols.generated.hpp"
