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
		HRESULT STDMETHODCALLTYPE RedirectVisualSetRedirectedVisual(UINT redirectVisualHandleIndex, UINT visualHandleIndex)
		{
			DEFINE_INVOKER(CChannel::RedirectVisualSetRedirectedVisual);
			return INVOKE_MEMBERFUNCTION(redirectVisualHandleIndex, visualHandleIndex);
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
			return reinterpret_cast<DynArray<CZOrderedRect>*>(const_cast<CArrayBasedCoverageSet*>(this + 49));
		}
		DynArray<CZOrderedRect>* GetOccluderArray() const
		{
			return reinterpret_cast<DynArray<CZOrderedRect>*>(const_cast<CArrayBasedCoverageSet*>(this));
		}
	};
	struct CVisualTree : CResource {};
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
	};
	struct EffectInput : CResource {};
	struct IDeviceTarget: CResource {};
	struct CDrawingContext : CResource
	{
		CDrawingContext* GetParentDrawingContext() const
		{
			return const_cast<CDrawingContext*>(this) + 3;
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
			return reinterpret_cast<CVisual* const*>(this)[1];
		}
		HRESULT STDMETHODCALLTYPE PostSubgraph(CVisualTree* visualTree, bool* unknown)
		{
			DEFINE_INVOKER(COcclusionContext::PostSubgraph);
			return INVOKE_MEMBERFUNCTION(visualTree, unknown);
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

	inline bool OnSymbolParsing(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset, const PSYMBOL_INFO originalSymInfo)
	{
		if (fullyUnDecoratedFunctionName == "CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta")
		{
			offset.To(g_moduleHandle, CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta);
		}

		if (
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CArrayBasedCoverageSet::") ||
			fullyUnDecoratedFunctionName.starts_with("CZOrderedRect::") ||
			fullyUnDecoratedFunctionName.starts_with("FastRegion::CRegion::") ||
			fullyUnDecoratedFunctionName.starts_with("FastRegion::Internal::CRgnData") ||
			fullyUnDecoratedFunctionName.starts_with("CRegion::") ||
			fullyUnDecoratedFunctionName == "GetCurrentFrameId" ||
			fullyUnDecoratedFunctionName == "CChannel::RedirectVisualSetRedirectedVisual" ||
			fullyUnDecoratedFunctionName == "CResource::GetOwningProcessId" ||
			fullyUnDecoratedFunctionName == "COcclusionContext::PostSubgraph"
#ifdef _DEBUG
			||
			fullyUnDecoratedFunctionName == "CBrushRenderingGraph::RenderSubgraphs" ||
			fullyUnDecoratedFunctionName == "CBrush::GenerateDrawList" ||
			fullyUnDecoratedFunctionName == "CWindowNode::RenderImage" ||
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CWindowNode::") ||
			(
				fullyUnDecoratedFunctionName.starts_with("CDrawingContext::") &&
				fullyUnDecoratedFunctionName != "CDrawingContext::IsOccluded"
			) ||
			functionName == "?IsOccluded@CDrawingContext@@QEBA_NAEBV?$TMilRect_@MUMilRectF@@UMil3DRectF@@UMilPointAndSizeF@@UNotNeeded@RectUniqueness@@@@H@Z"
#endif
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