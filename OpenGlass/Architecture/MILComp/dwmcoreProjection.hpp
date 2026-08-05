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
	inline const auto g_moduleHandle{ GetModuleHandleW(L"dwmcore.dll") };
	inline const auto g_versionInfo{ Util::GetModuleVersionInfo(g_moduleHandle) };

	struct IMILResource
	{
		virtual ULONG AddRef(void) = 0;
		virtual ULONG Release(void) = 0;
		// ...
	};
	struct CResource : IMILResource {};
	struct CResourceTable : IMILResource {};

	struct CMILMatrix;
	struct COcclusionContext;
	struct CCompositionSurfaceInfo;
	struct CRegion;
	struct CVisual;
	struct CDirtyRegion;
	struct CGeometry;
	struct CVisualTree : CResource {};
	struct CDesktopTree : CVisualTree {};
	struct CVisual : CResource
	{
		inline HWND GetHwnd() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::GetHwnd>(this);
		}
		inline CGeometry* GetClipNoRef() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CVisual::GetClipNoRef>(this);
		}
	};
	struct CSpriteVisual : CVisual
	{
		inline static PVOID* vftable{ nullptr };
	};

	struct IContent
	{
		// GetBounds
		// AddOcclusionInformation
		// Draw
		// HitTest
		// IsEmptyDrawing
		// ...
	};
	struct CContent : CResource, IContent {};

	struct CBrush : CContent {};
	struct CColorBrush : CBrush
	{
		inline static PVOID* vftable{ nullptr };

		const D2D1_COLOR_F& GetColor() const
		{
			return *CColorBrush_GetColor.address(this);
		}
	};

	struct CMILMatrix : D2D1_MATRIX_4X4_F
	{
		int flag;

		D2D1_MATRIX_3X2_F GetD2DMatrix() const
		{
			return D2D1::Matrix3x2F(
				_11, _12,
				_21, _22,
				_41, _42
			);
		}
		D2D1_MATRIX_4X4_F GetD3DMatrix() const
		{
			return D2D1::Matrix4x4F(
				_11, _12, _13, _14,
				_21, _22, _23, _24,
				_31, _32, _33, _34,
				_41, _42, _43, _44
			);
		}
	};
	struct CMatrixStack
	{
		inline const CMILMatrix* GetTopByReference() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CMatrixStack::GetTopByReference>(this);
		}
	};

	struct CZOrderedRect
	{
		D2D1_RECT_F m_transformedRect;
		int m_depth;
		CVisual* m_visual;
		D2D1_RECT_F m_originalRect;

		inline void UpdateDeviceRect(const CMILMatrix* matrix)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CZOrderedRect::UpdateDeviceRect>(this, matrix);
		}
		CZOrderedRect() = default;
		CZOrderedRect(const D2D1_RECT_F& rect, int depth, const CMILMatrix* matrix) : m_depth{ depth }, m_originalRect{ rect }
		{
			UpdateDeviceRect(matrix);
		}
	};
	struct CArrayBasedCoverageSet
	{
		DynArray<CZOrderedRect>* GetOccluderArray() const
		{
			return CArrayBasedCoverageSet_GetOccluderArray.mutable_address(this);
		}
	};
	struct CPathData;
	struct CShape
	{
		virtual ~CShape() = default;
		virtual UINT GetType() const = 0;
		virtual bool IsEmpty() const = 0;
		virtual HRESULT GetD2DGeometry(const CMILMatrix* matrix, ID2D1Geometry** geometry) const = 0;
		virtual HRESULT GetOutline(ID2D1GeometrySink* sink) const = 0;
		virtual HRESULT FlattenToLineSegments(float tolerance, CShape** shape) const = 0;
		virtual HRESULT GetTightBounds(D2D1_RECT_F* bounds, const CMILMatrix* matrix) const = 0;
		virtual bool ContainsOnlyPolygons() const = 0;
		virtual bool IsRectangles(UINT* count) const = 0;
		virtual bool AllowsOcclusion() const = 0;
		virtual bool GetRectangles(D2D1_RECT_F* buffer, UINT count) const = 0;
		virtual HRESULT GetBoundsForOcclusion(D2D1_RECT_F* bounds) const = 0;
		virtual HRESULT GetSimplifiedPathDataInternal(CPathData** pathData) const = 0;

		inline HRESULT CopyShape(
			const CMILMatrix* matrix,
			CShape** shape
		) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CShape::CopyShape>(this, matrix, shape);
		}
	};
	struct CRectanglesShape : CShape {};
	struct CRegionShape : CShape {};

	class CShapePtr
	{
		CShape* m_ptr{ nullptr };
		bool m_release{ true };
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
				std::invoke(
					**reinterpret_cast<void(CShape::***)(BYTE)>(m_ptr),
					m_ptr,
					true
				);
			}

			m_ptr = nullptr;
		}

		CShapePtr() noexcept = default;
		CShapePtr(CShape* src, bool release = true) noexcept : m_ptr{ src }, m_release{ !release } {}
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
		inline HRESULT GetShapeData(
			const D2D1_SIZE_F* size,
			CShapePtr* shape
		)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CGeometry::GetShapeData>(this, size, shape);
		}
	};
	struct CCombinedGeometry : CGeometry
	{
		inline static PVOID* vftable{ nullptr };
	};
	struct CDrawingContext;
	struct COcclusionContext;

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
			return ID2DContextOwner_GetCurrentZ.read(HookHelper::get_vftable_from(this))(this);
		}
		inline const RenderTargetInfo& GetCurrentRenderTargetInfo() const
		{
			return ID2DContextOwner_GetCurrentRenderTargetInfo.read(HookHelper::get_vftable_from(this))(this);
		}
	};

	struct CD2DContext : CResource
	{
		inline void EnsureBeginDraw()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CD2DContext::EnsureBeginDraw>(this);
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
		inline CD2DContext* GetD2DContext() const
		{
			return CD3DDevice_GetD2DContext.mutable_address(this);
		}
	};
	struct IBitmapResource;
	struct IDeviceTexture
	{
		inline ID3D11Texture2D* GetTexture2D(UINT* unknown = nullptr) const
		{
			return IDeviceTexture_GetTexture2D.read(HookHelper::get_vftable_from(this))(this, unknown);
		}
		inline ID3D11ShaderResourceView* GetShaderResourceView() const
		{
			return IDeviceTexture_GetShaderResourceView.read(HookHelper::get_vftable_from(this))(this);
		}
	};
	struct IDeviceTarget
	{
		inline ID3D11RenderTargetView* GetRenderTargetView() const
		{
			return IDeviceTarget_GetRenderTargetView.read(HookHelper::get_vftable_from(this))(this);
		}
		inline IDeviceTexture* GetDeviceTexture() const
		{
			return IDeviceTarget_GetDeviceTexture.mutable_address(this);
		}
	};
	struct COverlayContext : CResource
	{
		struct OverlayPlaneInfo;
	};
	struct CDrawingContext : IMILResource
	{
		// since windows 10 2004
		inline IDeviceTarget* GetDeviceTarget() const
		{
			return CDrawingContext_DeviceTarget.read(this);
		}
		inline CD3DDevice* GetD3DDevice() const
		{
			return *CDrawingContext_GetD3DDevice.address(this);
		}
		inline ID2DContextOwner* GetD2DContextOwner() const
		{
			return CDrawingContext_GetD2DContextOwner.mutable_address(this);
		}

		inline const CMILMatrix* GetWorldTransform() const
		{
			return CDrawingContext_GetWorldTransform.address(this)->GetTopByReference();
		}
		inline CMILMatrix* GetDeviceTransform() const
		{
			return CDrawingContext_GetDeviceTransform.mutable_address(this);
		}
		inline COcclusionContext* GetOcclusion() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::GetOcclusion>(this);
		}
		inline void GetClipBoundsWorld(D2D1_RECT_F& rect, bool useProjectionBounds) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::GetClipBoundsWorld>(this, rect, useProjectionBounds);
		}
		inline HRESULT FillShapeWithBrush(const CShape* shape, const ID2D1Brush* brush)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::FillShapeWithBrush>(this, shape, brush);
		}
		inline HRESULT FlushD2D()
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::FlushD2D>(this);
		}
		inline HRESULT ApplyRenderStateInternal(bool skipFlushingDeferredClipping)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::ApplyRenderStateInternal>(this, skipFlushingDeferredClipping);
		}
		inline CVisual* GetCurrentVisual() const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&CDrawingContext::GetCurrentVisual>(this);
		}

		float GetSDRBoost() const
		{
			return GetD2DContextOwner()->GetCurrentRenderTargetInfo().GetSDRBoost();
		}
	};
	struct COcclusionInfo;
	struct COcclusionContext : IMILResource
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
		inline D2D1_RECT_F PageInPixelsRectToDeviceRect(const D2D1_RECT_F& pixelsRect) const
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
			return COcclusionContext_GetArrayBasedCoverageSet.mutable_address(this);
		}
		inline HRESULT SetDeviceTransform(
			const dwmcore::CMILMatrix* matrix
		) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&COcclusionContext::SetDeviceTransform>(this, matrix);
		}
		inline bool IsOccluded(const D2D1_RECT_F& rect, int depth, bool ignoreDeviceTransform) const
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&COcclusionContext::IsOccluded>(this, rect, depth, ignoreDeviceTransform);
		}
		inline void CollectRectangleForOcclusion(const D2D1_RECT_F& rect, bool recordVisual)
		{
			OPENGLASS_MUSTTAIL return Projection::Invoke<&COcclusionContext::CollectRectangleForOcclusion>(this, rect, recordVisual);
		}
	};
	struct CTreeDirty {};
	struct CCachedVisualImage : CResource
	{
		struct CCachedTarget : CResource
		{
		};
	};
	struct CDrawListCache : CResource {};
	struct CDrawListEntryBuilder : CResource {};

	inline ULONGLONG GetCurrentFrameId()
	{
		OPENGLASS_MUSTTAIL return Projection::Invoke<&GetCurrentFrameId>();
	}

	using CVisual_SetClip_t = void(*)(CVisual*, CGeometry*);
	using CVisual_CollectOcclusion_t = HRESULT(*)(CVisual*, COcclusionContext*, COcclusionInfo*);
	using CSpriteVisual_Destructor_t = void(*)(CSpriteVisual*);
	using CShape_AllowsOcclusion_t = bool(*)(CShape*);
	using CGeometry_Destructor_t = void(*)(CGeometry*);
	using CCombinedGeometry_ProcessUpdate_t = HRESULT(*)(CCombinedGeometry*, CResourceTable*, const DWM::MILCMD_COMBINEDGEOMETRY*);
	using CD2DContext_DestroyDeviceResources_t = HRESULT(*)(CD2DContext*);
	using CDrawingContext_DrawVisualTree_t = HRESULT(*)(
		CDrawingContext*,
		CVisualTree*,
		const D2D1_RECT_F&,
		const COcclusionContext*,
		int,
		float,
		CVisual*
	);
	using CDrawingContext_PreSubgraph_t = HRESULT(*)(CDrawingContext*, CVisualTree*, bool*);
	using CCachedVisualImage_CCachedTarget_Update_t = HRESULT(*)(
		CCachedVisualImage::CCachedTarget*,
		const D2D1_RECT_F&,
		DWM::MilStretch,
		const RenderTargetInfo&
	);
	using COcclusionContext_CheckAndRecordOverlayCandidate_t = HRESULT(*)(
		COcclusionContext*,
		CVisual*,
		CCompositionSurfaceInfo*,
		const CMILMatrix&,
		const CShape*,
		int
	);
	using COcclusionContext_Compute_t = HRESULT(*)(
		COcclusionContext*,
		const CVisualTree*,
		const DWM::span<D2D1_RECT_F>&,
		float,
		const DWM::span<COverlayContext*>&
	);
	using COcclusionContext_Destructor_t = void(*)(COcclusionContext*);
	using CTreeDirty_GetOptimizedRect_t = D2D1_RECT_F*(*)(
		CTreeDirty*,
		D2D1_RECT_F*,
		UINT,
		const D2D1_RECT_F&,
		const COcclusionContext*,
		const CRegion*,
		const CMILMatrix*,
		const DWM::span<CVisual>&
	);

} // namespace OpenGlass::dwmcore

#include "dwmcore.Symbols.generated.hpp"
