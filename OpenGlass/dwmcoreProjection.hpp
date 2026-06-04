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
	struct CResource : IMILResource
	{
		DECLSPEC_INDIRECT_PROJECTION DWORD GetOwningProcessId()
		{
			return HANDLE_PROJECTION_FUNCTION(CResource::GetOwningProcessId, this);
		}
	};
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
		DECLSPEC_INDIRECT_PROJECTION HWND GetHwnd() const
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::GetHwnd, this);
		}
		DECLSPEC_INDIRECT_PROJECTION CGeometry* GetClipNoRef() const
		{
			return HANDLE_PROJECTION_FUNCTION(CVisual::GetClipNoRef, this);
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
			return *Util::PointerExecuteUnsafe<CColorBrush_GetColor_Offsets, Util::OffsetBy<D2D1_COLOR_F*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};

	struct CMILMatrix : D2D1_MATRIX_4X4_F
	{
		int flag;
		inline static const CMILMatrix* Identity{ nullptr };

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
		DECLSPEC_INDIRECT_PROJECTION const CMILMatrix* GetTopByReference() const
		{
			return HANDLE_PROJECTION_FUNCTION(CMatrixStack::GetTopByReference, this);
		}
	};

	struct CZOrderedRect
	{
		D2D1_RECT_F m_transformedRect;
		int m_depth;
		CVisual* m_visual;
		D2D1_RECT_F m_originalRect;

		DECLSPEC_INDIRECT_PROJECTION void UpdateDeviceRect(const CMILMatrix* matrix)
		{
			return HANDLE_PROJECTION_FUNCTION(CZOrderedRect::UpdateDeviceRect, this, matrix);
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
			return Util::PointerExecuteUnsafe<CArrayBasedCoverageSet_GetOccluderArray_Offsets, Util::OffsetBy<DynArray<CZOrderedRect>*>>(this, g_versionInfo.build, g_versionInfo.revision);
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

		DECLSPEC_INDIRECT_PROJECTION HRESULT CopyShape(
			const CMILMatrix* matrix,
			CShape** shape
		) const
		{
			return HANDLE_PROJECTION_FUNCTION(CShape::CopyShape, this, matrix, shape);
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
		DECLSPEC_INDIRECT_PROJECTION HRESULT GetShapeData(
			const D2D1_SIZE_F* size,
			CShapePtr* shape
		)
		{
			return HANDLE_PROJECTION_FUNCTION(CGeometry::GetShapeData, this, size, shape);
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
		DECLSPEC_DIRECT_PROJECTION float GetSDRBoost() const
		{
			return *Util::PointerExecuteUnsafe<RenderTargetInfo_GetSDRBoost_Offsets, Util::OffsetBy<float*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	struct ID2DContextOwner
	{
		DECLSPEC_DIRECT_PROJECTION UINT GetCurrentZ() const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<ID2DContextOwner_GetCurrentZ_Offsets, Util::DereferenceAt<decltype(&ID2DContextOwner::GetCurrentZ)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this
			);
		}
		DECLSPEC_DIRECT_PROJECTION const RenderTargetInfo& GetCurrentRenderTargetInfo() const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<ID2DContextOwner_GetCurrentRenderTargetInfo_Offsets, Util::DereferenceAt<decltype(&ID2DContextOwner::GetCurrentRenderTargetInfo)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this
			);
		}
	};
	
	struct CD2DContext : CResource
	{
		DECLSPEC_INDIRECT_PROJECTION void EnsureBeginDraw()
		{
			return HANDLE_PROJECTION_FUNCTION(CD2DContext::EnsureBeginDraw, this);
		}
		DECLSPEC_DIRECT_PROJECTION ID2D1DeviceContext* GetDeviceContext() const
		{
			return Util::PointerExecuteUnsafe<CD2DContext_GetDeviceContext_Offsets, Util::DereferenceAt<ID2D1DeviceContext*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	// CD3DDeviceLevel1 before windows 10 2004
	struct CD3DDevice
	{
		DECLSPEC_DIRECT_PROJECTION ID3D11Device* GetDevice() const
		{
			return Util::PointerExecuteUnsafe<CD3DDevice_GetDevice_Offsets, Util::DereferenceAt<ID3D11Device*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION ID3D11DeviceContext* GetImmediateContext() const
		{
			return Util::PointerExecuteUnsafe<CD3DDevice_GetImmediateContext_Offsets, Util::DereferenceAt<ID3D11DeviceContext*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION CD2DContext* GetD2DContext() const
		{
			return Util::PointerExecuteUnsafe<CD3DDevice_GetD2DContext_Offsets, Util::OffsetBy<CD2DContext*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	struct IBitmapResource;
	struct IDeviceTexture
	{
		DECLSPEC_DIRECT_PROJECTION ID3D11Texture2D* GetTexture2D(UINT* unknown = nullptr) const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<IDeviceTexture_GetTexture2D_Offsets, Util::DereferenceAt<decltype(&IDeviceTexture::GetTexture2D)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this,
				unknown
			);
		}
		DECLSPEC_DIRECT_PROJECTION ID3D11ShaderResourceView* GetShaderResourceView() const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<IDeviceTexture_GetShaderResourceView_Offsets, Util::DereferenceAt<decltype(&IDeviceTexture::GetShaderResourceView)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this
			);
		}
	};
	struct IDeviceTarget
	{
		DECLSPEC_DIRECT_PROJECTION ID3D11RenderTargetView* GetRenderTargetView() const
		{
			return std::invoke(
				Util::PointerExecuteUnsafe<IDeviceTarget_GetRenderTargetView_Offsets, Util::DereferenceAt<decltype(&IDeviceTarget::GetRenderTargetView)>>(HookHelper::get_vftable_from(this), g_versionInfo.build, g_versionInfo.revision),
				this
			);
		}
		DECLSPEC_DIRECT_PROJECTION IDeviceTexture* GetDeviceTexture() const
		{
			return Util::PointerExecuteUnsafe<IDeviceTarget_GetDeviceTexture_Offsets, Util::OffsetBy<IDeviceTexture*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
	};
	struct COverlayContext : CResource
	{
		struct OverlayPlaneInfo;
	};
	struct CDrawingContext : IMILResource
	{
		// since windows 10 2004
		DECLSPEC_DIRECT_PROJECTION IDeviceTarget* GetDeviceTarget() const
		{
			return reinterpret_cast<IDeviceTarget* const*>(this)[3];
		}
		DECLSPEC_DIRECT_PROJECTION CD3DDevice* GetD3DDevice() const
		{
			return *Util::PointerExecuteUnsafe<CDrawingContext_GetD3DDevice_Offsets, Util::OffsetBy<CD3DDevice**>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION ID2DContextOwner* GetD2DContextOwner() const
		{
			return Util::PointerExecuteUnsafe<CDrawingContext_GetD2DContextOwner_Offsets, Util::OffsetBy<ID2DContextOwner*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		
		DECLSPEC_DIRECT_PROJECTION const CMILMatrix* GetWorldTransform() const
		{
			return Util::PointerExecuteUnsafe<CDrawingContext_GetWorldTransform_Offsets, Util::OffsetBy<CMatrixStack*>>(this, g_versionInfo.build, g_versionInfo.revision)->GetTopByReference();
		}
		DECLSPEC_DIRECT_PROJECTION CMILMatrix* GetDeviceTransform() const
		{
			return Util::PointerExecuteUnsafe<CDrawingContext_GetDeviceTransform_Offsets, Util::OffsetBy<CMILMatrix*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_INDIRECT_PROJECTION COcclusionContext* GetOcclusion() const
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::GetOcclusion, this);
		}
		DECLSPEC_INDIRECT_PROJECTION void GetClipBoundsWorld(D2D1_RECT_F& rect) const
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::GetClipBoundsWorld, this, rect);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT FillShapeWithBrush(const CShape* shape, const ID2D1Brush* brush)
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::FillShapeWithBrush, this, shape, brush);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT FlushD2D()
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::FlushD2D, this);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT ApplyRenderStateInternal(bool skipFlushingDeferredClipping)
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::ApplyRenderStateInternal, this, skipFlushingDeferredClipping);
		}
		DECLSPEC_INDIRECT_PROJECTION CVisual* GetCurrentVisual() const
		{
			return HANDLE_PROJECTION_FUNCTION(CDrawingContext::GetCurrentVisual, this);
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
		DECLSPEC_DIRECT_PROJECTION ULONGLONG GetFrameId() const
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetFrameId_Offsets, Util::DereferenceAt<ULONGLONG>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION UINT GetCurrentZ() const
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetCurrentZ_Offsets, Util::DereferenceAt<UINT>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION const CMILMatrix* GetWorldTransform() const
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetWorldTransform_Offsets, Util::OffsetBy<CMatrixStack*>>(this, g_versionInfo.build, g_versionInfo.revision)->GetTopByReference();
		}
		DECLSPEC_DIRECT_PROJECTION const CMILMatrix* GetDeviceTransform() const
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetDeviceTransform_Offsets, Util::OffsetBy<CMILMatrix const*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION UINT* GetDeviceTransformFlag()
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetDeviceTransformFlag_Offsets, Util::OffsetBy<UINT*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION UINT GetDeviceTransformFlagValue() const
		{
			return *Util::PointerExecuteUnsafe<COcclusionContext_GetDeviceTransformFlag_Offsets, Util::OffsetBy<UINT*>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_DIRECT_PROJECTION D2D1_RECT_F PageInPixelsRectToDeviceRect(const D2D1_RECT_F& pixelsRect) const
		{
			auto result = pixelsRect;

			if (GetDeviceTransformFlagValue() & 0x1)
			{
				result = RectF::TransformRect(pixelsRect, GetDeviceTransform()->GetD2DMatrix());
			}

			return result;
		}
		DECLSPEC_DIRECT_PROJECTION CArrayBasedCoverageSet* GetArrayBasedCoverageSet() const
		{
			return Util::PointerExecuteUnsafe<COcclusionContext_GetArrayBasedCoverageSet_Offsets, Util::OffsetBy<CArrayBasedCoverageSet* const>>(this, g_versionInfo.build, g_versionInfo.revision);
		}
		DECLSPEC_INDIRECT_PROJECTION HRESULT SetDeviceTransform(
			const dwmcore::CMILMatrix* matrix
		) const
		{
			return HANDLE_PROJECTION_FUNCTION(COcclusionContext::SetDeviceTransform, this, matrix);
		}
		DECLSPEC_INDIRECT_PROJECTION bool IsOccluded(const D2D1_RECT_F& rect, int depth, bool ignoreDeviceTransform) const
		{
			return HANDLE_PROJECTION_FUNCTION(COcclusionContext::IsOccluded, this, rect, depth, ignoreDeviceTransform);
		}
		DECLSPEC_INDIRECT_PROJECTION void CollectRectangleForOcclusion(const D2D1_RECT_F& rect, bool recordVisual) const
		{
			return HANDLE_PROJECTION_FUNCTION(COcclusionContext::CollectRectangleForOcclusion, this, rect, recordVisual);
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

	DECLSPEC_INDIRECT_PROJECTION ULONGLONG GetCurrentFrameId()
	{
		return HANDLE_PROJECTION_FUNCTION(GetCurrentFrameId);
	}

	inline auto g_projectionArray = make_projection_array(
		g_versionInfo.build,

		MAKE_FUNCTION_PROJECTION_TUPLE(CResource::GetOwningProcessId, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CChannel::QueryResourceInterface, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CChannel::CombinedGeometryUpdate, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::GetHwnd, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CVisual::GetClipNoRef, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CVisual::SetClip", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CVisual::CollectOcclusion", 0, 0),
		MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(CSpriteVisual::vftable, "CSpriteVisual::`vftable'", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CSpriteVisual::~CSpriteVisual", 0, 0),
		
		MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(CColorBrush::vftable, "CColorBrush::`vftable'", 0, 0),

		MAKE_VARIABLE_PROJECTION_TUPLE(CMILMatrix::Identity, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CMatrixStack::GetTopByReference, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CZOrderedRect::UpdateDeviceRect, 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CShape::CopyShape, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CShape::AllowsOcclusion", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(CGeometry::GetShapeData, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CGeometry::~CGeometry", 0, 0),
		MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(CCombinedGeometry::vftable, "CCombinedGeometry::`vftable'", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CCombinedGeometry::ProcessUpdate", 0, 0),

		MAKE_EMPTY_PROJECTION_TUPLE("CD2DContext::DestroyDeviceResources", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CD2DContext::EnsureBeginDraw, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::GetOcclusion, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::GetClipBoundsWorld, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::FillShapeWithBrush, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::FlushD2D, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::ApplyRenderStateInternal, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(CDrawingContext::GetCurrentVisual, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CDrawingContext::DrawVisualTree", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CDrawingContext::PreSubgraph", 0, 0),
		
		MAKE_EMPTY_PROJECTION_TUPLE("CCachedVisualImage::CCachedTarget::Update", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("COcclusionContext::CheckAndRecordOverlayCandidate", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("COcclusionContext::Compute", 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("COcclusionContext::~COcclusionContext", 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(COcclusionContext::SetDeviceTransform, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(COcclusionContext::IsOccluded, 0, 0),
		MAKE_FUNCTION_PROJECTION_TUPLE(COcclusionContext::CollectRectangleForOcclusion, 0, 0),
		MAKE_EMPTY_PROJECTION_TUPLE("CTreeDirty::GetOptimizedRect", 0, 0),

		MAKE_FUNCTION_PROJECTION_TUPLE(GetCurrentFrameId, 0, 0)
	);
	
	inline bool SymbolParserCallback(PSYMBOL_INFO info, [[maybe_unused]] ULONG size)
	{
		CHAR symbolName[128]{};
		UnDecorateSymbolName(info->Name, symbolName, std::size(symbolName), UNDNAME_NAME_ONLY);

		g_projectionArray.Apply(
			symbolName,
			reinterpret_cast<PVOID>(info->Address)
		);

		return !g_projectionArray.IsAllReady();
	}
}
