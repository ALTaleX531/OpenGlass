#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "ProjectionHelper.hpp"
#include "DWM.hpp"
#include "DCompPrivates.hpp"
#include "uDwmProjection.Offsets.hpp"

namespace OpenGlass::uDWM
{
	using namespace DWM;
	inline const auto g_moduleHandle{GetModuleHandleW(L"uDWM.dll")};
	inline const auto g_versionInfo{Util::GetModuleVersionInfo(g_moduleHandle)};

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
		HRESULT QueryInterface([[maybe_unused]] REFIID riid, [[maybe_unused]] PVOID* ppvObject)
		{
			return E_NOTIMPL;
		}

	  protected:
		virtual ~CBaseObject() {}
	};

	struct CResourceProxy
	{
		IDwmChannel* GetChannel() const
		{
			return CResourceProxy_Channel.read(this);
		}
		UINT GetHandleId() const
		{
			return CResourceProxy_HandleId.read(this);
		}
	};
	struct CResource : CBaseObject
	{
		CResourceProxy* GetProxy() const
		{
			return CResource_Proxy.read(this);
		}
		inline static HRESULT Create(DwmResourceType type, IDwmChannel* channel, CResource** resource)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CResource::Create>(type, channel, resource);
		}
	};

	struct CBaseLegacyMilBrushProxy : CResource
	{
	};
	struct CBaseGeometryProxy : CResource
	{
	};
	struct CBaseTransformProxy : CResource
	{
	};
	struct CBaseResourceProxy : CResource
	{
	};
	struct CBaseImageProxy : CResource
	{
	};

	struct CRectResourceProxy : CBaseResourceProxy
	{
	};
	struct CSizeResourceProxy : CBaseResourceProxy
	{
	};
	struct CDoubleResourceProxy : CBaseResourceProxy
	{
	};
	struct CRectangleGeometryProxy : CBaseGeometryProxy
	{
	};
	struct CCombinedGeometryProxy : CBaseGeometryProxy
	{
	};
	struct CRgnGeometryProxy : CBaseGeometryProxy
	{
	};
	struct CSolidColorLegacyMilBrushProxy : CBaseLegacyMilBrushProxy
	{
		inline HRESULT Update(double opacity, const D2D1_COLOR_F& color)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CSolidColorLegacyMilBrushProxy::Update>(this, opacity, color);
		}
	};
	struct CImageLegacyMilBrushProxy : CBaseLegacyMilBrushProxy
	{
		inline HRESULT Update(
			double opacity,
			[[maybe_unused]] const D2D1_RECT_F& viewport, // this parameter is will always be ignored, the bounding box
														  // of the geometry will be used instead
			const D2D1_RECT_F& viewbox, const CDoubleResourceProxy* opacityAnimation, MilBrushMappingMode viewportUnits,
			MilBrushMappingMode viewboxUnits, const CRectResourceProxy* viewportAnimations,
			const CRectResourceProxy* viewboxAnimations, MilStretch stretchMode, MilTileMode tileMode,
			MilHorizontalAlignment alignmentX, MilVerticalAlignment alignmentY, const CBaseImageProxy* imageSource)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CImageLegacyMilBrushProxy::Update>(
				this, opacity, viewport, viewbox, opacityAnimation, viewportUnits, viewboxUnits, viewportAnimations,
				viewboxAnimations, stretchMode, tileMode, alignmentX, alignmentY, imageSource);
		}
	};

	inline HRESULT Fallback_CSolidColorLegacyMilBrushProxy_Update(
		CSolidColorLegacyMilBrushProxy* This, double opacity, const D2D1_COLOR_F& color
	)
	{
		return static_cast<dwmcore::CChannel*>(This->GetProxy()->GetChannel())
			->SolidColorLegacyMilBrushUpdate(This->GetProxy()->GetHandleId(), opacity, color, 0, 0, 0);
	}

	inline HRESULT Fallback_CImageLegacyMilBrushProxy_Update(
		CImageLegacyMilBrushProxy* This, double opacity, const D2D1_RECT_F& viewport, const D2D1_RECT_F& viewbox,
		const CDoubleResourceProxy* opacityAnimation, MilBrushMappingMode viewportUnits,
		MilBrushMappingMode viewboxUnits, const CRectResourceProxy* viewportAnimations,
		const CRectResourceProxy* viewboxAnimations, MilStretch stretchMode, MilTileMode tileMode,
		MilHorizontalAlignment alignmentX, MilVerticalAlignment alignmentY, const CBaseImageProxy* imageSource
	)
	{
		return static_cast<dwmcore::CChannel*>(This->GetProxy()->GetChannel())
			->ImageLegacyMilBrushUpdate(
				This->GetProxy()->GetHandleId(), opacity, viewport, viewbox,
				opacityAnimation ? opacityAnimation->GetProxy()->GetHandleId() : 0, 0, 0, viewportUnits,
				viewboxUnits, viewportAnimations ? viewportAnimations->GetProxy()->GetHandleId() : 0,
				viewboxAnimations ? viewboxAnimations->GetProxy()->GetHandleId() : 0, stretchMode, tileMode,
				alignmentX, alignmentY, imageSource ? imageSource->GetProxy()->GetHandleId() : 0);
	}
	struct CVisualProxy;
	struct CCachedVisualImageProxy : CBaseImageProxy
	{
	};

	struct VisualCollection;
	struct CVisualProxy : CResource
	{
		inline HRESULT SetSize(double cx, double cy)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisualProxy::SetSize>(this, cx, cy);
		}
	};

	struct IVisual : CBaseObject
	{
		STDMETHOD(Initialize)() PURE;
		STDMETHOD(InitializeFromSharedHandle)(HANDLE sharedHandle) PURE;
		STDMETHOD_(void, SetDirtyFlags)(ULONG flags) PURE;
	};
	struct CVisual : CBaseObject
	{
		inline bool IsRTLMirrored() const
		{
			const auto propByte = CVisual_IsRTLMirrored_ByteOffset.address(this);
			return (*propByte & 1) != 0;
		}
		inline MilSizeD GetScale() const
		{
			if (g_versionInfo.build < os::build_w11_24h2)
			{
				return CVisual_GetScale_MilSizeD.read(this);
			}
			const auto scaleF = CVisual_GetScale_D2D1_SIZE_F.address(this);
			return {static_cast<double>(scaleF->width), static_cast<double>(scaleF->height)};
		}

		inline const SIZE& GetSize() const
		{
			return *CVisual_GetSize.address(this);
		}
		LONG GetWidth() const
		{
			return GetSize().cx;
		}
		LONG GetHeight() const
		{
			return GetSize().cy;
		}

		inline const POINT& GetOffset() const
		{
			return *CVisual_GetOffset.address(this);
		}
		LONG GetX() const
		{
			return GetOffset().x;
		}
		LONG GetY() const
		{
			return GetOffset().y;
		}
		POINT GetLocalToParentVisualOffset(CVisual* parent = nullptr) const
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
		inline void SetExcludeSubtree(bool exclude)
		{
			auto properties = CVisual_SetExcludeSubtree_ByteOffset.address(this);
			if (exclude)
			{
				*properties |= 8;
			}
			else
			{
				*properties &= ~8;
			}
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
			return std::invoke(CVisual_GetTransformParent.read(HookHelper::get_vftable_from(this)), this);
		}

		inline void SetSize(const SIZE* size)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::SetSize>(this, size);
		}
		inline void SetInsetFromParent(const MARGINS& margins)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::SetInsetFromParent>(this, margins);
		}
		inline void SetInsetFromParentLeft(int left)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::SetInsetFromParentLeft>(this, left);
		}
		inline void SetDirtyFlags(int flags)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::SetDirtyFlags>(this, flags);
		}
		inline HRESULT MoveToFront(bool moveChildren)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::MoveToFront>(this, moveChildren);
		}
		inline HRESULT RenderRecursive()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::RenderRecursive>(this);
		}
		inline DWORD GetDirtyFlags() const
		{
			return *CVisual_GetDirtyFlags.address(this);
		}
		inline HRESULT _ValidateVisual()
		{
			return std::invoke(CVisual__ValidateVisual.read(HookHelper::get_vftable_from(this)), this);
		}
	};

	struct VisualCollection : CResource
	{
		inline HRESULT Remove(CVisual* visual)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&VisualCollection::Remove>(this, visual);
		}
		inline HRESULT InsertRelative(CVisual* visual, CVisual* referenceVisual, bool insertAfter, bool connectNow)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&VisualCollection::InsertRelative>(this, visual, referenceVisual, insertAfter,
																		 connectNow);
		}
	};

	struct IRenderDataBuilder : IUnknown
	{
		STDMETHOD(DrawBitmap)(UINT bitmapHandleId) PURE;
		STDMETHOD(DrawGeometry)(UINT geometryHandleId, UINT brushHandleId) PURE;
		STDMETHOD(DrawImage)(const D2D1_RECT_F& rect, UINT imageHandleId) PURE;
		STDMETHOD(DrawMesh2D)(UINT meshHandleId, UINT brushHandleId) PURE;
		STDMETHOD(DrawRectangle)(const D2D1_RECT_F* rect, UINT brushHandleId) PURE;
		STDMETHOD(DrawTileImage)(UINT imageHandleId, const D2D1_RECT_F& rect, float opacity,
								 const D2D1_POINT_2F& point) PURE;
		STDMETHOD(DrawVisual)(UINT visualHandleId) PURE;
		STDMETHOD(Pop)() PURE;
		STDMETHOD(PushTransform)(UINT transformHandleId) PURE;
		STDMETHOD(DrawSolidRectangle)(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) PURE;
	};
	struct CRenderDataInstruction : CResource
	{
		STDMETHOD(WriteInstruction)(IRenderDataBuilder* builder, const struct CVisual* visual) PURE;
	};
	struct CDrawGeometryInstruction : CRenderDataInstruction
	{
		inline static HRESULT Create(CBaseLegacyMilBrushProxy* brush, CBaseGeometryProxy* geometry,
									 CDrawGeometryInstruction** instruction)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawGeometryInstruction::Create>(brush, geometry, instruction);
		}

		inline CBaseLegacyMilBrushProxy*& GetBrush()
		{
			return *CDrawGeometryInstruction_GetBrush.address(this);
		}
		inline CBaseGeometryProxy*& GetGeometry()
		{
			return *CDrawGeometryInstruction_GetGeometry.address(this);
		}
	};
	struct CRectangleInstruction : CRenderDataInstruction
	{
	};
	struct CSolidRectangleInstruction : CRenderDataInstruction
	{
		DWORD m_refCount{1};
		DWORD m_padding{0};
		D2D1_COLOR_F m_color{};
		D2D1_RECT_F m_drawRect{};

	  public:
		STDMETHOD(WriteInstruction)(IRenderDataBuilder* builder, [[maybe_unused]] const CVisual* visual) override
		{
			return builder->DrawSolidRectangle(m_drawRect, m_color);
		}
		D2D1_COLOR_F& GetColor()
		{
			return m_color;
		}
		D2D1_RECT_F& GetRectangle()
		{
			return m_drawRect;
		}
	};
	class CDrawVisualTreeInstruction : public CRenderDataInstruction
	{
	};

	struct CRenderDataVisual : CVisual
	{
		inline DynArray<CRenderDataInstruction*>& GetInstructions() const
		{
			return CRenderDataVisual_GetInstructions.mutable_ref(this);
		}
		DWORD GetCount() const
		{
			return GetInstructions().count;
		}
		FORCEINLINE bool IsEmpty() const
		{
			return GetCount() == 0;
		}

		inline HRESULT AddInstruction(CRenderDataInstruction* instruction)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRenderDataVisual::AddInstruction>(this, instruction);
		}
		inline HRESULT ClearInstructions()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRenderDataVisual::ClearInstructions>(this);
		}
		inline HRESULT UpdateRenderData()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRenderDataVisual::UpdateRenderData>(this);
		}
		inline static HRESULT Create_At_Least_W10_1903(CRenderDataVisual** visual)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRenderDataVisual::Create_At_Least_W10_1903>(visual);
		}
		inline static HRESULT Create_Pre_W10_1903(IDwmChannel* channel, CRenderDataVisual** visual)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRenderDataVisual::Create_Pre_W10_1903>(channel, visual);
		}
		static HRESULT Create(CRenderDataVisual** visual);
	};
	struct CCanvasVisual : CRenderDataVisual
	{
	};
	struct CText : CRenderDataVisual
	{
		inline bool IsRTLReading() const
		{
			const auto p = CText_RTL_FlagByteOffset.address(this);
			return (*p & 2) != 0;
		}
		inline bool IsReverseAlignment() const
		{
			const auto p = CText_RTL_FlagByteOffset.address(this);
			return (*p & 4) != 0;
		}
	};
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
	struct CDWriteText : CVisual
	{
		inline IText* GetTextInterface() const
		{
			return CDWriteText_GetTextInterface.mutable_address(this);
		}
	};

	struct CImage : CVisual
	{
	};
	struct CBitmapSource : CBaseObject
	{
		inline MARGINS& GetNineGridMargins()
		{
			return CBitmapSource_GetNineGridMargins.ref(this);
		}
		inline const MARGINS& GetNineGridMargins() const
		{
			return CBitmapSource_GetNineGridMargins.ref(this);
		}
	};
	struct CBitmapSourceArray : DynArray<CBitmapSource*>
	{
	};

	struct AtlasedRects;
	struct CAtlasedImage;
	struct CAtlasedRectsVisual : CVisual
	{
		inline DWORD GetAtlasImageCount() const
		{
			return CAtlasedRectsVisual_GetAtlasImageCount.read(this);
		}
		inline HRESULT InitializeVisualTreeClone(CAtlasedRectsVisual* clonedVisual, UINT cloneOption)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedRectsVisual::InitializeVisualTreeClone>(this, clonedVisual, cloneOption);
		}
		inline void RemoveAtlasImage(CAtlasedImage* image)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedRectsVisual::RemoveAtlasImage>(this, image);
		}
		inline HRESULT AddAtlasImage(CAtlasedImage* image)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedRectsVisual::AddAtlasImage>(this, image);
		}
		inline HRESULT InsertAtlasImageAtIndex(CAtlasedImage* image, UINT index)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedRectsVisual::InsertAtlasImageAtIndex>(this, image, index);
		}
	};
	struct CTopLevelAtlasedRectsVisual : CAtlasedRectsVisual
	{
	};
	struct CAtlasedImage : CBaseObject
	{
		inline HRESULT AppendAtlasNineGrid(AtlasedRects& rects, CBitmapSource* bitmapSource)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedImage::AppendAtlasNineGrid>(this, rects, bitmapSource);
		}
		inline void AddNineGridAtlasSize(const MARGINS& margins, UINT* size)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedImage::AddNineGridAtlasSize>(this, margins, size);
		}
		inline void SetDirtyFlags(ULONG flags, ULONG mask)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasedImage::SetDirtyFlags>(this, flags, mask);
		}
		inline CAtlasedRectsVisual* GetParent() const
		{
			return CAtlasedImage_GetParent.read(this);
		}
		inline DWORD& GetSize()
		{
			return CAtlasedImage_GetSize.ref(this);
		}
		inline DWORD GetPartId() const
		{
			return *CAtlasedImage_GetPartId.address(this);
		}
	};
	struct CAtlasButton : CAtlasedImage
	{
		inline HRESULT AppendAtlas(AtlasedRects& rects)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasButton::AppendAtlas>(this, rects);
		}
		inline void AddApproximateAtlasSize(UINT* size)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CAtlasButton::AddApproximateAtlasSize>(this, size);
		}
	};

	struct CTimeline
	{
	};
	struct CButton : CAtlasedRectsVisual
	{
		enum class ButtonStates : UINT;

		inline static PVOID* vftable{nullptr};

		inline static HRESULT Create(CButton** visual)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::Create>(visual);
		}
		inline HRESULT SetVisualStates_Win10(uDWM::CBitmapSourceArray* buttonArray,
											 uDWM::CBitmapSourceArray* glyphArray, uDWM::CBitmapSource* glowBitmap,
											 float opacity)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::SetVisualStates_Win10>(this, buttonArray, glyphArray, glowBitmap,
																	   opacity);
		}
		inline HRESULT SetVisualStates_Win11(uDWM::CBitmapSourceArray* buttonArray,
											 uDWM::CBitmapSourceArray* glyphArray, float opacity)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::SetVisualStates_Win11>(this, buttonArray, glyphArray, opacity);
		}
		HRESULT SetVisualStates(uDWM::CBitmapSourceArray* buttonArray, uDWM::CBitmapSourceArray* glyphArray,
								float opacity)
		{
			if (g_versionInfo.build < os::build_w11_21h2) [[likely]]
			{
				return SetVisualStates_Win10(buttonArray, glyphArray, nullptr, opacity);
			}
			else
			{
				return SetVisualStates_Win11(buttonArray, glyphArray, opacity);
			}
		}

		inline float& GetGlyphOpacity()
		{
			return *CButton_GetGlyphOpacity.address(this);
		}
		inline BYTE* GetButtonState()
		{
			return CButton_GetButtonState.address(this);
		}
		inline DWORD* GetVisualState()
		{
			return CButton_GetVisualState.address(this);
		}
		inline CAtlasButton** GetFirstAtlasImage()
		{
			return CButton_GetFirstAtlasImage.address(this);
		}
		inline CAtlasButton** GetSecondAtlasImage()
		{
			return CButton_GetSecondAtlasImage.address(this);
		}
		inline CTimeline* GetTimeline()
		{
			return *CButton_GetTimeline.address(this);
		}
		inline CBitmapSourceArray* GetGlyphBitmapArray()
		{
			return CButton_GetGlyphBitmapArray.address(this);
		}
		inline CBitmapSourceArray* GetButtonBitmapArray()
		{
			return CButton_GetButtonBitmapArray.address(this);
		}
		inline void UpdateCrossfade()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::UpdateCrossfade>(this);
		}
		inline void DrawStateW(CAtlasButton* atlasButton, ButtonStates state)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::DrawStateW>(this, atlasButton, state);
		}
		inline HRESULT RedrawVisual()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CButton::RedrawVisual>(this);
		}
	};

	struct ACCENT_POLICY
	{
		DWORD AccentState;
		DWORD AccentFlags;
		DWORD dwGradientColor;
		DWORD dwAnimationId;

		bool IsActive() const
		{
			return AccentState >= 1 && AccentState <= 4;
		}
		bool IsAccentBlurRectEnabled() const
		{
			return (AccentFlags & (1 << 9)) != 0;
		}
		bool IsGdiRegionRespected() const
		{
			return (AccentFlags & (1 << 4)) != 0;
		}
		bool IsClipRegionEffective() const
		{
			return (AccentFlags & ((1 << 4) | (1 << 9))) != 0;
		}
	};
	struct CAccent : CRenderDataVisual
	{
		ACCENT_POLICY& GetAccentPolicy()
		{
			return *CAccent_GetAccentPolicy.address(this);
		}
	};
	struct CAccentBlurBehind : CRenderDataVisual
	{
	};
	struct CAccentAcrylicBlurBehind : CRenderDataVisual
	{
	};
	struct CWindowBorder : CVisual
	{
		inline HRESULT EnableBorder(bool enable)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CWindowBorder::EnableBorder>(this, enable);
		}
	};

	struct IDwmWindow;
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

		inline ACCENT_POLICY* GetAccentPolicy() const
		{
			return CWindowData_GetAccentPolicy.mutable_address(this);
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
		inline BYTE& GetClientBlurAttributeReference()
		{
			return *CWindowData_GetClientBlurAttribute.address(this);
		}
		bool ShouldTransitionOnMaximized() const
		{
			return GetClientBlurAttribute() & 0x8;
		}
		inline RECT& GetWindowRect() const
		{
			return CWindowData_GetWindowRect.mutable_ref(this);
		}
		inline MARGINS& GetClientMargins()
		{
			return *CWindowData_GetClientMargins.address(this);
		}
		inline MARGINS& GetExtendedFrameMargins()
		{
			return *CWindowData_GetExtendedFrameMargins.address(this);
		}
		bool IsFrameExtendedIntoClientAreaLRB()
		{
			MARGINS& margins = GetExtendedFrameMargins();

			return margins.cxLeftWidth || margins.cxRightWidth || margins.cyBottomHeight;
		}
		bool IsSheetOfGlass()
		{
			MARGINS& margins = GetExtendedFrameMargins();

			return IsSheetOfGlass(margins);
		}
		static bool IsSheetOfGlass(const MARGINS& margins)
		{
			return margins.cxLeftWidth == 0x7FFFFFFF || margins.cxRightWidth == 0x7FFFFFFF ||
				   margins.cyTopHeight == 0x7FFFFFFF || margins.cyBottomHeight == 0x7FFFFFFF;
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
			return D2D1::ColorF(color.r * balance, color.g * balance, color.b * balance, color.a);
		}
	};
	struct CTopLevelWindow : CVisual
	{
		struct WindowFrame
		{
			inline UINT32& GetCornerRadius()
			{
				return *CTopLevelWindow_WindowFrame_GetCornerRadius.address(this);
			}
			inline CBitmapSource*& GetCloseButtonGlowImage()
			{
				return CTopLevelWindow_WindowFrame_GetCloseButtonGlowImage.ref(this);
			}
			inline CBitmapSource*& GetMinMaxButtonGlowImage()
			{
				return CTopLevelWindow_WindowFrame_GetMinMaxButtonGlowImage.ref(this);
			}
		};
		inline static PVOID* vftable{nullptr};
		inline static WindowFrame*** s_rgpwfWindowFrames{nullptr};
		static auto GetWindowFrames()
		{
			return *s_rgpwfWindowFrames;
		}
		inline static HRESULT CreateBitmapFromAtlas(
			HTHEME theme,
			int partId,
			MARGINS* margins,
			CBitmapSource** bitmapSource
		)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::CreateBitmapFromAtlas>(theme, partId, margins, bitmapSource);
		}
		inline static HRESULT CreateGlyphsFromAtlas(HTHEME theme)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::CreateGlyphsFromAtlas>(theme);
		}
		inline HRESULT UpdateButtonVisuals(const WindowFrame* windowFrame)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::UpdateButtonVisuals>(this, windowFrame);
		}
		inline CAtlasedImage const*& GetNCAreaAtlasImage1()
		{
			return CTopLevelWindow_GetNCAreaAtlasImage1.ref(this);
		}
		inline CAtlasedImage const*& GetNCAreaAtlasImage2()
		{
			return CTopLevelWindow_GetNCAreaAtlasImage2.ref(this);
		}
		inline CAtlasedImage const*& GetNCAreaAtlasImage3()
		{
			return CTopLevelWindow_GetNCAreaAtlasImage3.ref(this);
		}

		bool IsOffscreen()
		{
			const auto& offset = GetOffset();

			return offset.x == -32000 || offset.y == -32000;
		}

		inline CWindowData* GetData() const
		{
			return *CTopLevelWindow_GetData_Index.address(this);
		}
		inline CText* GetTextVisual() const
		{
			return *CTopLevelWindow_GetTextVisual_Index.address(this);
		}
		inline CImage* GetIconVisual() const
		{
			return *CTopLevelWindow_GetIconVisual_Index.address(this);
		}
		inline CDWriteText* GetDWriteTextVisual() const
		{
			return *CTopLevelWindow_GetDWriteTextVisual_Index.address(this);
		}
		inline CAccent* GetAccent() const
		{
			return *CTopLevelWindow_GetAccent_Index.address(this);
		}
		inline CCanvasVisual* GetLegacyVisual() const
		{
			return *CTopLevelWindow_GetLegacyVisual_Index.address(this);
		}
		inline CCanvasVisual* GetClientBlurVisual() const
		{
			return *CTopLevelWindow_GetClientBlurVisual_Index.address(this);
		}
		inline CWindowBorder* GetWindowBorder() const
		{
			return *CTopLevelWindow_GetWindowBorder_Index.address(this);
		}

		inline CRgnGeometryProxy* GetBorderGeometry() const
		{
			if (g_versionInfo.build < os::build_w11_22h2)
			{
				return *CTopLevelWindow_GetBorderGeometry_Index_OnThis.address(this);
			}
			const auto legacyVisual = GetLegacyVisual();
			return legacyVisual
					   ? *CTopLevelWindow_GetBorderGeometry_Index_OnLegacy.address(
							 legacyVisual)
					   : nullptr;
		}
		inline CRgnGeometryProxy* GetCaptionGeometry() const
		{
			if (g_versionInfo.build < os::build_w11_22h2)
			{
				return *CTopLevelWindow_GetCaptionGeometry_Index_OnThis.address(this);
			}
			const auto legacyVisual = GetLegacyVisual();
			return legacyVisual
					   ? *CTopLevelWindow_GetCaptionGeometry_Index_OnLegacy.address(
							 legacyVisual)
					   : nullptr;
		}
		inline CSolidColorLegacyMilBrushProxy* GetCaptionBrush() const
		{
			if (g_versionInfo.build < os::build_w11_22h2)
			{
				return *CTopLevelWindow_GetCaptionBrush_Index_OnThis.address(
					this);
			}
			const auto legacyVisual = GetLegacyVisual();
			return legacyVisual ? *CTopLevelWindow_GetCaptionBrush_Index_OnLegacy
									   .address(legacyVisual)
								: nullptr;
		}
		inline CSolidColorLegacyMilBrushProxy* GetBorderBrush() const
		{
			if (g_versionInfo.build < os::build_w11_22h2)
			{
				return *CTopLevelWindow_GetBorderBrush_Index_OnThis.address(
					this);
			}
			const auto legacyVisual = GetLegacyVisual();
			return legacyVisual ? *CTopLevelWindow_GetBorderBrush_Index_OnLegacy
									   .address(legacyVisual)
								: nullptr;
		}
		inline CSolidColorLegacyMilBrushProxy* GetClientBlurBrush() const
		{
			return *CTopLevelWindow_GetClientBlurBrush_Index_OnThis.address(
				this);
		}

		inline bool& GetIsBorderUpdatesSuppressed()
		{
			return *CTopLevelWindow_GetIsBorderUpdatesSuppressed_ByteIndex.address(this);
		}

		inline bool IsRTLMirrored() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 0x20000) != 0;
		}
		inline bool IsWindowMaximized() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 0x20) != 0;
		}
		inline bool IsToolWindow() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 2) != 0;
		}
		inline bool IsLoneButton() const
		{
			return (*CTopLevelWindow_StateDwordIndex.address(this) & 0xB00) == 0;
		}

		inline bool HasNonClientArea() const
		{
			const auto margins = GetFrameInsideMargins();
			return margins.cxLeftWidth || margins.cxRightWidth || margins.cyBottomHeight || margins.cyTopHeight;
		}
		bool HasNonClientBackground(CWindowData* data = nullptr) const
		{
			if (!data)
			{
				data = GetData();
			}
			if ((data->GetNonClientAttribute() & 8) == 0)
			{
				return false;
			}

			if (!HasNonClientArea())
			{
				return false;
			}

			return true;
		}

		bool TreatAsMaximized(const CWindowData* data) const;

		inline CGlassColorizationResources* GetCaptionColorizationParameters() const
		{
			return *CTopLevelWindow_GetCaptionColorizationParameters_Index
						.address(this);
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

		inline HRESULT CloneVisualTreeForLivePreview_Win10(bool windowFramesOnly, bool unused1, bool unused2,
														   CTopLevelWindow** clonedWindow)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::CloneVisualTreeForLivePreview_Win10>(
				this, windowFramesOnly, unused1, unused2, clonedWindow);
		}
		inline HRESULT CloneVisualTreeForLivePreview_Win11(bool windowFramesOnly, CTopLevelWindow** clonedWindow)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::CloneVisualTreeForLivePreview_Win11>(this, windowFramesOnly,
																							 clonedWindow);
		}
		HRESULT CloneVisualTreeForLivePreview(bool windowFramesOnly, CTopLevelWindow** clonedWindow)
		{
			if (g_versionInfo.build < os::build_w11_22h2) [[likely]]
			{
				return CloneVisualTreeForLivePreview_Win10(windowFramesOnly, false, false, clonedWindow);
			}
			else
			{
				return CloneVisualTreeForLivePreview_Win11(windowFramesOnly, clonedWindow);
			}
		}
		inline bool TreatAsActiveWindow()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::TreatAsActiveWindow>(this);
		}
		inline HRESULT OnBlurBehindUpdated()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::OnBlurBehindUpdated>(this);
		}
		inline HRESULT OnAccentPolicyUpdated()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::OnAccentPolicyUpdated>(this);
		}
		inline RECT* GetActualWindowRect(RECT* rect, char relative, char respectMaximizedClip,
										 bool excludeShadowMargins) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CTopLevelWindow::GetActualWindowRect>(this, rect, relative, respectMaximizedClip,
																			 excludeShadowMargins);
		}
	};
	struct CTopLevelWindow3D : CRenderDataVisual
	{
	};

	struct LivePreviewVisual
	{
		CWindowData* data;
		CTopLevelWindow* livePreview;
		CTopLevelWindow* windowFrames;
		bool unknown0;
		bool unknown1;
		bool unknown2;
		bool unknown3;
		DWORD unkown4;
		ULONG_PTR unknown5;
	};
	struct LivePreviewResource
	{
		CHAR reserved[136];

		inline const RECT* GetWindowBoundingRect() const
		{
			return reinterpret_cast<const RECT*>(this);
		}
		inline CRectangleGeometryProxy* GetWindowBoundingGeometry() const
		{
			return *LivePreviewResource_GetWindowBoundingGeometry.address(this);
		}
		inline CBaseLegacyMilBrushProxy* GetWindowVisualBrush() const
		{
			return *LivePreviewResource_GetWindowVisualBrush.address(this);
		}

		inline const RECT* GetGlassBoundingRect() const
		{
			return LivePreviewResource_GetGlassBoundingRect.address(this);
		}
		inline CRectangleGeometryProxy* GetGlassBoundingGeometry() const
		{
			return *LivePreviewResource_GetGlassBoundingGeometry.address(this);
		}
		inline CBaseLegacyMilBrushProxy* GetGlassVisualBrush() const
		{
			return *LivePreviewResource_GetGlassVisualBrush.address(this);
		}

		inline HRGN GetReflectionRegion() const
		{
			return *LivePreviewResource_GetReflectionRegion.address(this);
		}
		inline CRgnGeometryProxy* GetReflectionGeometry() const
		{
			return *LivePreviewResource_GetReflectionGeometry.address(this);
		}

		inline bool IsWindowBoundingRectNotEmpty() const
		{
			return *LivePreviewResource_IsWindowBoundingRectNotEmpty_Index.address(this);
		}
		inline bool IsGlassBoundingRectNotEmpty() const
		{
			return *LivePreviewResource_IsGlassBoundingRectNotEmpty_Index.address(this);
		}
	};
	struct CLivePreview : CRenderDataVisual
	{
		inline CRenderDataVisual* GetThumbnailVisual() const
		{
			return *CLivePreview_GetThumbnailVisual.address(this);
		}
		inline CRenderDataVisual* GetGlassVisual() const
		{
			return *CLivePreview_GetGlassVisual.address(this);
		}
		inline DynArray<LivePreviewResource>* GetLivePreviewResourceArray() const
		{
			return CLivePreview_GetLivePreviewResourceArray.mutable_address(this);
		}
		inline DynArray<LivePreviewVisual>* GetLivePreviewVisualArray() const
		{
			return CLivePreview_GetLivePreviewVisualArray.mutable_address(this);
		}
		inline HRESULT _UpdateResources()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CLivePreview::_UpdateResources>(this);
		}
	};
	struct CAnimatedGlassSheet : CVisual
	{
		inline const RECT& GetSourceRect() const
		{
			return *CAnimatedGlassSheet_GetSourceRect.address(this);
		}
		inline const RECT& GetDestinationRect() const
		{
			return *CAnimatedGlassSheet_GetDestinationRect.address(this);
		}
		inline const RECT& GetAdjustedDestinationRect()
		{
			return *CAnimatedGlassSheet_GetAdjustedDestinationRect.address(this);
		}

		inline LONG GetAtlasPaddingTop() const
		{
			return CAnimatedGlassSheet_GetAtlasPaddingTop_Rect.address(this)->left;
		}
		inline LONG GetAtlasPaddingLeft() const
		{
			return CAnimatedGlassSheet_GetAtlasPaddingLeft_Rect.address(this)->right;
		}
		inline LONG GetAtlasPaddingRight() const
		{
			return -CAnimatedGlassSheet_GetAtlasPaddingRight_Rect.address(this)->bottom;
		}
		inline LONG GetAtlasPaddingBottom() const
		{
			return -CAnimatedGlassSheet_GetAtlasPaddingBottom_Rect.address(this)->top;
		}
	};
	struct CWindowList : CBaseObject
	{
		inline PRLIST_ENTRY GetWindowListForDesktop(ULONG_PTR desktopID)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CWindowList::GetWindowListForDesktop>(this, desktopID);
		}
	};

	struct CCompositor
	{
		inline HRESULT CreateSolidColorLegacyMilBrushProxy(
			CSolidColorLegacyMilBrushProxy** solidColorBrushProxy
		)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CCompositor::CreateSolidColorLegacyMilBrushProxy>(this, solidColorBrushProxy);
		}
		inline HRESULT CreateImageLegacyMilBrushProxy(CImageLegacyMilBrushProxy** imageBrushProxy)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CCompositor::CreateImageLegacyMilBrushProxy>(this, imageBrushProxy);
		}
		IDwmChannel* GetChannel() const
		{
			if (g_versionInfo.build < os::build_w10_1903)
			{
				return reinterpret_cast<IDwmChannel*>(const_cast<CCompositor*>(this));
			}
			return *CCompositor_GetChannel_Index.address(this);
		}
		IDCompositionDesktopDevicePartner* GetInteropCompositorDCompDevicePartner() const
		{
			return *CCompositor_GetInteropCompositorDCompDevicePartner
						.address(this);
		}
	};

	inline HRESULT Fallback_CCompositor_CreateSolidColorLegacyMilBrushProxy(
		CCompositor* This, CSolidColorLegacyMilBrushProxy** solidColorBrushProxy
	)
	{
		return CResource::Create(DwmResourceType::SolidColorLegacyMilBrushProxy, This->GetChannel(),
								 reinterpret_cast<CResource**>(solidColorBrushProxy));
	}

	inline HRESULT Fallback_CCompositor_CreateImageLegacyMilBrushProxy(
		CCompositor* This, CImageLegacyMilBrushProxy** imageBrushProxy
	)
	{
		return CResource::Create(DwmResourceType::ImageLegacyMilBrushProxy, This->GetChannel(),
								 reinterpret_cast<CResource**>(imageBrushProxy));
	}
	struct CDesktopManager
	{
		inline static CDesktopManager** s_pDesktopManagerInstance{nullptr};
		inline static LPCRITICAL_SECTION s_csDwmInstance{nullptr};

		static CDesktopManager* GetInstance()
		{
			return *s_pDesktopManagerInstance;
		}
		inline CLivePreview* GetLivePreview() const
		{
			return *CDesktopManager_GetLivePreview_Index.address(this);
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
			if (g_versionInfo.build < os::build_server_2022)
			{
				return *CDesktopManager_GetD2DDevice_OnThis_Index.address(this);
			}
			auto graphicsManager = *CDesktopManager_GetGraphicsManager_Index.address(this);
			return graphicsManager ? *CGraphicsManager_GetD2DDevice_Index.address(graphicsManager)
								   : nullptr;
		}
		inline IDCompositionDesktopDevicePartner* GetInteropCompositorDCompDevicePartner() const
		{
			if (g_versionInfo.build < os::build_server_2022)
			{
				return *CDesktopManager_GetDCompDevice_OnThis_Index
							.address(this);
			}
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

		return IsWindowMaximized() ||
			   (data && data->ShouldTransitionOnMaximized() && CDesktopManager::GetInstance()->HasMaximizedWindows());
	}
	inline HRESULT CRenderDataVisual::Create(CRenderDataVisual** visual)
	{
		HRESULT hr{S_OK};

		if (g_versionInfo.build < os::build_w10_1903)
		{
			struct CDesktopManager;
			hr = Create_Pre_W10_1903(uDWM::CDesktopManager::GetInstance()->GetCompositor()->GetChannel(), visual);
		}
		else
		{
			hr = Create_At_Least_W10_1903(visual);
		}

		return hr;
	}

	namespace ResourceHelper
	{
		inline HRESULT CreateGeometryFromHRGN(HRGN hrgn, CRgnGeometryProxy** geometry)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&ResourceHelper::CreateGeometryFromHRGN>(hrgn, geometry);
		}
		inline HRESULT CreateCombinedGeometry(
			CBaseGeometryProxy* geometry1, CBaseGeometryProxy* geometry2,
			D2D1_COMBINE_MODE combineMode, // this param is always ignored in udwm implementation
			CCombinedGeometryProxy** geometry)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&ResourceHelper::CreateCombinedGeometry>(geometry1, geometry2, combineMode,
																			   geometry);
		}
	} // namespace ResourceHelper

	inline uDWM::CTopLevelWindow* TryGetWindowFromVisual(uDWM::CVisual* visual)
	{
		auto current = visual->GetTransformParent();

		while (current && HookHelper::get_vftable_from(current) != uDWM::CTopLevelWindow::vftable)
		{
			current = current->GetTransformParent();
		}

		return static_cast<uDWM::CTopLevelWindow*>(current);
	}

} // namespace OpenGlass::uDWM

#include "udwm.Symbols.generated.hpp"
