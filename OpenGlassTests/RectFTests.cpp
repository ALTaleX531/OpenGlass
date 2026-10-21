#include "pch.h"
#include "TestHarness.hpp"
#include "RectF.hpp"

using namespace OpenGlass;

namespace
{
	bool RectNear(const D2D1_RECT_F& actual, const D2D1_RECT_F& expected)
	{
		constexpr float epsilon = 0.0001f;
		return
			std::fabs(actual.left - expected.left) < epsilon &&
			std::fabs(actual.top - expected.top) < epsilon &&
			std::fabs(actual.right - expected.right) < epsilon &&
			std::fabs(actual.bottom - expected.bottom) < epsilon;
	}
}

namespace OpenGlassTests
{
	void TestPixelAlign()
	{
		Check(RectNear(
			RectF::PixelAlign({ 10.999f, 20.001f, 30.001f, 40.999f }),
			{ 11.f, 20.f, 30.f, 41.f }
		));
		Check(RectNear(
			RectF::PixelAlign({ -30.001f, -40.999f, -10.999f, -20.001f }),
			{ -30.f, -41.f, -11.f, -20.f }
		));

		const D2D1_RECT_F deviceSpaceRectangle{ 10.999f, 20.001f, 30.001f, 40.999f };
		const auto identity = D2D1::Matrix4x4F{};
		Check(RectNear(
			RectF::ResolveDeviceBounds(deviceSpaceRectangle, identity, true),
			deviceSpaceRectangle
		));
		Check(RectNear(
			RectF::ResolveDeviceBounds(deviceSpaceRectangle, identity, false),
			{ 11.f, 20.f, 30.f, 41.f }
		));

		constexpr float pixelSnapTolerance = 1.f / 256.f;
		Check(RectNear(
			RectF::PixelAlign(
				{
					10.f + pixelSnapTolerance,
					20.f + pixelSnapTolerance,
					30.f - pixelSnapTolerance,
					40.f - pixelSnapTolerance
				}
			),
			{ 10.f, 20.f, 30.f, 40.f }
		));
		Check(RectNear(
			RectF::PixelAlign(
				{
					10.f + 2.f * pixelSnapTolerance,
					20.f + 2.f * pixelSnapTolerance,
					30.f - 2.f * pixelSnapTolerance,
					40.f - 2.f * pixelSnapTolerance
				}
			),
			{ 10.f, 20.f, 30.f, 40.f }
		));
	}

	void TestTransform2DBounds()
	{
		const D2D1_RECT_F rectangle{ 1.f, 2.f, 4.f, 6.f };

		auto translation = D2D1::Matrix4x4F{};
		translation._41 = 3.25f;
		translation._42 = -1.5f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, translation),
			{ 4.25f, 0.5f, 7.25f, 4.5f }
		));

		auto negativeScale = D2D1::Matrix4x4F{};
		negativeScale._11 = -2.f;
		negativeScale._22 = -3.f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, negativeScale),
			{ -8.f, -18.f, -2.f, -6.f }
		));

		auto rotation = D2D1::Matrix4x4F{};
		rotation._11 = 0.f;
		rotation._12 = 1.f;
		rotation._21 = -1.f;
		rotation._22 = 0.f;
		Check(RectNear(
			RectF::Transform2DBounds(rectangle, rotation),
			{ -6.f, 1.f, -2.f, 4.f }
		));

		auto perspective = D2D1::Matrix4x4F{};
		perspective._14 = 0.1f;
		Check(RectNear(
			RectF::Transform2DBounds({ 0.f, 0.f, 2.f, 1.f }, perspective),
			{ 0.f, 0.f, 5.f / 3.f, 1.f }
		));

		auto crossesProjectionPlane = D2D1::Matrix4x4F{};
		crossesProjectionPlane._14 = 1.f;
		crossesProjectionPlane._44 = -1.f;
		Check(RectNear(
			RectF::Transform2DBounds({ 0.f, 0.f, 2.f, 1.f }, crossesProjectionPlane),
			D2D1::InfiniteRect()
		));

		auto behindProjectionPlane = D2D1::Matrix4x4F{};
		behindProjectionPlane._44 = -1.f;
		Check(wil::rect_is_empty(RectF::Transform2DBounds(rectangle, behindProjectionPlane)));
	}
}
