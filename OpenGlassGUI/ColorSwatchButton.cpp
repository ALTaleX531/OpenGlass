#include "pch.h"
#include "ColorSwatchButton.hpp"
#include <wx/msw/dc.h>

namespace OpenGlass
{
	namespace
	{
		constexpr int SwatchSizeDip = 40;

		wxColour GetSurfaceBorder()
		{
			return wxSystemSettings::GetAppearance().IsDark()
				? wxColour(102, 102, 102)
				: wxColour(154, 154, 154);
		}

		wxColour GetSelectionBorder()
		{
			return wxSystemSettings::GetAppearance().IsDark()
				? wxColour(76, 194, 255)
				: wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
		}

		void DrawColorCardFan(wxGraphicsContext& context, const wxRect2DDouble& bounds)
		{
			const bool dark = wxSystemSettings::GetAppearance().IsDark();
			context.PushState();
			context.Translate(bounds.m_x, bounds.m_y);
			context.Scale(bounds.m_width / 40, bounds.m_height / 40);
			const wxColour colors[] = {
				wxColour(83, 145, 213), wxColour(154, 112, 190), wxColour(223, 105, 111),
				wxColour(236, 187, 80), wxColour(100, 177, 135)
			};
			for (size_t index = 0; index < std::size(colors); ++index)
			{
				context.PushState();
				context.Translate(20, 28);
				context.Rotate((static_cast<double>(index) - 2) * 0.4363323129985824); // 25 degrees
				context.SetPen(wxPen(wxColour(255, 255, 255), 1));
				context.SetBrush(wxBrush(colors[index]));
				context.DrawRoundedRectangle(-3.5, -21, 7, 24, 1.2);
				context.PopState();
			}
			context.SetPen(*wxTRANSPARENT_PEN);
			context.SetBrush(wxBrush(dark ? wxColour(221, 221, 221) : wxColour(255, 255, 255)));
			context.DrawEllipse(18, 26, 4, 4);
			context.SetBrush(wxBrush(wxColour(120, 120, 120)));
			context.DrawEllipse(19.2, 27.2, 1.6, 1.6);
			context.PopState();
		}

	}

	void DrawAutomaticColorSwatch(wxGraphicsContext& context, const wxRect2DDouble& bounds)
	{
		// Cache the fixed mode graphic for both buttons and preset thumbnails.
		static const wxImage image = [] {
			constexpr int size = 256;
			wxImage result(size, size, false);
			if (!result.IsOk()) return result;

			auto* pixels = result.GetData();
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
				{
					const double horizontal = static_cast<double>(x) / (size - 1);
					const double vertical = static_cast<double>(y) / (size - 1);
					// Red -> (0, 178, 255), top-left to bottom-right;
					// blue -> green, bottom-left to top-right. Screen: 1 - (1 - F)(1 - B).
					const double foreground = (horizontal + vertical) / 2;
					const double background = (horizontal + 1 - vertical) / 2;
					*pixels++ = static_cast<unsigned char>(255 * (1 - foreground) + 0.5);
					*pixels++ = static_cast<unsigned char>(255 * (1 - (1 - foreground * 178 / 255) * (1 - background)) + 0.5);
					*pixels++ = static_cast<unsigned char>(255 * (1 - (1 - foreground) * background) + 0.5);
				}
			}
			return result;
		}();
		if (!image.IsOk()) return;

		context.DrawBitmap(context.CreateBitmapFromImage(image),
			bounds.m_x, bounds.m_y, bounds.m_width, bounds.m_height);
	}

	ColorSwatchButton::ColorSwatchButton(wxWindow* parent, wxWindowID id, const wxString& label, DWORD argb, bool automatic)
		: wxToggleButton(parent, id, wxEmptyString)
		, m_argb(argb)
		, m_automatic(automatic)
	{
		SetName(label);
		SetToolTip(label);
		// Keep our draw cache separate from SetBitmap()'s erasing refresh.
		MakeOwnerDrawn();
		// MSWOnDraw paints the complete background with the swatch.
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		const wxSize buttonSize = FromDIP(wxSize(SwatchSizeDip, SwatchSizeDip));
		SetMinSize(buttonSize);
		SetMaxSize(buttonSize);
		RebuildBitmap();

		Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& event) {
			Refresh(false);
			event.Skip();
		});
		Bind(wxEVT_DPI_CHANGED, &ColorSwatchButton::OnDpiChanged, this);
		Bind(wxEVT_SYS_COLOUR_CHANGED, &ColorSwatchButton::OnSystemColorChanged, this);
	}

	void ColorSwatchButton::SetColor(DWORD argb)
	{
		const bool rgbChanged = (m_argb & 0xFFFFFF) != (argb & 0xFFFFFF);
		m_argb = argb;
		if (!rgbChanged)
		{
			return;
		}

		RebuildBitmap();
		Refresh(false);
	}

	void ColorSwatchButton::SetValue(bool value)
	{
		if (GetValue() == value)
		{
			return;
		}

		wxToggleButton::SetValue(value);
	}

	bool ColorSwatchButton::MSWOnDraw(WXDRAWITEMSTRUCT* item)
	{
		auto* drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(item);
		if (!drawItem || !drawItem->hDC)
		{
			return false;
		}

		const int width = drawItem->rcItem.right - drawItem->rcItem.left;
		const int height = drawItem->rcItem.bottom - drawItem->rcItem.top;
		if (width <= 0 || height <= 0)
		{
			return true;
		}

		wxBitmap buffer(width, height, 24);
		if (!buffer.IsOk())
		{
			return false;
		}
		wxMemoryDC dc(buffer);
		const bool pressed = (drawItem->itemState & ODS_SELECTED) != 0;
		const bool selected = GetValue() || pressed;
		wxColour background = GetParent()->GetBackgroundColour();
		if (!background.IsOk())
		{
			background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		}
		dc.SetBackground(wxBrush(background));
		dc.Clear();

		const wxBitmap& bitmap = selected ? m_selectedBitmap : m_normalBitmap;
		if (bitmap.IsOk())
		{
			const int x = (width - bitmap.GetWidth()) / 2;
			const int y = (height - bitmap.GetHeight()) / 2;
			dc.DrawBitmap(bitmap, x, y, true);
		}

		if (
			(drawItem->itemState & ODS_FOCUS) != 0
			&& (drawItem->itemState & ODS_NOFOCUSRECT) == 0
		)
		{
			RECT focusRect{ 0, 0, width, height };
			::InflateRect(&focusRect, -FromDIP(selected ? 4 : 2), -FromDIP(selected ? 4 : 2));
			::DrawFocusRect(reinterpret_cast<HDC>(dc.GetHDC()), &focusRect);
		}

		wxDCTemp target(reinterpret_cast<WXHDC>(drawItem->hDC),
			wxSize(drawItem->rcItem.right, drawItem->rcItem.bottom));
		target.Blit(drawItem->rcItem.left, drawItem->rcItem.top, width, height, &dc, 0, 0);
		return true;
	}

	void ColorSwatchButton::RebuildBitmap()
	{
		const wxSize bitmapSize = FromDIP(wxSize(SwatchSizeDip, SwatchSizeDip));
		const wxColour color(
			(m_argb >> 16) & 0xFF,
			(m_argb >> 8) & 0xFF,
			m_argb & 0xFF
		);
		const wxColour light = color.ChangeLightness(175);
		const wxColour dark = color.ChangeLightness(95);
		wxGraphicsGradientStops gradient(light, dark);

		auto createBitmap = [&](const wxColour& border, int borderWidth) {
			wxBitmap bitmap(bitmapSize.GetWidth(), bitmapSize.GetHeight(), 32);
			bitmap.UseAlpha();

			wxMemoryDC memoryDc(bitmap);
			memoryDc.SetBackground(wxBrush(wxColour(0, 0, 0, 0)));
			memoryDc.Clear();

			{
				std::unique_ptr<wxGraphicsContext> context{ wxGraphicsContext::Create(memoryDc) };
				if (context)
				{
					context->SetPen(*wxTRANSPARENT_PEN);
					context->SetBrush(wxBrush(border));
					context->DrawRectangle(0, 0, bitmapSize.GetWidth(), bitmapSize.GetHeight());

					const wxRect2DDouble bounds(borderWidth, borderWidth,
						bitmapSize.GetWidth() - 2 * borderWidth, bitmapSize.GetHeight() - 2 * borderWidth);
					if (m_automatic)
					{
						DrawAutomaticColorSwatch(*context, bounds);
						DrawColorCardFan(*context, bounds);
					}
					else
					{
						context->SetBrush(context->CreateLinearGradientBrush(0, 0,
							bitmapSize.GetWidth(), bitmapSize.GetHeight(), gradient));
						context->DrawRectangle(bounds.m_x, bounds.m_y, bounds.m_width, bounds.m_height);
					}
				}
			}

			memoryDc.SelectObject(wxNullBitmap);
			return bitmap;
		};

		m_surfaceBorder = GetSurfaceBorder();
		m_selectionBorder = GetSelectionBorder();
		m_normalBitmap = createBitmap(m_surfaceBorder, std::max(1, FromDIP(1)));
		m_selectedBitmap = createBitmap(m_selectionBorder, std::max(2, FromDIP(2)));
	}

	void ColorSwatchButton::OnDpiChanged(wxDPIChangedEvent& event)
	{
		const wxSize buttonSize = FromDIP(wxSize(SwatchSizeDip, SwatchSizeDip));
		SetMinSize(buttonSize);
		SetMaxSize(buttonSize);
		RebuildBitmap();
		Refresh(false);
		InvalidateBestSize();
		GetParent()->Layout();
		event.Skip();
	}

	void ColorSwatchButton::OnSystemColorChanged(wxSysColourChangedEvent& event)
	{
		if (m_surfaceBorder != GetSurfaceBorder() || m_selectionBorder != GetSelectionBorder())
		{
			RebuildBitmap();
			Refresh(false);
		}
		event.Skip();
	}
}
