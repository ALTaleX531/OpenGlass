#pragma once
#include <d2d1_1.h>
#include <wil/win32_helpers.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace OpenGlass::RectF
{
	[[nodiscard]] inline D2D1_RECT_F Align(const D2D1_RECT_F& rectangle)
	{
		return
		{
			std::floor(rectangle.left + 0.5f),
			std::floor(rectangle.top + 0.5f),
			std::floor(rectangle.right + 0.5f),
			std::floor(rectangle.bottom + 0.5f)
		};
	}
	[[nodiscard]] inline float PixelAlign(float value, bool upperBound)
	{
		// DWM snaps coordinates already within 1/256 pixel of an integer before
		// applying the outward floor/ceil used for the remaining coordinates.
		constexpr float pixelSnapTolerance = 1.f / 256.f;
		const auto nearestInteger = std::round(value);
		if (std::fabs(value - nearestInteger) <= pixelSnapTolerance)
		{
			return nearestInteger;
		}
		return upperBound ? std::ceil(value) : std::floor(value);
	}
	[[nodiscard]] inline D2D1_RECT_F PixelAlign(const D2D1_RECT_F& rectangle)
	{
		return
		{
			PixelAlign(rectangle.left, false),
			PixelAlign(rectangle.top, false),
			PixelAlign(rectangle.right, true),
			PixelAlign(rectangle.bottom, true)
		};
	}
	[[nodiscard]] inline D2D1_RECT_F FromRectU(const D2D1_RECT_U& rectangle)
	{
		return
		{
			static_cast<float>(rectangle.left),
			static_cast<float>(rectangle.top),
			static_cast<float>(rectangle.right),
			static_cast<float>(rectangle.bottom)
		};
	}
	[[nodiscard]] inline D2D1_RECT_F FromRectL(const D2D1_RECT_L& rectangle)
	{
		return
		{
			static_cast<float>(rectangle.left),
			static_cast<float>(rectangle.top),
			static_cast<float>(rectangle.right),
			static_cast<float>(rectangle.bottom)
		};
	}
	[[nodiscard]] inline D2D1_RECT_U ToRectU(const D2D1_RECT_F& rectangle)
	{
		return
		{
			static_cast<UINT32>(std::floor(rectangle.left)),
			static_cast<UINT32>(std::floor(rectangle.top)),
			static_cast<UINT32>(std::ceil(rectangle.right)),
			static_cast<UINT32>(std::ceil(rectangle.bottom))
		};
	}
	[[nodiscard]] inline D2D1_RECT_L ToRectL(const D2D1_RECT_F& rectangle)
	{
		return
		{
			static_cast<LONG>(std::floor(rectangle.left)),
			static_cast<LONG>(std::floor(rectangle.top)),
			static_cast<LONG>(std::ceil(rectangle.right)),
			static_cast<LONG>(std::ceil(rectangle.bottom))
		};
	}
	[[nodiscard]] inline D2D1_RECT_F TransformRect(const D2D1_RECT_F& rectangle, const D2D1_MATRIX_3X2_F& matrix)
	{
		const auto matrixHelper = D2D1::Matrix3x2F::ReinterpretBaseType(&matrix);
		const auto topLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.top));
		const auto topRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.top));
		const auto bottomLeft = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.left, rectangle.bottom));
		const auto bottomRight = matrixHelper->TransformPoint(D2D1::Point2F(rectangle.right, rectangle.bottom));

		D2D1_RECT_F transformedRect{};
		transformedRect.left = (std::min)({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
		transformedRect.top = (std::min)({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });
		transformedRect.right = (std::max)({ topLeft.x, topRight.x, bottomLeft.x, bottomRight.x });
		transformedRect.bottom = (std::max)({ topLeft.y, topRight.y, bottomLeft.y, bottomRight.y });

		return transformedRect;
	}
	[[nodiscard]] inline D2D1_RECT_F Transform2DBounds(
		const D2D1_RECT_F& rectangle,
		const D2D1_MATRIX_4X4_F& matrix
	)
	{
		// Mirrors CMILMatrix::Transform2DBoundsHelper<0> for a z=0 rectangle.
		constexpr float matrixEpsilon = 0.000081380211f;
		const auto isNear = [matrixEpsilon](float value, float expected)
		{
			return std::fabs(value - expected) < matrixEpsilon;
		};

		if (
			isNear(matrix._11, 1.f) &&
			isNear(matrix._12, 0.f) &&
			isNear(matrix._14, 0.f) &&
			isNear(matrix._21, 0.f) &&
			isNear(matrix._22, 1.f) &&
			isNear(matrix._24, 0.f) &&
			isNear(matrix._44, 1.f)
		)
		{
			return
			{
				rectangle.left + matrix._41,
				rectangle.top + matrix._42,
				rectangle.right + matrix._41,
				rectangle.bottom + matrix._42
			};
		}

		if (
			isNear(matrix._12, 0.f) &&
			isNear(matrix._14, 0.f) &&
			isNear(matrix._21, 0.f) &&
			isNear(matrix._24, 0.f) &&
			isNear(matrix._44, 1.f)
		)
		{
			const auto left = rectangle.left * matrix._11 + matrix._41;
			const auto top = rectangle.top * matrix._22 + matrix._42;
			const auto right = rectangle.right * matrix._11 + matrix._41;
			const auto bottom = rectangle.bottom * matrix._22 + matrix._42;
			return
			{
				(std::min)(left, right),
				(std::min)(top, bottom),
				(std::max)(left, right),
				(std::max)(top, bottom)
			};
		}

		struct HomogeneousPoint
		{
			float x;
			float y;
			float w;
		};
		const auto transformPoint = [&](float x, float y)
		{
			return HomogeneousPoint
			{
				y * matrix._21 + x * matrix._11 + matrix._41,
				y * matrix._22 + x * matrix._12 + matrix._42,
				y * matrix._24 + x * matrix._14 + matrix._44
			};
		};
		std::array points
		{
			transformPoint(rectangle.left, rectangle.top),
			transformPoint(rectangle.left, rectangle.bottom),
			transformPoint(rectangle.right, rectangle.bottom),
			transformPoint(rectangle.right, rectangle.top)
		};

		if (std::ranges::any_of(points, [&](const auto& point) { return point.w < matrixEpsilon; }))
		{
			if (std::ranges::any_of(points, [&](const auto& point) { return point.w >= -matrixEpsilon; }))
			{
				return D2D1::InfiniteRect();
			}
			return {};
		}

		for (auto& point : points)
		{
			if (!isNear(point.w, 1.f))
			{
				point.x /= point.w;
				point.y /= point.w;
			}
		}

		D2D1_RECT_F transformedRect
		{
			points[0].x,
			points[0].y,
			points[0].x,
			points[0].y
		};
		for (const auto& point : std::span{ points }.subspan(1))
		{
			transformedRect.left = (std::min)(transformedRect.left, point.x);
			transformedRect.top = (std::min)(transformedRect.top, point.y);
			transformedRect.right = (std::max)(transformedRect.right, point.x);
			transformedRect.bottom = (std::max)(transformedRect.bottom, point.y);
		}
		return transformedRect;
	}
	[[nodiscard]] inline D2D1_RECT_F ResolveDeviceBounds(
		const D2D1_RECT_F& rectangle,
		const D2D1_MATRIX_4X4_F& deviceTransform,
		bool rectangleIsDeviceSpace
	)
	{
		if (rectangleIsDeviceSpace)
		{
			return rectangle;
		}
		return PixelAlign(Transform2DBounds(rectangle, deviceTransform));
	}
	inline constexpr bool DoesIntersectUnsafe(
		const D2D1_RECT_F& rc1,
		const D2D1_RECT_F& rc2
	)
	{
		if (
			!wil::rect_is_empty(rc1) &&
			!wil::rect_is_empty(rc2) &&

			rc1.right > rc2.left &&
			rc2.right > rc1.left &&
			rc1.bottom > rc2.top &&
			rc2.bottom > rc1.top
		)
		{
			return true;
		}
		return false;
	}
	inline constexpr bool IntersectUnsafe(
		D2D1_RECT_F& dst,
		const D2D1_RECT_F& src
	)
	{
		dst.left = (std::max)(dst.left, src.left);
		dst.top = (std::max)(dst.top, src.top);
		dst.right = (std::min)(dst.right, src.right);
		dst.bottom = (std::min)(dst.bottom, src.bottom);

		if (!wil::rect_is_empty(dst))
		{
			return true;
		}

		dst.left = dst.top = dst.right = dst.bottom = 0.f;
		return false;
	}
}
