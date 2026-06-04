#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "ProjectionHelper.hpp"
#include "DWM.hpp"
#include "uDwmProjection.Offsets.hpp"
#include "DCompProjection.hpp"

namespace OpenGlass::uDWM
{
	using namespace DWM;
	inline const auto g_moduleHandle{ GetModuleHandleW(L"uDWM.dll") };
	inline const auto g_versionInfo{ Util::GetModuleVersionInfo(g_moduleHandle) };

	struct CBaseObject
	{
		size_t AddRef()
		{
			return InterlockedIncrement(reinterpret_cast<DWORD*>(this) + 2);
		}
		size_t Release()
		{
			auto result = InterlockedDecrement(reinterpret_cast<DWORD*>(this) + 2);
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
		DECLSPEC_DIRECT_PROJECTION IDwmChannel* GetChannel() const
		{
			return *reinterpret_cast<IDwmChannel* const*>(reinterpret_cast<ULONG_PTR>(this) + 16);
		}
		DECLSPEC_DIRECT_PROJECTION UINT GetHandleId() const
		{
			return *reinterpret_cast<UINT const*>(reinterpret_cast<ULONG_PTR>(this) + 24);
		}
	};
	struct CResource : CBaseObject 
	{
		DECLSPEC_DIRECT_PROJECTION CResourceProxy* GetProxy() const
		{
			return reinterpret_cast<CResourceProxy* const*>(this)[2];
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
		DECLSPEC_INDIRECT_PROJECTION HRESULT SetSize(double cx, double cy)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisualProxy::SetSize, this, cx, cy);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT STDMETHODCALLTYPE SetClip(CBaseGeometryProxy* geometry)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisualProxy::SetClip, this, geometry);
		}

		// dcomp-side DirectComposition::CVisualProxy* at offset +0x20
		// Created by channel->vtable[13] during CVisualProxy::CVisualProxy construction
		// Exposes IDCompositionVisualRestricted (IID 8819f277-549c-4862-8812-b114f85d1aae)
		IUnknown* GetDCompVisualProxy() const
		{
			return *reinterpret_cast<IUnknown* const*>(reinterpret_cast<ULONG_PTR>(this) + 0x20);
		}
	};

	struct CSpriteVisualProxy : CVisualProxy
	{
		// WinRT ABI::Windows::UI::Composition::ISpriteVisual* at offset +0x28
		// QI'd from GetDCompVisualProxy() during CSpriteVisualProxy::CSpriteVisualProxy construction
		// IID 08e05581-1ad1-4f97-9757-402d76e4233b
		IUnknown* GetSpriteVisualPartner() const
		{
			return *reinterpret_cast<IUnknown* const*>(reinterpret_cast<ULONG_PTR>(this) + 0x28);
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
		DECLSPEC_DIRECT_PROJECTION bool IsRTLMirrored() const
		{
			const auto propByte = Util::PointerExecuteUnsafe<CVisual_IsRTLMirrored_ByteOffset_Offsets, Util::OffsetBy<BYTE const*>>(this, g_versionInfo.build, g_versionInfo.revision);
			return (*propByte & 1) != 0;
		}
		DECLSPEC_DIRECT_PROJECTION D2D1_SIZE_F GetScale() const
		{
			return *Util::PointerExecuteUnsafe<CVisual_GetScale_Offsets, Util::OffsetBy<D2D1_SIZE_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION const SIZE& GetSize() const
		{
			return *Util::PointerExecuteUnsafe<CVisual_GetSize_Offsets, Util::OffsetBy<SIZE const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION LONG GetWidth() const
		{
			return GetSize().cx;
		}
		DECLSPEC_DIRECT_PROJECTION LONG GetHeight() const
		{
			return GetSize().cy;
		}

		DECLSPEC_DIRECT_PROJECTION const POINT& GetOffset() const
		{
			return *Util::PointerExecuteUnsafe<CVisual_GetOffset_Offsets, Util::OffsetBy<POINT const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION LONG GetX() const
		{
			return GetOffset().x;
		}
		DECLSPEC_DIRECT_PROJECTION LONG GetY() const
		{
			return GetOffset().y;
		}

		DECLSPEC_DIRECT_PROJECTION POINT GetLocalToParentVisualOffset(CVisual* parent = nullptr) const
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
		DECLSPEC_DIRECT_PROJECTION VisualCollection* GetVisualCollection()
		{
			return Util::PointerExecuteUnsafe<CVisual_GetVisualCollection_Offsets, Util::OffsetBy<VisualCollection*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CVisualProxy* GetVisualProxy() const
		{
			return *Util::PointerExecuteUnsafe<CVisual_GetVisualProxy_Offsets, Util::OffsetBy<CVisualProxy**>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CVisual* GetTransformParent() const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<CVisual_GetTransformParent_Offsets, Util::DereferenceAt<decltype(&CVisual::GetTransformParent)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this
			);
		}

		DECLSPEC_INDIRECT_PROJECTION void SetOffset(const POINT* offset)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::SetOffset, this, offset);
		}
		DECLSPEC_INDIRECT_PROJECTION void SetSize(const SIZE* size)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::SetSize, this, size);
		}
		DECLSPEC_INDIRECT_PROJECTION void SetInsetFromParent(const MARGINS& margins)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::SetInsetFromParent, this, margins);
		}
		DECLSPEC_INDIRECT_PROJECTION void SetInsetFromParentLeft(int left)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::SetInsetFromParentLeft, this, left);
		}
		DECLSPEC_INDIRECT_PROJECTION void SetDirtyFlags(int flags)
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::SetDirtyFlags, this, flags);
		}
		DECLSPEC_DIRECT_PROJECTION DWORD GetDirtyFlags() const
		{
			return *Util::PointerExecuteUnsafe<CVisual_GetDirtyFlags_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	struct CContainerVisual : CVisual
	{
		DECLSPEC_INDIRECT_PROJECTION HRESULT STDMETHODCALLTYPE InitializeVisualTreeClone(
			CVisual* clonedVisual,
			UINT cloneOption
		)
		{
			return HANDLE_PROJECTION_FUNCTION(CContainerVisual::InitializeVisualTreeClone, this, clonedVisual, cloneOption);
		}
	};
	struct CSpriteVisual : CContainerVisual
	{
		DECLSPEC_DIRECT_PROJECTION CSpriteVisualProxy* GetVisualProxy() const
		{
			return static_cast<CSpriteVisualProxy*>(CVisual::GetVisualProxy());
		}
		DECLSPEC_INDIRECT_PROJECTION static HRESULT Create(CSpriteVisual** spriteVisual)
		{
			return HANDLE_PROJECTION_FUNCTION(CSpriteVisual::Create, spriteVisual);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT GetBrush(abi::ICompositionBrush** brush) const
		{
			return HANDLE_PROJECTION_FUNCTION(CSpriteVisual::GetBrush, this, brush);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT SetBrush(abi::ICompositionBrush* brush) const
		{
			return HANDLE_PROJECTION_FUNCTION(CSpriteVisual::SetBrush, this, brush);
		}
	};
	struct CRectangleVisual : CSpriteVisual
	{
		const D2D1_RECT_F& GetRect() const
		{
			return *Util::PointerExecuteUnsafe<CRectangleVisual_GetRect_Offsets, Util::OffsetBy<D2D1_RECT_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_INDIRECT_PROJECTION HRESULT UpdateClip(uDWM::CBaseGeometryProxy* geometry)
		{
			return HANDLE_PROJECTION_FUNCTION(CRectangleVisual::UpdateClip, this, geometry);
		}
		DECLSPEC_INDIRECT_PROJECTION bool SetRect(const D2D1_RECT_F& rc)
		{
			return HANDLE_PROJECTION_FUNCTION(CRectangleVisual::SetRect, this, rc);
		}
	};
	struct CSolidRectangleVisual : CRectangleVisual
	{
		DECLSPEC_INDIRECT_PROJECTION HRESULT UpdateColor(const D2D1_COLOR_F& color)
		{
			return HANDLE_PROJECTION_FUNCTION(CSolidRectangleVisual::UpdateColor, this, color);
		}
		DECLSPEC_INDIRECT_PROJECTION static HRESULT Create(CSolidRectangleVisual** visual)
		{
			return HANDLE_PROJECTION_FUNCTION(CSolidRectangleVisual::Create, visual);
		}
	};
	struct CNineGridVisual : CRectangleVisual {};
	struct CLegacyNonClientBackground : CContainerVisual
	{
		DECLSPEC_INDIRECT_PROJECTION void ClearAll()
		{
			return HANDLE_PROJECTION_FUNCTION(CLegacyNonClientBackground::ClearAll, this);
		}
		DECLSPEC_DIRECT_PROJECTION const D2D1_RECT_F& GetBorderInnerBounds() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetBorderInnerBounds_Offsets, Util::OffsetBy<D2D1_RECT_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION const D2D1_RECT_F& GetBorderOuterBounds() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetBorderOuterBounds_Offsets, Util::OffsetBy<D2D1_RECT_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION const D2D1_COLOR_F& GetBorderColor() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetBorderColor_Offsets, Util::OffsetBy<D2D1_COLOR_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION const D2D1_COLOR_F& GetCaptionColor() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetCaptionColor_Offsets, Util::OffsetBy<D2D1_COLOR_F const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CNineGridVisual* GetBorderVisual() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetBorderVisual_Offsets, Util::OffsetBy<CNineGridVisual* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CRectangleVisual* GetCaptionVisual() const
		{
			return *Util::PointerExecuteUnsafe<CLegacyNonClientBackground_GetCaptionVisual_Offsets, Util::OffsetBy<CRectangleVisual* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};

	struct VisualCollection : CResource
	{
		DECLSPEC_INDIRECT_PROJECTION HRESULT Remove(CVisual* visual)
		{
			return HANDLE_PROJECTION_FUNCTION(VisualCollection::Remove, this, visual);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT InsertRelative(
			CVisual* visual,
			CVisual* referenceVisual,
			bool insertAfter,
			bool connectNow
		)
		{
			return HANDLE_PROJECTION_FUNCTION(
				VisualCollection::InsertRelative,
				this,
				visual,
				referenceVisual,
				insertAfter,
				connectNow
			);
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

		DECLSPEC_DIRECT_PROJECTION CDWriteText* GetDWriteText() const
		{
			return Util::PointerExecuteUnsafe<IText_GetDWriteText_NegativeOffset_Offsets, Util::OffsetBy<CDWriteText*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION bool IsRTLReading() const
		{
			return *Util::PointerExecuteUnsafe<IText_RTL_Index_Offsets, Util::OffsetBy<bool const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION bool IsReverseAlignment() const
		{
			return *Util::PointerExecuteUnsafe<IText_Reverse_Index_Offsets, Util::OffsetBy<bool const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	struct CDWriteText : CSpriteVisual
	{
		DECLSPEC_DIRECT_PROJECTION IText* GetTextInterface() const
		{
			return Util::PointerExecuteUnsafe<CDWriteText_GetTextInterface_Offsets, Util::OffsetBy<IText*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};

	struct CImage : CNineGridVisual {};
	struct CBitmapSource : CBaseObject {};
	struct CBitmapSourceArray : DynArray<CBitmapSource*> {};

	struct CPrimitive : CBaseObject {};
	struct CNineGridImagePrimitive : CPrimitive {};
	struct CThemePartPrimitive : CNineGridImagePrimitive
	{
		DECLSPEC_DIRECT_PROJECTION DWORD GetPartId() const
		{
			return *Util::PointerExecuteUnsafe<CThemePartPrimitive_GetPartId_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};

	struct CButton : CCanvasVisual
	{
		inline static PVOID* vftable{ nullptr };

		DECLSPEC_DIRECT_PROJECTION float& GetGlyphOpacity()
		{
			return *Util::PointerExecuteUnsafe<CButton_GetGlyphOpacity_Offsets, Util::OffsetBy<float*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};

	struct CWindowBorder : CContainerVisual
	{
		DECLSPEC_INDIRECT_PROJECTION HRESULT EnableBorder(bool enable)
		{
			return HANDLE_PROJECTION_FUNCTION(CWindowBorder::EnableBorder, this, enable);
		}
	};

	struct CTopLevelWindow;
	struct CWindowData : CBaseObject
	{
		DECLSPEC_DIRECT_PROJECTION HWND GetHwnd() const
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetHwnd_Offsets, Util::OffsetBy<const HWND*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CTopLevelWindow* GetWindow() const
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetWindow_Index_Offsets, Util::OffsetBy<CTopLevelWindow* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION DWM_SYSTEMBACKDROP_TYPE& GetSystemBackdropType()
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetSystemBackdropType_Index_Offsets, Util::OffsetBy<DWM_SYSTEMBACKDROP_TYPE*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION UINT GetWindowDPI() const
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetWindowDPI_Offsets, Util::OffsetBy<UINT const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION DWORD GetFrameThickness()
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetFrameThickness_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION BYTE& GetNonClientAttributeReference()
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetNonClientAttribute_Offsets, Util::OffsetBy<BYTE*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION BYTE GetNonClientAttribute() const
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetNonClientAttribute_Offsets, Util::OffsetBy<BYTE const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION BYTE GetClientBlurAttribute() const
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetClientBlurAttribute_Offsets, Util::OffsetBy<BYTE const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION bool ShouldTransitionOnMaximized() const
		{
			return GetClientBlurAttribute() & 0x8;
		}
		DECLSPEC_DIRECT_PROJECTION MARGINS& GetExtendedFrameMargins()
		{
			return *Util::PointerExecuteUnsafe<CWindowData_GetExtendedFrameMargins_Offsets, Util::OffsetBy<MARGINS*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION bool IsSheetOfGlass()
		{
			MARGINS& margins = GetExtendedFrameMargins();

			return IsSheetOfGlass(margins);
		}
		DECLSPEC_DIRECT_PROJECTION static bool IsSheetOfGlass(const MARGINS& margins)
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
		DECLSPEC_DIRECT_PROJECTION const float& GetBalance() const
		{
			return reinterpret_cast<float const*>(this)[8];
		}
		DECLSPEC_DIRECT_PROJECTION const D2D1_COLOR_F& GetColor() const
		{
			return reinterpret_cast<D2D1_COLOR_F const*>(this)[1];
		}
		DECLSPEC_DIRECT_PROJECTION D2D1_COLOR_F getArgbcolor() const
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

		DECLSPEC_DIRECT_PROJECTION CWindowData* GetData() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetData_Index_Offsets, Util::OffsetBy<CWindowData* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CImage* GetIconVisual() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetIconVisual_Index_Offsets, Util::OffsetBy<CImage* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CDWriteText* GetDWriteTextVisual() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetDWriteTextVisual_Index_Offsets, Util::OffsetBy<CDWriteText* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CLegacyNonClientBackground* GetLegacyVisual() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetLegacyVisual_Index_Offsets, Util::OffsetBy<CLegacyNonClientBackground* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CSolidRectangleVisual* GetClientBlurVisual() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetClientBlurVisual_Index_Offsets, Util::OffsetBy<CSolidRectangleVisual* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CWindowBorder* GetWindowBorder() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetWindowBorder_Index_Offsets, Util::OffsetBy<CWindowBorder* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}

		DECLSPEC_DIRECT_PROJECTION bool& GetIsBorderUpdatesSuppressed()
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetIsBorderUpdatesSuppressed_ByteIndex_Offsets, Util::OffsetBy<bool*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION bool IsWindowMaximized() const
		{
			return (*Util::PointerExecuteUnsafe<CTopLevelWindow_IsWindowMaximized_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision) & 0x4) != 0;
		}
		DECLSPEC_DIRECT_PROJECTION bool HasThinRenderedBorder() const
		{
			return (*Util::PointerExecuteUnsafe<CTopLevelWindow_StateDwordIndex_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision) & 2) != 0;
		}
		DECLSPEC_DIRECT_PROJECTION bool IsLoneButton() const
		{
			return (*Util::PointerExecuteUnsafe<CTopLevelWindow_StateDwordIndex_Offsets, Util::OffsetBy<DWORD const*>>(this, g_versionInfo.build, g_versionInfo.revision) & 0xB00) == 0;
		}
		bool TreatAsMaximized(const CWindowData* data) const;

		DECLSPEC_DIRECT_PROJECTION CGlassColorizationResources* GetCaptionColorizationParameters() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetCaptionColorizationParameters_Index_Offsets, Util::OffsetBy<CGlassColorizationResources* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CButton* GetButton(int index)
		{
			const auto base = Util::PointerExecuteUnsafe<CTopLevelWindow_GetButton_BasePointerIndex_Offsets, Util::OffsetBy<BYTE*>>(this, g_versionInfo.build, g_versionInfo.revision);
			CButton** button = reinterpret_cast<CButton**>(base + index * sizeof(void*));
			return (button && *button) ? *button : nullptr;
		}

		DECLSPEC_DIRECT_PROJECTION MARGINS& GetFrameOutsideMargins(bool zoomed)
		{
			if (zoomed)
			{
				return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetFrameOutsideMargins_Zoomed_Offsets, Util::OffsetBy<MARGINS*>>(this, g_versionInfo.build, g_versionInfo.revision);
			}
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetFrameOutsideMargins_Normal_Offsets, Util::OffsetBy<MARGINS*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION MARGINS& GetFrameInsideMargins() const
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetFrameInsideMargins_Offsets, Util::OffsetBy<MARGINS*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION MARGINS& GetBorderMargins()
		{
			return *Util::PointerExecuteUnsafe<CTopLevelWindow_GetBorderMargins_Offsets, Util::OffsetBy<MARGINS*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_INDIRECT_PROJECTION bool TreatAsActiveWindow() const
		{
			return HANDLE_PROJECTION_FUNCTION(CTopLevelWindow::TreatAsActiveWindow, this);
		}
		DECLSPEC_INDIRECT_PROJECTION void OnBlurBehindUpdated()
		{
			return HANDLE_PROJECTION_FUNCTION(CTopLevelWindow::OnBlurBehindUpdated, this);
		}
		DECLSPEC_INDIRECT_PROJECTION void OnSystemBackdropUpdated()
		{
			return HANDLE_PROJECTION_FUNCTION(CTopLevelWindow::OnSystemBackdropUpdated, this);
		}
		DECLSPEC_INDIRECT_PROJECTION RECT* GetActualWindowRect(
			RECT* rect,
			char relative,
			char respectMaximizedClip,
			bool excludeShadowMargins
		) const
		{
			return HANDLE_PROJECTION_FUNCTION(
				CTopLevelWindow::GetActualWindowRect,
				this,
				rect,
				relative,
				respectMaximizedClip,
				excludeShadowMargins
			);
		}
	};

	struct CWindowList : CBaseObject
	{
		DECLSPEC_INDIRECT_PROJECTION PRLIST_ENTRY GetWindowListForDesktop(ULONG_PTR desktopID)
		{
			return HANDLE_PROJECTION_FUNCTION(CWindowList::GetWindowListForDesktop, this, desktopID);
		}
	};

	struct CCompositor
	{
		IDCompositionDesktopDevicePartner6* GetInteropCompositorDCompDevicePartner() const
		{
			return *Util::PointerExecuteUnsafe<CCompositor_GetInteropCompositorDCompDevicePartner_Offsets, Util::OffsetBy<IDCompositionDesktopDevicePartner6* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT ForceRender()
		{
			return HANDLE_PROJECTION_FUNCTION(CCompositor::ForceRender, this);
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
		DECLSPEC_DIRECT_PROJECTION CCompositor* GetCompositor() const
		{
			return *Util::PointerExecuteUnsafe<CDesktopManager_GetCompositor_Index_Offsets, Util::OffsetBy<CCompositor* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CWindowList* GetWindowList() const
		{
			return *Util::PointerExecuteUnsafe<CDesktopManager_GetWindowList_Index_Offsets, Util::OffsetBy<CWindowList* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION IWICImagingFactory* GetWICFactory() const
		{
			return *Util::PointerExecuteUnsafe<CDesktopManager_GetWICFactory_Index_Offsets, Util::OffsetBy<IWICImagingFactory* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION ID2D1Device* GetD2DDevice() const
		{
			auto graphicsDeviceManager = *Util::PointerExecuteUnsafe<CDesktopManager_GetGraphicsDeviceManager_Index_Offsets, Util::OffsetBy<void* const*>>(this, g_versionInfo.build, g_versionInfo.revision);
			return *Util::PointerExecuteUnsafe<CGraphicsDeviceManager_GetD2DDevice_Index_Offsets, Util::OffsetBy<ID2D1Device**>>(graphicsDeviceManager, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION IDCompositionDesktopDevicePartner6* GetInteropCompositorDCompDevicePartner() const
		{
			return GetCompositor()->GetInteropCompositorDCompDevicePartner();
		}
		DECLSPEC_DIRECT_PROJECTION bool& GetIsHighContrastMode()
		{
			return *Util::PointerExecuteUnsafe<CDesktopManager_GetIsHighContrastMode_BoolIndex_Offsets, Util::OffsetBy<bool*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION bool HasMaximizedWindows() const
		{
			return Util::PointerExecuteUnsafe<CDesktopManager_HasMaximizedWindows_BoolIndex_Offsets, Util::DereferenceAt<bool>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION double GetDPIValue() const
		{
			return *Util::PointerExecuteUnsafe<CDesktopManager_GetDPIValue_Index_Offsets, Util::OffsetBy<double const*>>(this, g_versionInfo.build, g_versionInfo.revision);
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
		DECLSPEC_INDIRECT_PROJECTION HRESULT CreateGeometryFromHRGN(
			HRGN hrgn,
			CRgnGeometryProxy** geometry
		)
		{
			return HANDLE_PROJECTION_FUNCTION(ResourceHelper::CreateGeometryFromHRGN, hrgn, geometry);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT CreateCombinedGeometry(
			CBaseGeometryProxy* geometry1,
			CBaseGeometryProxy* geometry2,
			D2D1_COMBINE_MODE combineMode, // this param is always ignored in udwm implementation
			CCombinedGeometryProxy** geometry
		)
		{
			return HANDLE_PROJECTION_FUNCTION(ResourceHelper::CreateCombinedGeometry, geometry1, geometry2, combineMode, geometry);
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

	inline auto g_projectionArray = make_projection_array(
		g_versionInfo.build,

		MAKE_EMPTY_PROJECTION_TUPLE("CVisual::UpdateOffset", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CVisual::SendSetOffset", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::SetOffset, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::SetSize, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::SetInsetFromParent, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::SetInsetFromParentLeft, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::SetDirtyFlags, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisualProxy::SetSize, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisualProxy::SetClip, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(VisualCollection::Remove, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(VisualCollection::InsertRelative, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CContainerVisual::InitializeVisualTreeClone, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CRectangleVisual::SetRect, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CRectangleVisual::UpdateClip, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CSolidRectangleVisual::UpdateColor, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CSolidRectangleVisual::Create, 0, 0),

		MAKE_EMPTY_PROJECTION_TUPLE("CSpriteVisual::SendSetSize", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CSpriteVisual::CloneVisualTree", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CSpriteVisual::Create, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE_BY_ALIAS(CSpriteVisual::GetBrush, "CSpriteVisual::GetBrush<Windows::UI::Composition::ICompositionBrush>", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE_BY_ALIAS(CSpriteVisual::SetBrush, "CSpriteVisual::SetBrush<Windows::UI::Composition::ICompositionBrush * __ptr64>", 0, 0),

		MAKE_EMPTY_PROJECTION_TUPLE("CDWriteText::ValidateVisual", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CDWriteText::InitializeVisualTreeClone", 0, 0),

		MAKE_EMPTY_PROJECTION_TUPLE("CThemePartPrimitive::ShouldClone", 0, 0),

		MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(CButton::vftable, "CButton::`vftable'", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CLegacyNonClientBackground::ClearAll, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CLegacyNonClientBackground::HasSomethingToRender", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CLegacyNonClientBackground::SetCaptionRect", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CLegacyNonClientBackground::SetBorderRects", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CLegacyNonClientBackground::SetCaptionColor", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CLegacyNonClientBackground::~CLegacyNonClientBackground", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CWindowBorder::EnableBorder, 0, 0),

		MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(CTopLevelWindow::vftable, "CTopLevelWindow::`vftable'", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CTopLevelWindow::GetActualWindowRect, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CTopLevelWindow::TreatAsActiveWindow, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CTopLevelWindow::OnBlurBehindUpdated, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CTopLevelWindow::OnSystemBackdropUpdated, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::EdgeBorderMustBeOpaque", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::UpdateNCAreaBackground", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::UpdateNCAreaPositionsAndSizes", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::UpdateClientBlur", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::ValidateVisual", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::UpdateWindowVisuals", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::~CTopLevelWindow", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTopLevelWindow::IsShadowNCAreaPart", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("SetMargin", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CGlassColorizationParameters::AdjustWindowColorization", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CWindowList::GetWindowListForDesktop, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CCompositor::ForceRender, 0, 0),

		MAKE_VARIABLE_PROJECTION_TUPLE(CDesktopManager::s_pDesktopManagerInstance, 0, 0),
		MAKE_VARIABLE_PROJECTION_TUPLE(CDesktopManager::s_csDwmInstance, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CDesktopManager::IsHighContrastMode", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CGraphicsDeviceManager::ReleaseGraphicsDevice", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(ResourceHelper::CreateGeometryFromHRGN, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(ResourceHelper::CreateCombinedGeometry, 0, 0)
	);
	
	inline bool SymbolParserCallback(PSYMBOL_INFO info, [[maybe_unused]] ULONG size)
	{
		CHAR symbolName[128]{};
		UnDecorateSymbolName(info->Name, symbolName, std::size(symbolName), UNDNAME_NAME_ONLY);

		if (
			!strcmp(symbolName, "CRectangleVisual::SetRect") &&
			!strstr(info->Name, "TMilRect_")
		)
		{
			return true;
		}

		if (
			!strcmp(symbolName, "CVisual::SetSize") &&
			!strstr(info->Name, "tagSIZE@@@Z")
		)
		{
			return true;
		}

		if (
			!strcmp(symbolName, "SetMargin") &&
			!strstr(info->Name, "HHHHPEBU1")
		)
		{
			return true;
		}

		g_projectionArray.Apply(
			symbolName,
			reinterpret_cast<PVOID>(info->Address)
		);

		return !g_projectionArray.IsAllReady();
	}
}
