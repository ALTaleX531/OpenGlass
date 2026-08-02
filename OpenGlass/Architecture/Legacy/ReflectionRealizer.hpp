#pragma once
#include "resource.h"
#include "framework.hpp"
#include "cpprt.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass
{
	struct ReflectionContext
	{
		const dwmcore::CMILMatrix* worldTransform;
		const D2D1_RECT_F* viewport;
		float opacity;
	};

	class CReflectionRealizer
	{
		winrt::com_ptr<ID2D1Bitmap1> m_reflectionBitmap{ nullptr };
		winrt::com_ptr<ID2D1SpriteBatch> m_spriteBatch{ nullptr };

		inline HRESULT LoadTexture(ID2D1DeviceContext* context);
	public:
		HRESULT Render(
			ID2D1DeviceContext* context,
			const std::span<const D2D1_RECT_F>& rectangles,
			const ReflectionContext& reflectionContext
		);
		void Reset()
		{
			m_reflectionBitmap = nullptr;
		}
	};
}
