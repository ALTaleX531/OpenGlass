#pragma once
#include "framework.hpp"
#include "cpprt.hpp"

namespace OpenGlass::DWM
{
	// actually gsl::span
	template <typename T>
	struct span
	{
		size_t length;
		T* data;

		auto views() const
		{
			return std::span<T>(data, length);
		}
	};

	template <typename T>
	struct DynArray
	{
		T* data;
		T* initialAllocation;
		UINT allocSize;
		UINT capacity;
		UINT count;

		auto views() const
		{
			return std::span<T>(data, count);
		}
	};

	template <typename T>
	struct MyDynArrayImpl : DynArray<T>
	{
		[[nodiscard]] void* operator new[](
			size_t size
		)
		{
			auto memory = HeapAlloc(OpenGlass::Util::g_processHeap, 0, size);
			THROW_LAST_ERROR_IF_NULL(memory);
			return memory;
		}
		void operator delete[](
			void* ptr
		) noexcept
		{
			FAIL_FAST_IF_NULL(ptr);
			HeapFree(OpenGlass::Util::g_processHeap, 0, ptr);
		}

		MyDynArrayImpl()
		{
			this->capacity = 8;
			this->data = new T[this->capacity];
			this->count = 0;
		}
		~MyDynArrayImpl()
		{
			delete[] this->data;
			this->data = nullptr;
			this->capacity = this->count = 0;
		}

		void Clear()
		{
			if (this->count != 0)
			{
				this->capacity = 8;
				delete[] this->data;
				this->data = new T[this->capacity];
				this->count = 0;
			}
		}
		void Add(const T& object)
		{
			auto newSize = this->count + 1u;
			if (newSize < this->count)
			{
				FAIL_FAST_HR(static_cast<HRESULT>(0x80070216ul));
			}
			else
			{
				auto bufferSize = this->count * sizeof(T);
				if (newSize > this->capacity)
				{
					auto tmp = std::unique_ptr<T[]>(this->data);

					this->capacity *= 2;
					this->data = new T[this->capacity];
					memcpy_s(this->data, bufferSize, tmp.get(), bufferSize);
				}

				*reinterpret_cast<T*>(reinterpret_cast<ULONG_PTR>(this->data) + bufferSize) = object;
				this->count = newSize;
			}
		}
		template <typename... Args>
		void AddInPlace(Args&&... args)
		{
			auto newSize = this->count + 1u;
			if (newSize < this->count)
			{
				FAIL_FAST_HR(static_cast<HRESULT>(0x80070216ul));
			}
			else
			{
				auto bufferSize = this->count * sizeof(T);
				if (newSize > this->capacity)
				{
					auto tmp = std::unique_ptr<T[]>(this->data);

					this->capacity *= 2;
					this->data = new T[this->capacity];
					memcpy_s(this->data, bufferSize, tmp.get(), bufferSize);
				}

				*reinterpret_cast<T*>(reinterpret_cast<ULONG_PTR>(this->data) + bufferSize) = T{ std::forward<Args>(args)... };
				this->count = newSize;
			}
		}
	};

	struct MilPoint2D
	{
		double x;
		double y;
	};
	struct MilPoint3F
	{
		FLOAT x;
		FLOAT y;
		FLOAT z;
	};
	struct MilRectF
	{
		float x;
		float y;
		float width;
		float height;
	};
	struct MilRectD
	{
		double left;
		double top;
		double right;
		double bottom;
	};
	struct MilSizeD
	{
		double width;
		double height;
	};
	enum DwmResourceType : UINT
	{
		ImageLegacyMilBrushProxy = 17,
		SolidColorLegacyMilBrushProxy = 34,
	};
	enum MIL_RESOURCE_TYPE : UINT {};
	enum class MilBrushMappingMode
	{
		Absolute,
		RelativeToBoundingBox
	};
	enum class MilBitmapInterpolationMode
	{
		NearestNeighbor,
		Linear,
		Cubic,
		Fant,
		TriLinear,
		Anisotropic,
		SuperSample
	};
	enum class MilStretch
	{
		None,
		Fill,
		Uniform,
		UniformToFill
	};
	enum class MilTileMode
	{
		Extend
	};
	enum class MilHorizontalAlignment
	{
		Left,
		Center,
		Right
	};
	enum class MilVerticalAlignment
	{
		Top,
		Center,
		Bottom
	};
	struct MilColorTransform
	{
		union
#pragma warning(suppress : 4201)
		{
			struct
#pragma warning(suppress : 4201)
			{
				FLOAT _11, _12, _13, _14, _15;
				FLOAT _21, _22, _23, _24, _25;
				FLOAT _31, _32, _33, _34, _35;
				FLOAT _41, _42, _43, _44, _45;
				FLOAT _51, _52, _53, _54, _55;
			} DUMMYSTRUCTNAME;

			FLOAT m[5][5];
		} DUMMYUNIONNAME;
	};
	using MilTransform = D2D1_MATRIX_3X2_F;
	using MilTransform3D = D2D1_MATRIX_4X3_F;

	using HMIL_CONNECTION = HANDLE;
	struct IDwmChannel {};
	struct IDwmWindow {};
}
namespace OpenGlass::dwmcore
{
	using namespace DWM;
	struct CChannel : IDwmChannel
	{
		inline HRESULT MatrixTransformUpdate(
			UINT handleId,
			MilMatrix3x2D* matrix
		);
		inline HRESULT SolidColorLegacyMilBrushUpdate(
			UINT handleId,
			double opacity,
			const D2D1_COLOR_F& color,
			UINT opacityAnimationsHandleId,
			UINT transformHandleId,
			UINT relativeTransformHandleId
		);
		inline HRESULT ImageLegacyMilBrushUpdate(
			UINT handleId,
			double opacity,
			const D2D1_RECT_F& viewport,
			const D2D1_RECT_F& viewbox,
			UINT opacityAnimationsHandleId,
			UINT transformHandleId,
			UINT relativeTransformHandleId,
			MilBrushMappingMode viewportUnits,
			MilBrushMappingMode viewboxUnits,
			UINT viewportAnimationsHandleId,
			UINT viewboxAnimationsHandleId,
			MilStretch stretchMode,
			MilTileMode tileMode,
			MilHorizontalAlignment alignmentX,
			MilVerticalAlignment alignmentY,
			UINT imageSourceHandleId
		);
	};
}
