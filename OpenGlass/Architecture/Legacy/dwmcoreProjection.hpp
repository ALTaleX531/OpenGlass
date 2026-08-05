#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "ProjectionHelper.hpp"
#include "D2DPrivates.hpp"
#include "DWM.hpp"
#include "dwmcoreProjection.Offsets.hpp"

namespace OpenGlass::dwmcore
{
	using namespace DWM;
	inline const auto g_moduleHandle{GetModuleHandleW(L"dwmcore.dll")};
	inline const auto g_versionInfo{Util::GetModuleVersionInfo(g_moduleHandle)};

	struct CResource : IUnknown
	{
	};
	struct IMILResource
	{
		virtual ULONG AddRef(void) = 0;
		virtual ULONG Release(void) = 0;
	};
	inline HRESULT CChannel::MatrixTransformUpdate(UINT handleId, MilMatrix3x2D* matrix)
	{
		OPENGLASS_MUSTTAIL
		return Projection::Invoke<&CChannel::MatrixTransformUpdate>(this, handleId, matrix);
	}
	inline HRESULT CChannel::SolidColorLegacyMilBrushUpdate(UINT handleId, double opacity, const D2D1_COLOR_F& color,
															UINT opacityAnimationsHandleId, UINT transformHandleId,
															UINT relativeTransformHandleId)
	{
		OPENGLASS_MUSTTAIL
		return Projection::Invoke<&CChannel::SolidColorLegacyMilBrushUpdate>(
			this, handleId, opacity, color, opacityAnimationsHandleId, transformHandleId, relativeTransformHandleId);
	}
	inline HRESULT CChannel::ImageLegacyMilBrushUpdate(
		UINT handleId, double opacity, const D2D1_RECT_F& viewport, const D2D1_RECT_F& viewbox,
		UINT opacityAnimationsHandleId, UINT transformHandleId, UINT relativeTransformHandleId,
		MilBrushMappingMode viewportUnits, MilBrushMappingMode viewboxUnits, UINT viewportAnimationsHandleId,
		UINT viewboxAnimationsHandleId, MilStretch stretchMode, MilTileMode tileMode, MilHorizontalAlignment alignmentX,
		MilVerticalAlignment alignmentY, UINT imageSourceHandleId)
	{
		OPENGLASS_MUSTTAIL
		return Projection::Invoke<&CChannel::ImageLegacyMilBrushUpdate>(
			this, handleId, opacity, viewport, viewbox, opacityAnimationsHandleId, transformHandleId,
			relativeTransformHandleId, viewportUnits, viewboxUnits, viewportAnimationsHandleId,
			viewboxAnimationsHandleId, stretchMode, tileMode, alignmentX, alignmentY, imageSourceHandleId);
	}

	struct CMILMatrix;
	struct COcclusionContext;
	struct CRegion;
	struct CVisual;
	struct CDirtyRegion;
	struct ISwapChainContent;
	struct CVisualTree : CResource
	{
	};
	struct CDirtyRegion;
	struct CDesktopTree : CVisualTree
	{
	};
	struct CVisual : CResource
	{
		inline HWND GetTopLevelWindow() const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CVisual::GetTopLevelWindow>(this);
		}
	};
	struct CFloatResource : CResource
	{
	};

	struct CMILMatrix : D2D1_MATRIX_4X4_F
	{
		int flag;
		inline static const CMILMatrix* Identity{nullptr};

		D2D1_MATRIX_3X2_F GetD2DMatrix() const
		{
			return D2D1::Matrix3x2F(_11, _12, _21, _22, _41, _42);
		}
		D2D1_MATRIX_4X4_F GetD3DMatrix() const
		{
			return D2D1::Matrix4x4F(_11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44);
		}
	};
	struct CMatrixStack
	{
		inline const CMILMatrix* GetTopByReference() const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CMatrixStack::GetTopByReference>(this);
		}
	};

	struct CZOrderedRectBase
	{
		D2D1_RECT_F m_transformedRect;
		int m_depth;

		D2D1_RECT_F& GetOriginalRect();
		void UpdateDeviceRectInternal(const CMILMatrix* matrix, const D2D1_RECT_F& originalRect)
		{
			auto& transformedRect = m_transformedRect;
			if (matrix)
			{
				transformedRect = RectF::TransformRect(originalRect, matrix->GetD2DMatrix());
			}
			else
			{
				transformedRect = originalRect;
			}
			transformedRect.left = std::ceil(transformedRect.left);
			transformedRect.top = std::ceil(transformedRect.top);
			transformedRect.right = std::floor(transformedRect.right);
			transformedRect.bottom = std::floor(transformedRect.bottom);
		}
		void UpdateDeviceRect(const CMILMatrix* matrix)
		{
			UpdateDeviceRectInternal(matrix, GetOriginalRect());
		}
		CZOrderedRectBase() = default;
		CZOrderedRectBase(int depth) : m_depth{depth} {}
	};
	struct CZOrderedRect : CZOrderedRectBase
	{
		D2D1_RECT_F m_originalRect;

		void UpdateDeviceRect(const CMILMatrix* matrix)
		{
			UpdateDeviceRectInternal(matrix, m_originalRect);
		}
		CZOrderedRect() = default;
		CZOrderedRect(const D2D1_RECT_F& rect, int depth, const CMILMatrix* matrix)
			: CZOrderedRectBase{depth}, m_originalRect{rect}
		{
			UpdateDeviceRect(matrix);
		}
	};
	struct CZOrderedRect2 : CZOrderedRectBase
	{
		CVisual* m_visual;
		D2D1_RECT_F m_originalRect;

		void UpdateDeviceRect(const CMILMatrix* matrix)
		{
			UpdateDeviceRectInternal(matrix, m_originalRect);
		}
		CZOrderedRect2() = default;
		CZOrderedRect2(const D2D1_RECT_F& rect, int depth, const CMILMatrix* matrix)
			: CZOrderedRectBase{depth}, m_originalRect{rect}
		{
			UpdateDeviceRect(matrix);
		}
	};
	inline D2D1_RECT_F& CZOrderedRectBase::GetOriginalRect()
	{
		return CZOrderedRectBase_OriginalRect.ref(this);
	}

	struct CArrayBasedCoverageSet : CResource
	{
		inline DynArray<CZOrderedRect>* GetAntiOccluderArray() const
		{
			return CArrayBasedCoverageSet_GetAntiOccluderArray.mutable_address(this);
		}
		DynArray<CZOrderedRect>* GetOccluderArray() const
		{
			return CArrayBasedCoverageSet_GetOccluderArray.mutable_address(this);
		}
		DynArray<CZOrderedRect2>* GetOccluderArray2() const
		{
			return reinterpret_cast<DynArray<CZOrderedRect2>*>(const_cast<CArrayBasedCoverageSet*>(this));
		}

		bool IsCovered(const D2D1_RECT_F& coverage, int depth) const
		{
			bool antiOccluderExisted{false};
			int antiOccluderDepth{};
			if (g_versionInfo.build < os::build_server_2022)
			{
				for (const auto& zorderedRect : GetAntiOccluderArray()->views())
				{
					if (zorderedRect.m_depth >= depth)
					{
						break;
					}

					if (!wil::rect_is_empty(zorderedRect.m_transformedRect) &&
						std::abs(wil::rect_height(zorderedRect.m_transformedRect) *
								 wil::rect_width(zorderedRect.m_transformedRect)) > 1.f &&

						RectF::DoesIntersectUnsafe(zorderedRect.m_transformedRect, coverage))
					{
						antiOccluderDepth = zorderedRect.m_depth;
						antiOccluderExisted = true;
						break;
					}
				}
			}

			const auto checkArrayBasedCoverageSet =
				[&coverage, depth, antiOccluderExisted, antiOccluderDepth](auto&& views)
			{
				auto visibleRect = coverage;

				for (const auto& zorderedRect : views)
				{
					if (zorderedRect.m_depth >= depth)
					{
						break;
					}

					if (!wil::rect_is_empty(zorderedRect.m_transformedRect) &&
						(!antiOccluderExisted || zorderedRect.m_depth > antiOccluderDepth))
					{
						if (zorderedRect.m_transformedRect.left <= visibleRect.left &&
							zorderedRect.m_transformedRect.right >= visibleRect.right)
						{
							if (visibleRect.top >= zorderedRect.m_transformedRect.top)
							{
								if (zorderedRect.m_transformedRect.bottom >= visibleRect.bottom)
								{
									return true;
								}
								if (zorderedRect.m_transformedRect.bottom > visibleRect.top)
								{
									visibleRect.top = zorderedRect.m_transformedRect.bottom;
								}
							}
							else if (zorderedRect.m_transformedRect.bottom >= coverage.bottom &&
									 coverage.bottom > zorderedRect.m_transformedRect.top)
							{
								visibleRect.bottom = zorderedRect.m_transformedRect.top;
							}
						}
					}
				}

				return false;
			};

			if (
				Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_with_25h2_code_staged>(
					g_versionInfo.build, g_versionInfo.revision
				)
			)
			{
				return checkArrayBasedCoverageSet(GetOccluderArray()->views());
			}
			else
			{
				return checkArrayBasedCoverageSet(GetOccluderArray2()->views());
			}
		}
	};

	struct CRenderCommand
	{
		UINT type;
	};
	struct CDrawGeometryCommand : CRenderCommand
	{
		UINT brushIndex;
		UINT geometryIndex;
	};
	struct CRenderDataBuilder : CResource
	{
	};
	struct CRenderData : CResource
	{
		using CRenderDataResourceArray = DynArray<CResource*>;
		inline CRenderDataResourceArray* GetResources() const
		{
			return CRenderData_GetResources.mutable_address(this);
		}
	};
	struct CLegacyMilBrush : CResource
	{
	};
	struct CSolidColorLegacyMilBrush : CLegacyMilBrush
	{
		inline const D2D1_COLOR_F& GetRealizedColor() const
		{
			return *CSolidColorLegacyMilBrush_GetRealizedColor.address(this);
		}
		inline static PVOID* vftable{nullptr};
	};
	struct CImageSource : CResource
	{
	};
	struct CImageLegacyMilBrush : CLegacyMilBrush
	{
		inline static PVOID* vftable{nullptr};

		inline CImageSource* GetImageSource() const
		{
			return CImageLegacyMilBrush_GetImageSource.read(this);
		}
		inline float GetOpacityValue() const
		{
			if (g_versionInfo.build < os::build_w10_1903)
			{
				return static_cast<float>(CImageLegacyMilBrush_GetOpacityValue_Double.read(this));
			}
			return CImageLegacyMilBrush_GetOpacityValue.read(this);
		}
		inline CFloatResource* GetFloatResource() const
		{
			return CImageLegacyMilBrush_GetFloatResource.read(this);
		}
		inline const D2D1_RECT_F& GetViewport() const
		{
			return *CImageLegacyMilBrush_GetViewport.address(this);
		}
		inline const D2D1_RECT_F& GetViewbox() const
		{
			return *CImageLegacyMilBrush_GetViewbox.address(this);
		}
	};

	struct CTransform : CResource
	{
	};
	struct CShape
	{
		virtual ~CShape() = default;
		virtual UINT GetType() const = 0;
		virtual bool IsEmpty() const = 0;
		virtual HRESULT GetD2DGeometry(const CMILMatrix* matrix, ID2D1Geometry** geometry) const = 0;

		inline HRESULT CopyShape(const CMILMatrix* matrix, CShape** shape) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CShape::CopyShape>(this, matrix, shape);
		}
		inline static HRESULT Combine(const CShape* shape1, const CMILMatrix* matrix1, const CShape* shape2,
									  const CMILMatrix* matrix2, D2D1_COMBINE_MODE mode, CShape** shape)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CShape::Combine>(shape1, matrix1, shape2, matrix2, mode, shape);
		}

		inline HRESULT GetTightBounds(D2D1_RECT_F* lprc, const CMILMatrix* matrix) const
		{
			return std::invoke(CShape_GetTightBounds.read(HookHelper::get_vftable_from(this)), this, lprc, matrix);
		}
		inline bool IsRectangles(UINT* count) const
		{
			return std::invoke(CShape_IsRectangles.read(HookHelper::get_vftable_from(this)), this, count);
		}
		inline bool GetRectangles(D2D1_RECT_F* buffer, UINT count) const
		{
			return std::invoke(CShape_GetRectangles.read(HookHelper::get_vftable_from(this)), this, buffer, count);
		}
	};
	struct CRectanglesShape : CShape
	{
	};
	struct CRegionShape : CShape
	{
		static inline PVOID* vftable{nullptr};
		static inline PVOID dtor{nullptr};

		inline HRESULT BuildFromRects(const D2D1_RECT_L* buffer, UINT count)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CRegionShape::BuildFromRects>(this, buffer, count);
		}
	};
	// valid until (build, revision)
	template <ULONG build = 0, ULONG revision = 0> struct CRegionShapeImpl
	{
		static_assert(false, "Not implemented.");
	};
	template <> struct CRegionShapeImpl<os::build_w10_2004, 0>
	{
		struct FastRegionStorage
		{
			ULONG_PTR data;
			DWORD count;
			std::byte payload[60];
		};

		void* vftable;
		void* unknown;
		FastRegionStorage regionData;
		ID2D1Geometry* geometry;
		static_assert(sizeof(FastRegionStorage) == 72);

		CRegionShapeImpl()
		{
			memset(this, 0, sizeof(*this));
			vftable = CRegionShape::vftable;
			regionData.data = reinterpret_cast<ULONG_PTR>(&regionData.count);
		}
		~CRegionShapeImpl()
		{
			std::invoke(Util::force_cast_to<void (CRegionShape::*)()>(CRegionShape::dtor),
						reinterpret_cast<CRegionShape*>(this));
		}
		CRegionShape* As()
		{
			return reinterpret_cast<CRegionShape*>(this);
		}
	};
	static_assert(offsetof(CRegionShapeImpl<os::build_w10_2004>, regionData) +
				  offsetof(CRegionShapeImpl<os::build_w10_2004>::FastRegionStorage, data) == 16);
	static_assert(offsetof(CRegionShapeImpl<os::build_w10_2004>, regionData) +
				  offsetof(CRegionShapeImpl<os::build_w10_2004>::FastRegionStorage, count) == 24);

	class CShapePtr
	{
		CShape* m_ptr{nullptr};
		bool m_release{true};

	  public:
		CShape* operator->() const
		{
			return m_ptr;
		}
		CShape* get() const
		{
			return m_ptr;
		}
		CShape** put()
		{
			Release();
			return &m_ptr;
		}
		explicit operator bool() const
		{
			return m_ptr != nullptr;
		}
		void Release()
		{
			if (m_release && m_ptr)
			{
				std::invoke(**reinterpret_cast<void (CShape::***)(BYTE)>(m_ptr), m_ptr, true);
			}

			m_ptr = nullptr;
		}

		CShapePtr() noexcept = default;
		CShapePtr(CShape* src, bool release = true) noexcept : m_ptr{src}, m_release{!release} {}
		CShapePtr(const CShapePtr&) noexcept = delete;
		CShapePtr(CShapePtr&& src) noexcept
		{
			Release();
			m_ptr = src.m_ptr;
			m_release = src.m_release;
			src.m_ptr = nullptr;
		}
		~CShapePtr() noexcept
		{
			Release();
		}
	};
	struct CGeometry : CResource
	{
		inline HRESULT GetShapeData(const D2D1_SIZE_F* size, CShapePtr* shape)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CGeometry::GetShapeData>(this, size, shape);
		}
	};
	struct CRectangleGeometry : CGeometry
	{
		inline static PVOID* vftable{nullptr};
	};
	struct CGeometry2D : CGeometry
	{
	};
	struct CDrawingContext;
	struct COcclusionContext;
	struct IDrawingContext
	{
		virtual HRESULT Clear(const D2D1_COLOR_F& color) = 0;
		virtual HRESULT DrawRectangle(const D2D1_RECT_F& lprc, CLegacyMilBrush* brush, CResource* resource) = 0;
		virtual HRESULT DrawSolidRectangle(const D2D1_RECT_F& lprc, const D2D1_COLOR_F& color) = 0;
		virtual HRESULT DrawImage(CResource* image, const D2D1_RECT_F* lprc, CResource* resource) = 0;
		virtual HRESULT DrawGeometry(CLegacyMilBrush* brush, CGeometry* geometry) = 0;
		virtual HRESULT TileImage(CResource* image, D2D1_RECT_F& lprc, D2D1_POINT_2F& point, float) = 0;
		virtual HRESULT DrawBitmap(CResource* bitmap) = 0;
		virtual HRESULT DrawInk(ID2D1Ink* ink, const D2D1_COLOR_F& color, ID2D1InkStyle* inkStyle) = 0;
		virtual HRESULT DrawGenericInk(struct IDCompositionDirectInkWetStrokePartner*, bool) = 0;
		virtual HRESULT DrawYCbCrBitmap(CResource*, CResource*, D2D1_YCBCR_CHROMA_SUBSAMPLING) = 0;
		virtual HRESULT DrawMesh2D(CGeometry2D* geometry, CImageSource* imageSource) = 0;
		virtual HRESULT DrawVisual(CVisual* visual) = 0;
		virtual HRESULT Pop() = 0;
		virtual HRESULT PushTransform(CTransform* transfrom) = 0;
		virtual HRESULT ApplyRenderState() = 0;

		inline CDrawingContext* GetDrawingContext() const
		{
			return IDrawingContext_GetDrawingContext.mutable_address(this);
		}
		COcclusionContext* GetOcclusionContext() const
		{
			return reinterpret_cast<COcclusionContext*>(reinterpret_cast<ULONG_PTR>(this));
		}
	};

	struct RenderTargetInfo
	{
		// since windows 10 22h2
		inline float GetSDRBoost() const
		{
			return *RenderTargetInfo_GetSDRBoost.address(this);
		}
	};
	struct ID2DContextOwner
	{
		inline UINT GetCurrentZ() const
		{
			return std::invoke(ID2DContextOwner_GetCurrentZ.read(HookHelper::get_vftable_from(this)), this);
		}
		inline const RenderTargetInfo& GetCurrentRenderTargetInfo() const
		{
			return std::invoke(
				ID2DContextOwner_GetCurrentRenderTargetInfo.read(HookHelper::get_vftable_from(this)), this);
		}
	};

	enum class DisplayId : DWORD;
	struct CD2DContext : CResource
	{
		inline void EnsureBeginDraw()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CD2DContext::EnsureBeginDraw>(this);
		}
		inline ID2D1DeviceContext* GetDeviceContext() const
		{
			return CD2DContext_GetDeviceContext.read(this);
		}
	};
	// CD3DDeviceLevel1 before windows 10 2004
	struct CD3DDevice
	{
		inline ID3D11Device* GetDevice() const
		{
			return CD3DDevice_GetDevice.read(this);
		}
		inline ID3D11DeviceContext* GetImmediateContext() const
		{
			return CD3DDevice_GetImmediateContext.read(this);
		}
		CD2DContext* GetD2DContext() const
		{
			return CD3DDevice_GetD2DContext.mutable_address(this);
		}
	};
	struct IBitmapResource;
	struct IDeviceTexture
	{
		inline ID3D11Texture2D* GetTexture2D() const
		{
			return std::invoke(IDeviceTexture_GetTexture2D.read(HookHelper::get_vftable_from(this)), this);
		}
		inline ID3D11ShaderResourceView* GetShaderResourceView() const
		{
			return std::invoke(
				IDeviceTexture_GetShaderResourceView.read(HookHelper::get_vftable_from(this)), this);
		}
	};
	struct CD3DSurface
	{
		inline ID3D11Texture2D* GetTexture2D() const
		{
			return CD3DSurface_GetTexture2D.read(this);
		}
		inline ID3D11ShaderResourceView* GetShaderResourceView() const
		{
			return CD3DSurface_GetShaderResourceView.read(this);
		}
		inline ID3D11RenderTargetView* GetRenderTargetView() const
		{
			return CD3DSurface_GetRenderTargetView.read(this);
		}
	};
	struct IRenderTarget
	{
		inline CD3DSurface* GetTargetSurfaceNoRef() const
		{
			return std::invoke(IRenderTarget_GetTargetSurfaceNoRef.read(HookHelper::get_vftable_from(this)), this);
		}
		inline float GetSDRBoost() const
		{
			return std::invoke(IRenderTarget_GetSDRBoost.read(HookHelper::get_vftable_from(this)), this);
		}
	};
	struct IDeviceTarget
	{
		inline ID3D11RenderTargetView* GetRenderTargetView() const
		{
			return std::invoke(IDeviceTarget_GetRenderTargetView.read(HookHelper::get_vftable_from(this)), this);
		}
		inline IDeviceTexture* GetDeviceTexture() const
		{
			return IDeviceTarget_GetDeviceTexture.mutable_address(this);
		}
	};
	struct CComposeTop;
	struct CHwndRenderTarget : CResource
	{
	};
	struct COverlayContext : CResource
	{
		struct OverlayPlaneInfo;
	};
	struct CCompositionSurfaceInfo;
	struct ISwapChainRealization;
	struct CDrawingContext
	{
		// since windows 10 2004
		IDeviceTarget* GetDeviceTarget() const
		{
			return CDrawingContext_DeviceTarget.read(this);
		}
		// before windows 10 2004
		IRenderTarget* GetRenderTarget() const
		{
			return CDrawingContext_RenderTarget.read(this);
		}
		CD3DDevice* GetD3DDevice() const
		{
			return *CDrawingContext_GetD3DDevice.address(this);
		}
		inline IDrawingContext* GetInterface() const
		{
			return CDrawingContext_GetInterface.mutable_address(this);
		}
		inline ID2DContextOwner* GetD2DContextOwner() const
		{
			return CDrawingContext_GetD2DContextOwner.mutable_address(this);
		}
		inline COcclusionContext* GetOcclusionContext() const
		{
			return CDrawingContext_GetOcclusionContext.read(this);
		}

		inline const CMILMatrix* GetWorldTransform() const
		{
			return CDrawingContext_GetWorldTransform.address(this)->GetTopByReference();
		}
		inline CMILMatrix* GetDeviceTransform() const
		{
			return CDrawingContext_GetDeviceTransform.mutable_address(this);
		}
		inline bool IsBounding() const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::IsBounding>(this);
		}
		inline bool CalcPartiallyVisibleRectangleSet(const D2D1_RECT_F& bounds, int depth, D2D1_RECT_F* rectangles[4],
													 UINT* count) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::CalcPartiallyVisibleRectangleSet>(this, bounds, depth,
																						  rectangles, count);
		}
		inline HRESULT GetUnOccludedWorldShape(
			const CShape* shape, int depth, CShape** worldShape
		) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::GetUnOccludedWorldShape>(this, shape, depth, worldShape);
		}
		inline void CalcWorldSpaceClippedBounds(const D2D1_RECT_F& rect, D2D_RECT_F* clippedBounds) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::CalcWorldSpaceClippedBounds>(this, rect, clippedBounds);
		}
		inline void GetClipBoundsWorld(D2D1_RECT_F& rect, bool useProjectionBounds) const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::GetClipBoundsWorld>(this, rect, useProjectionBounds);
		}
		inline HRESULT FillShapeWithBrush(const CShape* shape, const ID2D1Brush* brush)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::FillShapeWithBrush>(this, shape, brush);
		}
		inline HRESULT FlushD2D()
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::FlushD2D>(this);
		}
		inline HRESULT ApplyRenderStateInternal(bool skipFlushingDeferredClipping)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::ApplyRenderStateInternal>(this, skipFlushingDeferredClipping);
		}
		inline CVisual* GetCurrentVisual() const
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&CDrawingContext::GetCurrentVisual>(this);
		}

		CVisual* GetCurrentVisualHelper() const
		{
			return (g_versionInfo.build >= os::build_w11_24h2
						? this
						: reinterpret_cast<dwmcore::CDrawingContext*>(GetD2DContextOwner()))
				->GetCurrentVisual();
		}
		float GetSDRBoost() const
		{
			if (g_versionInfo.build < os::build_w10_2004)
			{
				return GetRenderTarget()->GetSDRBoost();
			}
			else
			{
				return GetD2DContextOwner()->GetCurrentRenderTargetInfo().GetSDRBoost();
			}
		}
	};

	inline HRESULT Fallback_CDrawingContext_GetUnOccludedWorldShape(
		const CDrawingContext* This, const CShape* shape, int depth, CShape** worldShape
	)
	{
		const auto worldTransform = This->GetWorldTransform();
		D2D1_RECT_F bounds{};
		RETURN_IF_FAILED(shape->GetTightBounds(&bounds, nullptr));

		D2D1_RECT_F rectanglesF[4]{};
		UINT count{};
		if (D2D1_RECT_F* buffer[4]{&rectanglesF[0], &rectanglesF[1], &rectanglesF[2], &rectanglesF[3]};
			!This->CalcPartiallyVisibleRectangleSet(bounds, depth, buffer, &count))
		{
			return MILERR_GENERIC_IGNORE;
		}

		CRegionShapeImpl<os::build_w10_2004> regionShape{};
		D2D1_RECT_L rectanglesL[4]{};
		for (uint32_t i = 0; i < count; i++)
		{
			rectanglesL[i] = RectF::ToRectL(rectanglesF[i]);
		}
		RETURN_IF_FAILED(regionShape.As()->BuildFromRects(rectanglesL, count));

		return CShape::Combine(regionShape.As(), nullptr, shape, worldTransform, D2D1_COMBINE_MODE_INTERSECT,
							   worldShape);
	}

	inline void Fallback_CDrawingContext_GetClipBoundsWorld(
		const CDrawingContext* This, D2D1_RECT_F& rect, [[maybe_unused]] bool useProjectionBounds
	)
	{
		This->CalcWorldSpaceClippedBounds({-INFINITY, -INFINITY, INFINITY, INFINITY}, &rect);
	}
	struct COcclusionContext : IDrawingContext
	{
		// not needed before windows 10 2004
		inline ULONGLONG GetFrameId() const
		{
			return COcclusionContext_GetFrameId.read(this);
		}
		inline UINT GetCurrentZ() const
		{
			return COcclusionContext_GetCurrentZ.read(this);
		}
		inline const CMILMatrix* GetWorldTransform() const
		{
			return COcclusionContext_GetWorldTransform.address(this)->GetTopByReference();
		}
		inline const CMILMatrix* GetDeviceTransform() const
		{
			return COcclusionContext_GetDeviceTransform.address(this);
		}
		inline UINT* GetDeviceTransformFlag()
		{
			return COcclusionContext_GetDeviceTransformFlag.address(this);
		}
		inline UINT GetDeviceTransformFlagValue() const
		{
			return *COcclusionContext_GetDeviceTransformFlag.address(this);
		}
		D2D1_RECT_F PageInPixelsRectToDeviceRect(const D2D1_RECT_F& pixelsRect) const
		{
			auto result = pixelsRect;

			if (GetDeviceTransformFlagValue() & 0x1)
			{
				result = RectF::TransformRect(pixelsRect, GetDeviceTransform()->GetD2DMatrix());
			}

			return result;
		}
		inline CArrayBasedCoverageSet* GetArrayBasedCoverageSet() const
		{
			const auto result = COcclusionContext_GetArrayBasedCoverageSet.mutable_address(this);

			// stored as a pointer before
			if (g_versionInfo.build < os::build_w10_2004)
			{
				return *reinterpret_cast<CArrayBasedCoverageSet* const*>(result);
			}

			return result;
		}
		inline HRESULT SetDeviceTransform(const dwmcore::CMILMatrix* matrix)
		{
			OPENGLASS_MUSTTAIL
			return Projection::Invoke<&COcclusionContext::SetDeviceTransform>(this, matrix);
		}
	};
	struct CDirtyRegion
	{
		// not exist before windows 10 2004
		inline COcclusionContext* GetOcclusionContext() const
		{
			return CDirtyRegion_GetOcclusionContext.mutable_address(this);
		}
	};
	struct CTreeDirty
	{
	};
	struct CWindowOcclusionInfo : CResource
	{
	};
	struct CCachedVisualImage : CResource
	{
		struct RenderTargetBitmapInfo : CResource
		{
		};
		struct CCachedTarget : CResource
		{
		};
	};
	struct CDrawListCache : CResource
	{
	};
	struct CDrawListEntryBuilder : CResource
	{
	};

	inline ULONGLONG GetCurrentFrameId()
	{
		OPENGLASS_MUSTTAIL
		return Projection::Invoke<&GetCurrentFrameId>();
	}

	using COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_t = HRESULT(*)(COcclusionContext*, CVisual*, ISwapChainContent*, const CMILMatrix*, const CShape*, int);
	using COcclusionContext_CheckAndRecordOverlayCandidate_19041_t = HRESULT(*)(COcclusionContext*, CVisual*, CCompositionSurfaceInfo*, const CMILMatrix*, const CShape*, int);
	using COcclusionContext_CheckAndRecordOverlayCandidate_26100_t = HRESULT(*)(COcclusionContext*, CVisual*, CCompositionSurfaceInfo*, const CMILMatrix&, const CShape*, int);
	using COcclusionContext_CollectRectangleForOcclusion_Pre_22000_t = HRESULT(*)(COcclusionContext*, const D2D1_RECT_F*, bool, D2D1_RECT_F*);
	using COcclusionContext_CollectRectangleForOcclusion_22000_t = void(*)(COcclusionContext*, const D2D1_RECT_F*, bool, bool, D2D1_RECT_F*);
	using COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_t = void(*)(COcclusionContext*, const D2D1_RECT_F&, D2D1_RECT_F*);
	using COcclusionContext_CollectRectangleForOcclusion_26100_7840_t = void(*)(COcclusionContext*, const D2D1_RECT_F&, bool, D2D1_RECT_F*);

} // namespace OpenGlass::dwmcore

#include "dwmcore.Symbols.generated.hpp"
