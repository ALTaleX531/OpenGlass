#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "Utils.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::dwmcore
{
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"dwmcore.dll") };
	inline std::unordered_map<std::string, PVOID> g_symbolMap{};
	template <typename T>
	FORCEINLINE T GetAddressFromSymbolMap(std::string_view functionName)
	{
		auto it{ g_symbolMap.find(std::string{ functionName }) };
		return it != g_symbolMap.end() ? Utils::cast_pointer<T>(it->second) : nullptr;
	}
	template <typename T>
	FORCEINLINE void GetAddressFromSymbolMap(std::string_view functionName, T& target)
	{
		auto it{ g_symbolMap.find(std::string{ functionName }) };
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

	/*namespace FastRegion
	{
		namespace Internal
		{
			struct CRgnData
			{
				HRESULT STDMETHODCALLTYPE Offset(int x, int y)
				{
					DEFINE_INVOKER(FastRegion::Internal::CRgnData::Offset);
					return INVOKE_MEMBERFUNCTION(x, y);
				}
				bool STDMETHODCALLTYPE IsEqualTo(const CRgnData& data)
				{
					DEFINE_INVOKER(FastRegion::Internal::CRgnData::IsEqualTo);
					return INVOKE_MEMBERFUNCTION(data);
				}
			};
		};
		struct CRegion
		{
			PVOID m_reserved[10];
			
			Internal::CRgnData* GetRgnData() const
			{
				return reinterpret_cast<Internal::CRgnData* const*>(this)[0];
			}
			HRESULT STDMETHODCALLTYPE Union(const CRegion& region)
			{
				DEFINE_INVOKER(FastRegion::CRegion::Union);
				return INVOKE_MEMBERFUNCTION(region);
			}
			HRESULT STDMETHODCALLTYPE Subtract(const CRegion& region)
			{
				DEFINE_INVOKER(FastRegion::CRegion::Subtract);
				return INVOKE_MEMBERFUNCTION(region);
			}
			HRESULT STDMETHODCALLTYPE Intersect(const CRegion& region)
			{
				DEFINE_INVOKER(FastRegion::CRegion::Intersect);
				return INVOKE_MEMBERFUNCTION(region);
			}
			bool STDMETHODCALLTYPE Contains(const CRegion& region)
			{
				DEFINE_INVOKER(FastRegion::CRegion::Contains);
				return INVOKE_MEMBERFUNCTION(region);
			}
			// return false if empty
			bool STDMETHODCALLTYPE GetBoundingRect(RECT& rect) const
			{
				DEFINE_INVOKER(FastRegion::CRegion::GetBoundingRect);
				return INVOKE_MEMBERFUNCTION(rect);
			}
			size_t STDMETHODCALLTYPE GetRectangleCount() const
			{
				DEFINE_INVOKER(FastRegion::CRegion::GetRectangleCount);
				return INVOKE_MEMBERFUNCTION();
			}
			void STDMETHODCALLTYPE FreeMemory()
			{
				DEFINE_INVOKER(FastRegion::CRegion::FreeMemory);
				return INVOKE_MEMBERFUNCTION();
			}
			FastRegion::CRegion* STDMETHODCALLTYPE Constructor(const RECT& rect)
			{
				DEFINE_USER_INVOKER(FastRegion::CRegion::Constructor, "FastRegion::CRegion::CRegion");
				return INVOKE_MEMBERFUNCTION(rect);
			}

			CRegion() = delete;
			CRegion(const RECT& rect) { Constructor(rect); }
			~CRegion() { FastRegion::CRegion::FreeMemory(); }
		};
	}
	struct CRegion : FastRegion::CRegion
	{
		void STDMETHODCALLTYPE SetHRGN(HRGN region)
		{
			DEFINE_INVOKER(CRegion::SetHRGN);
			return INVOKE_MEMBERFUNCTION(region);
		}
		HRESULT STDMETHODCALLTYPE CreateHRGN(HRGN* region) const
		{
			DEFINE_INVOKER(CRegion::CreateHRGN);
			return INVOKE_MEMBERFUNCTION(region);
		}
		void STDMETHODCALLTYPE Copy(const CRegion& region)
		{
			DEFINE_INVOKER(CRegion::Copy);
			return INVOKE_MEMBERFUNCTION(region);
		}
		size_t STDMETHODCALLTYPE GetRectangles(std::vector<RECT>* rectangles) const
		{
			DEFINE_INVOKER(CRegion::GetRectangles);
			return INVOKE_MEMBERFUNCTION(rectangles);
		}
		std::vector<RECT> GetRectanglesVector() const
		{
			std::vector<RECT> rectangles{};
			GetRectangles(&rectangles);
			return rectangles;
		}

		CRegion(const RECT& rect) : FastRegion::CRegion{ rect } {}
		CRegion(const CRegion& region) = delete;
		CRegion(CRegion&& region) = delete;
		CRegion() = delete;
		const CRegion& operator=(const CRegion& region) = delete;
	};*/

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
		HRESULT STDMETHODCALLTYPE DuplicateSharedResource(HANDLE handle, UINT type, UINT* handleIndex)
		{
			DEFINE_INVOKER(CChannel::DuplicateSharedResource);
			return INVOKE_MEMBERFUNCTION(handle, type, handleIndex);
		}
		HRESULT STDMETHODCALLTYPE MatrixTransformUpdate(UINT handleIndex, MilMatrix3x2D* matrix)
		{
			DEFINE_INVOKER(CChannel::MatrixTransformUpdate);
			return INVOKE_MEMBERFUNCTION(handleIndex, matrix);
		}
	};
	namespace CCommonRegistryData
	{
		inline PULONGLONG m_backdropBlurCachingThrottleQPCTimeDelta{ nullptr };
	}

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
			auto memory{ HeapAlloc(OpenGlass::Utils::g_processHeap, 0, size) };
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
			auto newSize{ this->size + 1u };
			if (newSize < this->size)
			{
				FAIL_FAST_HR(static_cast<HRESULT>(0x80070216ul));
			}
			else
			{
				auto bufferSize{ this->size * sizeof(T) };
				if (newSize > this->capacity)
				{
					auto tmp{ std::unique_ptr<T[]>(this->data)};

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
	struct CWindowBackgroundTreatment : CResource {};
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
	struct EffectInput : CResource {};
	struct IDeviceTarget: CResource {};
	struct ID2DContextOwner : CResource {};
	struct CD2DContext : CResource
	{
		ID2D1DeviceContext* GetDeviceContext() const
		{
			return reinterpret_cast<ID2D1DeviceContext* const*>(this)[30];
		}
		HRESULT STDMETHODCALLTYPE FillEffect(
			const ID2DContextOwner* contextOwner,
			ID2D1Effect* effect,
			const D2D_RECT_F* lprc,
			const D2D_POINT_2F* point,
			D2D1_INTERPOLATION_MODE interpolationMode,
			D2D1_COMPOSITE_MODE compositeMode
		)
		{
			DEFINE_INVOKER(CD2DContext::FillEffect);
			return INVOKE_MEMBERFUNCTION(contextOwner, effect, lprc, point, interpolationMode, compositeMode);
		}
	};
	struct CDrawingContext : CResource
	{
		CD2DContext* GetD2DContext() const
		{
			if (os::buildNumber < os::build_w10_2004)
			{
				return reinterpret_cast<CD2DContext* const*>(this)[48];
			}
			return reinterpret_cast<CD2DContext*>(reinterpret_cast<ULONG_PTR const*>(this)[5] + 16);
		}
		ID2DContextOwner* GetD2DContextOwner() const
		{
			if (os::buildNumber < os::build_w10_2004)
			{
				return reinterpret_cast<ID2DContextOwner* const*>(this)[8];
			}
			return reinterpret_cast<ID2DContextOwner*>(reinterpret_cast<ULONG_PTR const>(this) + 24);
		}
		bool STDMETHODCALLTYPE IsOccluded(const D2D1_RECT_F& lprc, int flag) const
		{
			DEFINE_INVOKER(CDrawingContext::IsOccluded);
			return INVOKE_MEMBERFUNCTION(lprc, flag);
		}
		CVisual* STDMETHODCALLTYPE GetCurrentVisual() const
		{
			DEFINE_INVOKER(CDrawingContext::GetCurrentVisual);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE GetClipBoundsWorld(D2D1_RECT_F& lprc) const
		{
			DEFINE_INVOKER(CDrawingContext::GetClipBoundsWorld);
			return INVOKE_MEMBERFUNCTION(lprc);
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
		HRESULT STDMETHODCALLTYPE PostSubgraph(CVisualTree* visualTree, bool* unknown)
		{
			DEFINE_INVOKER(COcclusionContext::PostSubgraph);
			return INVOKE_MEMBERFUNCTION(visualTree, unknown);
		}
	};
	struct CCustomBlur : CResource 
	{
		ID2D1DeviceContext* GetDeviceContext() const
		{
			return reinterpret_cast<ID2D1DeviceContext* const*>(this)[2];
		}
		ID2D1Effect* GetCropEffect() const
		{
			return reinterpret_cast<ID2D1Effect* const*>(this)[3];
		}
		ID2D1Effect* GetBorderEffect() const
		{
			return reinterpret_cast<ID2D1Effect* const*>(this)[4];
		}
		ID2D1Effect* GetScaleEffect() const
		{
			return reinterpret_cast<ID2D1Effect* const*>(this)[5];
		}
		ID2D1Effect* GetDirectionalBlurXEffect() const
		{
			return reinterpret_cast<ID2D1Effect* const*>(this)[6];
		}
		ID2D1Effect* GetDirectionalBlurYEffect() const
		{
			return reinterpret_cast<ID2D1Effect* const*>(this)[7];
		}
		void STDMETHODCALLTYPE Reset()
		{
			DEFINE_INVOKER(CCustomBlur::Reset);
			return INVOKE_MEMBERFUNCTION();
		}
		static HRESULT STDMETHODCALLTYPE Create(ID2D1DeviceContext* deviceContext, CCustomBlur** customBlur)
		{
			DEFINE_INVOKER(CCustomBlur::Create);
			return INVOKE_FUNCTION(deviceContext, customBlur);
		}
		static float STDMETHODCALLTYPE DetermineOutputScale(
			float size,
			float blurAmount,
			D2D1_GAUSSIANBLUR_OPTIMIZATION optimization
		)
		{
			DEFINE_INVOKER(CCustomBlur::DetermineOutputScale);
			return INVOKE_FUNCTION(size, blurAmount, optimization);
		}
	};
	struct CDrawListCache : CResource {};
	struct CDrawListBrush : CResource {};
	struct CBrush : CResource {};
	struct CBrushRenderingGraph : CResource {};
	struct CShape : CResource {};
	struct CWindowOcclusionInfo : CResource {};
	struct IBitmapResource : CResource {};
	struct CWindowNode : CResource
	{
		HWND STDMETHODCALLTYPE GetHwnd() const
		{
			DEFINE_INVOKER(CWindowNode::GetHwnd);
			return INVOKE_MEMBERFUNCTION();
		}
	};
	inline ULONGLONG GetCurrentFrameId()
	{
		DEFINE_INVOKER(GetCurrentFrameId);
		return INVOKE_FUNCTION();
	}

	inline bool OnSymbolParsing(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset, const PSYMBOL_INFO /*originalSymInfo*/)
	{
		if (fullyUnDecoratedFunctionName == "CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta")
		{
			offset.To(g_moduleHandle, CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta);
		}

		if (
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CArrayBasedCoverageSet::") ||
			fullyUnDecoratedFunctionName.starts_with("CZOrderedRect::") ||
			/*fullyUnDecoratedFunctionName.starts_with("FastRegion::CRegion::") ||
			fullyUnDecoratedFunctionName.starts_with("FastRegion::Internal::CRgnData") ||
			fullyUnDecoratedFunctionName.starts_with("CRegion::") ||*/
			fullyUnDecoratedFunctionName.starts_with("CCustomBlur::") ||
			fullyUnDecoratedFunctionName == "GetCurrentFrameId" ||
			fullyUnDecoratedFunctionName == "CChannel::DuplicateSharedResource" ||
			fullyUnDecoratedFunctionName == "CChannel::MatrixTransformUpdate" ||
			fullyUnDecoratedFunctionName == "CResource::GetOwningProcessId" ||
			fullyUnDecoratedFunctionName == "COcclusionContext::PostSubgraph" ||
			fullyUnDecoratedFunctionName == "CBlurRenderingGraph::DeterminePreScale" ||
			fullyUnDecoratedFunctionName == "CD2DContext::FillEffect" ||
			(
				fullyUnDecoratedFunctionName.starts_with("CDrawingContext::") &&
				fullyUnDecoratedFunctionName != "CDrawingContext::IsOccluded"
				) ||
			functionName == "?IsOccluded@CDrawingContext@@QEBA_NAEBV?$TMilRect_@MUMilRectF@@UMil3DRectF@@UMilPointAndSizeF@@UNotNeeded@RectUniqueness@@@@H@Z"
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