#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
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
	struct ID2DContextOwner : CResource {};
	struct CD2DContext : CResource
	{
		ID2D1DeviceContext* GetDeviceContext() const
		{
			return reinterpret_cast<ID2D1DeviceContext* const*>(this)[30];
		}
	};
	struct CDrawingContext : CResource
	{
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
	};
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
		if (
			fullyUnDecoratedFunctionName.starts_with("CArrayBasedCoverageSet::") ||
			fullyUnDecoratedFunctionName.starts_with("CZOrderedRect::") ||
			fullyUnDecoratedFunctionName == "GetCurrentFrameId" ||
			fullyUnDecoratedFunctionName == "CChannel::MatrixTransformUpdate" ||
			fullyUnDecoratedFunctionName == "CResource::GetOwningProcessId" ||
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