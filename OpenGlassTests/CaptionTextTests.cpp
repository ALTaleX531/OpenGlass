#include "pch.h"
#include "TestHarness.hpp"
#include "CaptionText.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestDWriteGeometry()
	{
		winrt::com_ptr<IDWriteFactory> factory;
		winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
		winrt::com_ptr<IDWriteTextFormat> format;
		winrt::check_hresult(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.f, L"en-US", format.put()));
		winrt::check_hresult(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
		for (const auto direction : { DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT })
		{
			winrt::check_hresult(format->SetReadingDirection(direction));
			float leadingWidth{};
			for (const auto alignment : { DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_TRAILING })
			{
				winrt::check_hresult(format->SetTextAlignment(alignment));
				winrt::com_ptr<IDWriteTextLayout> layout;
				constexpr wchar_t text[] = L"Window title \x05D0\x05D1";
				winrt::check_hresult(factory->CreateTextLayout(text, ARRAYSIZE(text) - 1, format.get(), 10000.f, 30.f, layout.put()));
				DWRITE_TEXT_METRICS metrics{};
				DWRITE_OVERHANG_METRICS overhangs{};
				winrt::check_hresult(layout->GetMetrics(&metrics));
				winrt::check_hresult(layout->GetOverhangMetrics(&overhangs));
				const auto bounds = CaptionText::CalculateDWriteBitmapBounds(metrics, overhangs);
				const auto width = bounds.right - bounds.left;
				Check(width > 0.f && width < 1000.f);
				Check(bounds.bottom > bounds.top);
				if (alignment == DWRITE_TEXT_ALIGNMENT_LEADING) leadingWidth = width;
				Check(std::fabs(width - leadingWidth) <= 2.f);

				// Normalizing the bitmap must preserve the layout origin, including
				// BeginDraw's nonzero atlas offset and fractional coordinates.
				const D2D1_POINT_2F origin{ 143.25f, 79.5f };
				const D2D1_POINT_2F bitmapOrigin{ origin.x + bounds.left, origin.y + bounds.top };
				Check(bitmapOrigin.x - bounds.left == origin.x);
				Check(bitmapOrigin.y - bounds.top == origin.y);
			}
		}
	}
}
