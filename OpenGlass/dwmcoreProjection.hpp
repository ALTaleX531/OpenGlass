#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "Utils.hpp"
#include "HookHelper.hpp"
#include "OSHelper.hpp"

namespace OpenGlass::dwmcore
{
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"dwmcore.dll") };
	inline std::unordered_map<std::string, PVOID> g_symbolMap{};
	template <typename T>
	FORCEINLINE T GetAddressFromSymbolMap(std::string_view functionName)
	{
		auto it = g_symbolMap.find(std::string{ functionName });
		return it != g_symbolMap.end() ? Utils::cast_pointer<T>(it->second) : nullptr;
	}
	template <typename T>
	FORCEINLINE void GetAddressFromSymbolMap(std::string_view functionName, T& target)
	{
		auto it = g_symbolMap.find(std::string{ functionName });
		if (it != g_symbolMap.end()) [[likely]]
		{
			target = Utils::cast_pointer<T>(it->second);
		}
#ifdef _DEBUG
		else
		{
			OutputDebugStringA(std::format("{} - symbol missing for {}\n", __FUNCTION__, functionName.data()).c_str());
		}
#endif // _DEBUG
	}

	struct CResource : IUnknown
	{
		UINT STDMETHODCALLTYPE GetOwningProcessId()
		{
			DEFINE_INVOKER(CResource::GetOwningProcessId);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	struct CChannel
	{
		HRESULT STDMETHODCALLTYPE MatrixTransformUpdate(UINT handleIndex, MilMatrix3x2D* matrix)
		{
			DEFINE_INVOKER(CChannel::MatrixTransformUpdate);
			return INVOKE_MEMBERFUNCTION(handleIndex, matrix);
		}
		HRESULT STDMETHODCALLTYPE CombinedGeometryUpdate(UINT combinedGeometryHandleIndex, UINT combineMode, UINT geometryHandleIndex1, UINT geometryHandleIndex2)
		{
			DEFINE_INVOKER(CChannel::CombinedGeometryUpdate);
			return INVOKE_MEMBERFUNCTION(combinedGeometryHandleIndex, combineMode, geometryHandleIndex1, geometryHandleIndex2);
		}
	};

	struct CZOrderedRect
	{
		D2D1_RECT_F transformedRect;
		int depth;
		D2D1_RECT_F originalRect;

		HRESULT STDMETHODCALLTYPE UpdateDeviceRect(const MilMatrix3x2D* matrix)
		{
			DEFINE_INVOKER(CZOrderedRect::UpdateDeviceRect);
			return INVOKE_MEMBERFUNCTION(matrix);
		}
	};

	template <typename T>
	struct DynArray
	{
		T* data;
		T* buffer;
		UINT bufferCapacity;
		UINT capacity;
		UINT size;
	};
	template <typename T>
	struct MyDynArrayImpl : DynArray<T>
	{
		[[nodiscard]] void* operator new[](
			size_t size
		)
		{
			auto memory = HeapAlloc(OpenGlass::Utils::g_processHeap, 0, size);
			THROW_LAST_ERROR_IF_NULL(memory);
			return memory;
		}
		void operator delete[](
			void* ptr
		) noexcept
		{
			FAIL_FAST_IF_NULL(ptr);
			HeapFree(OpenGlass::Utils::g_processHeap, 0, ptr);
			ptr = nullptr;
		}

		MyDynArrayImpl()
		{
			this->capacity = 8;
			this->data = new T[this->capacity];
			this->size = 0;
		}
		~MyDynArrayImpl()
		{
			delete[] this->data;
			this->data = nullptr;
			this->capacity = this->size = 0;
		}

		void Clear()
		{
			if (this->size != 0)
			{
				this->capacity = 8;
				delete[] this->data;
				this->data = new T[this->capacity];
				this->size = 0;
			}
		}
		void Add(const T& object)
		{
			auto newSize = this->size + 1u;
			if (newSize < this->size)
			{
				FAIL_FAST_HR(static_cast<HRESULT>(0x80070216ul));
			}
			else
			{
				auto bufferSize = this->size * sizeof(T);
				if (newSize > this->capacity)
				{
					auto tmp = std::unique_ptr<T[]>(this->data);

					this->capacity *= 2;
					this->data = new T[this->capacity];
					memcpy_s(this->data, bufferSize, tmp.get(), bufferSize);
				}

				*reinterpret_cast<T*>(reinterpret_cast<ULONG_PTR>(this->data) + bufferSize) = object;
				this->size = newSize;
			}
		}
	};
	struct CArrayBasedCoverageSet : CResource
	{
		HRESULT STDMETHODCALLTYPE Add(
			const D2D1_RECT_F& lprc,
			int depth,
			const MilMatrix3x2D* matrix
		)
		{
			DEFINE_INVOKER(CArrayBasedCoverageSet::Add);
			return INVOKE_MEMBERFUNCTION(lprc, depth, matrix);
		}

		DynArray<CZOrderedRect>* GetAntiOccluderArray() const
		{
			DynArray<CZOrderedRect>* array{ nullptr };
			if (os::buildNumber < os::build_w10_2004)
			{
				array = reinterpret_cast<DynArray<CZOrderedRect>*>(const_cast<CArrayBasedCoverageSet*>(this + 52));
			}
			else
			{
				array = reinterpret_cast<DynArray<CZOrderedRect>*>(const_cast<CArrayBasedCoverageSet*>(this + 49));
			}

			return array;
		}
		DynArray<CZOrderedRect>* GetOccluderArray() const
		{
			return reinterpret_cast<DynArray<CZOrderedRect>*>(const_cast<CArrayBasedCoverageSet*>(this));
		}
	};
	struct CVisual;
	struct CVisualTree : CResource 
	{
		CVisual* GetVisual() const
		{
			return reinterpret_cast<CVisual* const*>(this)[7];
		}
	};
	struct CVisual : CResource
	{
		HRESULT STDMETHODCALLTYPE GetVisualTree(CVisualTree** visualTree, bool value) const
		{
			DEFINE_INVOKER(CVisual::GetVisualTree);
			return INVOKE_MEMBERFUNCTION(visualTree, value);
		}
		const D2D1_RECT_F& STDMETHODCALLTYPE GetBounds(CVisualTree* visualTree) const
		{
			DEFINE_INVOKER(CVisual::GetBounds);
			return INVOKE_MEMBERFUNCTION(visualTree);
		}
		HWND STDMETHODCALLTYPE GetHwnd() const
		{
			DEFINE_INVOKER(CVisual::GetHwnd);
			return INVOKE_MEMBERFUNCTION();
		}
		HWND STDMETHODCALLTYPE GetTopLevelWindow() const
		{
			DEFINE_INVOKER(CVisual::GetTopLevelWindow);
			return INVOKE_MEMBERFUNCTION();
		}
	};

	typedef D2D1_MATRIX_5X4_F CMILMatrix;
	struct CLegacyMilBrush : CResource {};
	struct CSolidColorLegacyMilBrush : CLegacyMilBrush
	{
		const D2D1_COLOR_F& GetRealizedColor() const
		{
			return *reinterpret_cast<D2D1_COLOR_F*>(reinterpret_cast<ULONG_PTR>(this) + 88);
		}
	};

	struct CTransform : CResource {};
	struct CShape
	{
		virtual ~CShape() = default;
		virtual UINT STDMETHODCALLTYPE GetType() const = 0;
		virtual bool STDMETHODCALLTYPE IsEmpty() const = 0;
		virtual HRESULT STDMETHODCALLTYPE GetD2DGeometry(const CMILMatrix* matrix, ID2D1Geometry** geometry) const = 0;
		virtual HRESULT STDMETHODCALLTYPE GetTightBounds(D2D1_RECT_F* lprc, const CMILMatrix* matrix) const = 0;
		virtual bool STDMETHODCALLTYPE IsRectangles(UINT* count) const = 0;
		virtual bool STDMETHODCALLTYPE GetRectangles(D2D1_RECT_F* buffer, UINT count) const = 0;
		virtual HRESULT STDMETHODCALLTYPE GetUnOccludedWorldShape(const D2D1_RECT_F* bounds, const CMILMatrix& matrix, CShape** shape) const = 0;
	};
	struct CShapePtr
	{
		CShape* ptr;
		bool notRef;

		CShape* operator->()
		{
			return ptr;
		}
		operator bool()
		{
			return ptr != nullptr;
		}
		void Release()
		{
			if (notRef && ptr)
			{
				(**reinterpret_cast<void(***)(CShape*, bool)>(ptr))(ptr, true);
			}

			ptr = nullptr;
			notRef = false;
		}
		~CShapePtr()
		{
			Release();
		}
	};
	struct CGeometry : CResource 
	{
		HRESULT STDMETHODCALLTYPE GetShapeData(const D2D1_SIZE_F* size, CShapePtr* shape)
		{
			DEFINE_INVOKER(CGeometry::GetShapeData);
			return INVOKE_MEMBERFUNCTION(size, shape);
		}
	};
	struct CGeometry2D : CGeometry {};
	struct CImageSource : CResource {};
	struct CDrawingContext;
	struct IDrawingContext
	{
		virtual HRESULT STDMETHODCALLTYPE Clear(const D2D1_COLOR_F& color) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawRectangle(const D2D1_RECT_F& lprc, CLegacyMilBrush* brush, CResource* resource) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawSolidRectangle(const D2D1_RECT_F& lprc, const D2D1_COLOR_F& color) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawImage(CResource* image, const D2D1_RECT_F* lprc, CResource* resource) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawGeometry(CLegacyMilBrush* brush, CGeometry* geometry) = 0;
		virtual HRESULT STDMETHODCALLTYPE TileImage(CResource* image, D2D1_RECT_F& lprc, D2D1_POINT_2F& point, float) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawBitmap(CResource* bitmap) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawInk(ID2D1Ink* ink, const D2D1_COLOR_F& color, ID2D1InkStyle* inkStyle) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawGenericInk(struct IDCompositionDirectInkWetStrokePartner*, bool) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawYCbCrBitmap(CResource*, CResource*, D2D1_YCBCR_CHROMA_SUBSAMPLING) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawMesh2D(CGeometry2D* geometry, CImageSource* imageSource) = 0;
		virtual HRESULT STDMETHODCALLTYPE DrawVisual(CVisual* visual) = 0;
		virtual HRESULT STDMETHODCALLTYPE Pop() = 0;
		virtual HRESULT STDMETHODCALLTYPE PushTransform(CTransform* transfrom) = 0;
		virtual HRESULT STDMETHODCALLTYPE ApplyRenderState() = 0;

		CDrawingContext* GetDrawingContext() const
		{
			CDrawingContext* value{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				value = reinterpret_cast<CDrawingContext*>(reinterpret_cast<ULONG_PTR>(this));
			}
			else
			{
				value = reinterpret_cast<CDrawingContext*>(reinterpret_cast<ULONG_PTR>(this) - 16);
			}

			return value;
		}
	};

	typedef DWORD DisplayId;
	typedef UINT StereoContext;
	struct IImageSource;
	struct RenderTargetInfo;
	// 1809 limited
	/*struct ID2DContextOwner
	{
		virtual HRESULT STDMETHODCALLTYPE ImageSourceToD2DBitmap(IImageSource* imageSource, ID2D1Bitmap1** bitmap) = 0;
		virtual bool STDMETHODCALLTYPE IsIn3DMode() = 0;
		virtual void STDMETHODCALLTYPE GetWorldTransform3x2(D2D_MATRIX_3X2_F* transform) = 0;
		virtual void STDMETHODCALLTYPE GetWorldTransform4x4(D2D_MATRIX_4X4_F* transform) = 0;
		virtual UINT STDMETHODCALLTYPE GetCurrentZ() = 0;
		virtual CVisual* STDMETHODCALLTYPE GetCurrentVisual() = 0;
		virtual LUID STDMETHODCALLTYPE GetCurrentAdapterLuid() = 0;
		virtual DisplayId STDMETHODCALLTYPE GetCurrentDisplayId() = 0;
		virtual StereoContext STDMETHODCALLTYPE GetCurrentStereoContext() = 0;
		virtual bool STDMETHODCALLTYPE GetCurrentHardwareProtection() = 0;
		virtual RenderTargetInfo STDMETHODCALLTYPE GetCurrentRenderTargetInfo() = 0;
	};*/
	struct ID2DContextOwner
	{
		virtual bool STDMETHODCALLTYPE IsIn3DMode() = 0;
		virtual void STDMETHODCALLTYPE GetWorldTransform3x2(D2D_MATRIX_3X2_F* transform) = 0;
		virtual void STDMETHODCALLTYPE GetWorldTransform4x4(D2D_MATRIX_4X4_F* transform) = 0;
		virtual UINT STDMETHODCALLTYPE GetCurrentZ() = 0;
		virtual CVisual* STDMETHODCALLTYPE GetCurrentVisual() = 0;
		virtual RenderTargetInfo STDMETHODCALLTYPE GetCurrentRenderTargetInfo() = 0;
	};

	struct CD2DContext : CResource
	{
		ID2D1DeviceContext* GetDeviceContext() const
		{
			return reinterpret_cast<ID2D1DeviceContext* const*>(this)[30];
		}
		void STDMETHODCALLTYPE GetClip(ID2DContextOwner* owner, D2D1_RECT_F* clipRect, D2D1_ANTIALIAS_MODE* mode) const
		{
			DEFINE_INVOKER(CD2DContext::GetClip);
			return INVOKE_MEMBERFUNCTION(owner, clipRect, mode);
		}
	};
	struct IDeviceTarget;
	struct IDeviceSurface
	{
		virtual D2D1_SIZE_U STDMETHODCALLTYPE GetSize() const = 0;
		virtual DisplayId STDMETHODCALLTYPE GetDisplayId() const = 0;
		virtual HRESULT STDMETHODCALLTYPE GetD2DBitmap(ID2D1Bitmap1** bitmap, bool ignoreAlpha) = 0;
	};
	struct IBitmapResource;
	struct CDrawingContext
	{
		struct CDisableCPUClipScope
		{
			CDrawingContext* m_drawingContext;

			HRESULT STDMETHODCALLTYPE Enter(CDrawingContext* drawingContext)
			{
				DEFINE_INVOKER(CDrawingContext::CDisableCPUClipScope::Enter);
				return INVOKE_MEMBERFUNCTION(drawingContext);
			}
			~CDisableCPUClipScope()
			{
				DEFINE_CUSTOM_INVOKER(void(STDMETHODCALLTYPE CDisableCPUClipScope::*)(), "CDrawingContext::CDisableCPUClipScope::~CDisableCPUClipScope");
				INVOKE_MEMBERFUNCTION();
			}
		};

		IDeviceTarget* GetDeviceTarget() const
		{
			return reinterpret_cast<IDeviceTarget* const*>(this)[4];
		}
		HRESULT GetD2DBitmap(ID2D1Bitmap1** bitmap) const
		{
			auto deviceTarget = GetDeviceTarget();
			auto deviceSurface = reinterpret_cast<IDeviceSurface*>(
				reinterpret_cast<ULONG_PTR>(deviceTarget) +
				*reinterpret_cast<int*>(reinterpret_cast<ULONG_PTR*>(deviceTarget)[1] + 16) +
				8ull
			);

			return deviceSurface->GetD2DBitmap(bitmap, false);
		}
		CD2DContext* GetD2DContext() const
		{
			if (os::buildNumber < os::build_w10_2004)
			{
				return reinterpret_cast<CD2DContext* const*>(this)[48];
			}
			return reinterpret_cast<CD2DContext*>(reinterpret_cast<ULONG_PTR const*>(this)[5] + 16);
		}
		IDrawingContext* GetInterface() const
		{
			IDrawingContext* value{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				value = reinterpret_cast<IDrawingContext*>(reinterpret_cast<ULONG_PTR>(this));
			}
			else
			{
				value = reinterpret_cast<IDrawingContext*>(reinterpret_cast<ULONG_PTR>(this) + 16);
			}

			return value;
		}
		ID2DContextOwner* GetD2DContextOwner() const
		{
			ID2DContextOwner* value{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				value = reinterpret_cast<ID2DContextOwner*>(reinterpret_cast<ULONG_PTR>(this) + 8);
			}
			else
			{
				value = reinterpret_cast<ID2DContextOwner*>(reinterpret_cast<ULONG_PTR>(this) + 24);
			}

			return value;
		}
		IUnknown* GetUnknown() const
		{
			IUnknown* value{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				value = reinterpret_cast<IUnknown*>(reinterpret_cast<ULONG_PTR>(this) + 16);
			}
			else
			{
				value = reinterpret_cast<IUnknown*>(reinterpret_cast<ULONG_PTR>(this));
			}

			return value;
		}

		bool STDMETHODCALLTYPE IsOccluded(const D2D1_RECT_F& lprc, int depth) const
		{
			DEFINE_INVOKER(CDrawingContext::IsOccluded);
			return INVOKE_MEMBERFUNCTION(lprc, depth);
		}
		HRESULT STDMETHODCALLTYPE GetWorldTransform(CMILMatrix* matrix) const
		{
			DEFINE_INVOKER(CDrawingContext::GetWorldTransform);
			return INVOKE_MEMBERFUNCTION(matrix);
		}
		HRESULT STDMETHODCALLTYPE GetWorldTransform3x2(D2D1_MATRIX_3X2_F* matrix) const
		{
			DEFINE_INVOKER(CDrawingContext::GetWorldTransform3x2);
			return INVOKE_MEMBERFUNCTION(matrix);
		}
		ULONG_PTR STDMETHODCALLTYPE CalcWorldSpaceClippedBounds(const D2D1_RECT_F& sourceRect, const D2D1_RECT_F* targetRect) const
		{
			DEFINE_INVOKER(CDrawingContext::CalcWorldSpaceClippedBounds);
			return INVOKE_MEMBERFUNCTION(sourceRect, targetRect);
		}
		ULONG_PTR STDMETHODCALLTYPE CalcLocalSpaceClippedBounds(const D2D1_RECT_F& sourceRect, const D2D1_RECT_F* targetRect) const
		{
			DEFINE_INVOKER(CDrawingContext::CalcLocalSpaceClippedBounds);
			return INVOKE_MEMBERFUNCTION(sourceRect, targetRect);
		}
		HRESULT STDMETHODCALLTYPE GetUnOccludedWorldShape(const CShape* shape, int depth, CShapePtr* worldShape) const
		{
			DEFINE_INVOKER(CDrawingContext::GetUnOccludedWorldShape);
			return INVOKE_MEMBERFUNCTION(shape, depth, worldShape);
		}
		ULONG_PTR STDMETHODCALLTYPE GetClipBoundsWorld(D2D1_RECT_F* lprc) const
		{
			DEFINE_INVOKER(CDrawingContext::GetClipBoundsWorld);
			return INVOKE_MEMBERFUNCTION(lprc);
		}
		HRESULT STDMETHODCALLTYPE FillShapeWithColor(const CShape* shape, const D2D1_COLOR_F* color)
		{
			DEFINE_INVOKER(CDrawingContext::FillShapeWithColor);
			return INVOKE_MEMBERFUNCTION(shape, color);
		}
		HRESULT STDMETHODCALLTYPE FlushD2D()
		{
			DEFINE_INVOKER(CDrawingContext::FlushD2D);
			return INVOKE_MEMBERFUNCTION();
		}
		CVisual* STDMETHODCALLTYPE GetCurrentVisual() const
		{
			DEFINE_INVOKER(CDrawingContext::GetCurrentVisual);
			return INVOKE_MEMBERFUNCTION();
		}
		bool  STDMETHODCALLTYPE IsInLayer() const
		{
			DEFINE_INVOKER(CDrawingContext::IsInLayer);
			return INVOKE_MEMBERFUNCTION();
		}
		bool  STDMETHODCALLTYPE IsNormalDesktopRender() const
		{
			DEFINE_INVOKER(CDrawingContext::IsNormalDesktopRender);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	struct COcclusionContext : CResource
	{
		CVisual* GetVisual() const
		{
			CVisual* visual{ nullptr };

			if (os::buildNumber < os::build_w10_2004)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[6];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[1];
			}

			return visual;
		}
	};
	struct CDirtyRegion : COcclusionContext
	{
		void STDMETHODCALLTYPE SetFullDirty()
		{
			DEFINE_INVOKER(CDirtyRegion::SetFullDirty);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	struct CWindowOcclusionInfo : CResource {};
	struct CWindowNode : CResource
	{
		HWND STDMETHODCALLTYPE GetHwnd() const
		{
			DEFINE_INVOKER(CWindowNode::GetHwnd);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	struct CDrawListCache : CResource {};


	FORCEINLINE ULONGLONG STDMETHODCALLTYPE GetCurrentFrameId()
	{
		DEFINE_INVOKER(GetCurrentFrameId);
		return INVOKE_FUNCTION();
	}
	FORCEINLINE float STDMETHODCALLTYPE scRGBTosRGB(float channel)
	{
		DEFINE_INVOKER(scRGBTosRGB);
		return INVOKE_FUNCTION(channel);
	}
	FORCEINLINE D2D1_COLOR_F STDMETHODCALLTYPE Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB(const D2D1_COLOR_F& color)
	{
		DEFINE_INVOKER(Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB);
		return INVOKE_FUNCTION(color);
	}
	FORCEINLINE D2D1_COLOR_F STDMETHODCALLTYPE Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB(const D2D1_COLOR_F& color)
	{
		DEFINE_INVOKER(Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB);
		return INVOKE_FUNCTION(color);
	}
	FORCEINLINE UINT32 STDMETHODCALLTYPE PixelAlign(float value, UINT mode = 0)
	{
		DEFINE_INVOKER(PixelAlign);
		return INVOKE_FUNCTION(value, mode);
	}

	namespace CCommonRegistryData
	{
		inline PULONG m_dwOverlayTestMode{ nullptr };
	}

	inline bool OnSymbolParsing(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset, const PSYMBOL_INFO /*originalSymInfo*/)
	{
		if (fullyUnDecoratedFunctionName == "CCommonRegistryData::m_dwOverlayTestMode")
		{
			offset.To(g_moduleHandle, CCommonRegistryData::m_dwOverlayTestMode);
		}
		if (
			fullyUnDecoratedFunctionName.starts_with("CArrayBasedCoverageSet::") ||
			fullyUnDecoratedFunctionName.starts_with("CZOrderedRect::") ||
			fullyUnDecoratedFunctionName == "GetCurrentFrameId" ||
			fullyUnDecoratedFunctionName == "scRGBTosRGB" ||
			fullyUnDecoratedFunctionName == "PixelAlign" ||
			fullyUnDecoratedFunctionName == "Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB" ||
			fullyUnDecoratedFunctionName == "Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB" ||
			fullyUnDecoratedFunctionName == "CChannel::MatrixTransformUpdate" ||
			fullyUnDecoratedFunctionName == "CChannel::CombinedGeometryUpdate" ||
			fullyUnDecoratedFunctionName == "CResource::GetOwningProcessId" ||
			fullyUnDecoratedFunctionName == "CRenderData::TryDrawCommandAsDrawList" ||
			fullyUnDecoratedFunctionName == "CGeometry::GetShapeData" ||
			fullyUnDecoratedFunctionName == "CGeometry::~CGeometry" ||
			fullyUnDecoratedFunctionName == "CDirtyRegion::SetFullDirty" ||
			fullyUnDecoratedFunctionName == "CDirtyRegion::_Add" ||
			fullyUnDecoratedFunctionName == "CVisual::GetHwnd" ||
			fullyUnDecoratedFunctionName == "CD2DContext::GetClip" ||
			fullyUnDecoratedFunctionName == "CSolidColorLegacyMilBrush::IsOfType" ||
			(
				fullyUnDecoratedFunctionName.starts_with("CDrawingContext::") &&
				fullyUnDecoratedFunctionName != "CDrawingContext::IsOccluded"
			) ||
			functionName == "?IsOccluded@CDrawingContext@@QEBA_NAEBV?$TMilRect_@MUMilRectF@@UMil3DRectF@@UMilPointAndSizeF@@UNotNeeded@RectUniqueness@@@@H@Z" ||
			functionName == "??_7CSolidColorLegacyMilBrush@@6B@"
		)
		{
			g_symbolMap.insert_or_assign(
				std::string{ fullyUnDecoratedFunctionName },
				offset.To(g_moduleHandle)
			);
		}

		return true;
	}
}