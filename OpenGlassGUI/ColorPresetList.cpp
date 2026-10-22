#include "pch.h"
#include "ColorPresetList.hpp"
#include <wx/wupdlock.h>

namespace OpenGlass
{
	namespace
	{
		constexpr int SwatchSizeDip = 40;
		constexpr int SwatchTopPaddingDip = 10;
		constexpr int SwatchBottomPaddingDip = 2;

		wxColour GetSurfaceBorder()
		{
			return wxSystemSettings::GetAppearance().IsDark()
				? wxColour(102, 102, 102)
				: wxColour(154, 154, 154);
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

	wxDEFINE_EVENT(wxEVT_COLOR_PRESET_INVOKED, wxCommandEvent);
	wxDEFINE_EVENT(wxEVT_CUSTOM_COLOR_EDIT_REQUESTED, wxCommandEvent);

	ColorPresetList::ColorPresetList(wxWindow* parent)
		: wxListView(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxLC_ICON | wxLC_SINGLE_SEL | wxLC_AUTOARRANGE | wxLC_ALIGN_TOP | wxBORDER_NONE)
	{
		SetName(L"Color presets");
		const auto hwnd = static_cast<HWND>(GetHandle());
		// The containing page scrolls; the palette grows to fit its wrapped rows.
		SetWindowLongPtrW(hwnd, GWL_STYLE, GetWindowLongPtrW(hwnd, GWL_STYLE) | LVS_NOSCROLL);
		ListView_SetExtendedListViewStyleEx(hwnd, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
		SetAutomaticDescription(L"Automatic accent color from the wallpaper");
		UpdateAppearance();
		SetFamily(ColorizationPresets::Family::Vista);
		Bind(wxEVT_KEY_DOWN, &ColorPresetList::OnKeyDown, this);
		Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
			const auto selected = GetFirstSelected();
			if (selected >= 0) Focus(selected);
			event.Skip();
		});
		Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { UpdateLayout(); event.Skip(); });
		Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
			RebuildImages();
			UpdateLayout();
			event.Skip();
		});
		Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
			event.Skip();
			// Apply after wxWidgets has updated the native control's theme.
			CallAfter([this] { UpdateAppearance(); RebuildImages(); });
		});
	}

	void ColorPresetList::SetFamily(ColorizationPresets::Family family)
	{
		const auto presets = ColorizationPresets::Get(family);
		if (m_presets.data() == presets.data()) return;
		wxWindowUpdateLocker lock(this);
		m_presets = presets;
		m_currentSelection = -1;
		m_preferCustomSelection = false;
		DeleteAllItems();
		InsertItem(0, L"Automatic", 0);
		for (size_t index = 0; index < m_presets.size(); ++index)
		{
			const auto& preset = m_presets[index];
			InsertItem(static_cast<long>(index) + 1, wxString(preset.name.data(), preset.name.size()), static_cast<int>(index) + 1);
		}
		InsertItem(GetCustomIndex(), L"Custom", static_cast<int>(GetCustomIndex()));
		RebuildImages();
		Focus(0);
		UpdateLayout();
	}

	const ColorizationPresets::Preset* ColorPresetList::GetPreset(long index) const noexcept
	{
		return index > 0 && index < GetCustomIndex() ? &m_presets[index - 1] : nullptr;
	}

	void ColorPresetList::SetCustomColor(DWORD argb)
	{
		const bool rgbChanged = (m_customColor & 0xFFFFFF) != (argb & 0xFFFFFF);
		m_customColor = argb;
		if (!rgbChanged) return;
		if (auto* images = GetImageList(wxIMAGE_LIST_NORMAL))
		{
			images->Replace(static_cast<int>(GetCustomIndex()), CreateSwatch(argb, false));
			RefreshItem(GetCustomIndex());
		}
	}

	void ColorPresetList::SetCurrentSelection(long index)
	{
		// Custom and a built-in swatch can represent the same configuration.
		// Preserve an explicit Custom choice only while they remain equivalent;
		// mode changes and different colors still follow configuration refreshes.
		if (m_preferCustomSelection && index != GetCustomIndex())
		{
			const auto* preset = GetPreset(index);
			if (preset && (preset->argb & 0xFFFFFF) == (m_customColor & 0xFFFFFF)
				&& ColorizationPresets::CalculateVistaOpacity(preset->argb) == ColorizationPresets::CalculateVistaOpacity(m_customColor))
				index = GetCustomIndex();
			else m_preferCustomSelection = false;
		}
		const bool initializeFocus = m_currentSelection < 0;
		m_currentSelection = index;
		// Initialize at the applied color. Later reads must preserve navigation,
		// including an inactive list; entering the palette handles focus separately.
		if (index >= 0 && index < GetItemCount() && initializeFocus) Focus(index);
		const auto previous = GetFirstSelected();
		if (previous == index) return;
		m_settingSelection = true;
		auto reset = wil::scope_exit([this] { m_settingSelection = false; });
		if (previous != -1) Select(previous, false);
		if (index >= 0 && index < GetItemCount()) Select(index);
	}

	void ColorPresetList::SetAutomaticDescription(const wxString& description)
	{
		const auto tooltip = description + L"\nClick or press Enter/Space to apply a color. On selected Custom, Enter/Space opens the editor; double-click also edits. Arrow keys move focus.";
		// Resetting unchanged text can show the tooltip again during selection refreshes.
		if (GetToolTipText() != tooltip) SetToolTip(tooltip);
	}

	wxBitmap ColorPresetList::CreateSwatch(DWORD argb, bool automatic) const
	{
		const auto size = FromDIP(wxSize(SwatchSizeDip, SwatchSizeDip));
		const int top = FromDIP(SwatchTopPaddingDip);
		wxBitmap bitmap(size.x, top + size.y + FromDIP(SwatchBottomPaddingDip), 32);
		bitmap.UseAlpha();
		wxMemoryDC dc(bitmap);
		dc.SetBackground(wxBrush(wxColour(0, 0, 0, 0)));
		dc.Clear();
		{
			std::unique_ptr<wxGraphicsContext> context{ wxGraphicsContext::Create(dc) };
			if (context)
			{
				context->SetPen(*wxTRANSPARENT_PEN);
				context->SetBrush(wxBrush(GetSurfaceBorder()));
				context->DrawRectangle(0, top, size.x, size.y);
				const int border = std::max(1, FromDIP(1));
				const wxRect2DDouble bounds(border, top + border, size.x - 2 * border, size.y - 2 * border);
				if (automatic)
				{
					DrawAutomaticColorSwatch(*context, bounds);
					DrawColorCardFan(*context, bounds);
				}
				else
				{
					const wxColour color((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
					context->SetBrush(context->CreateLinearGradientBrush(0, top, size.x, top + size.y,
						color.ChangeLightness(175), color.ChangeLightness(95)));
					context->DrawRectangle(bounds.m_x, bounds.m_y, bounds.m_width, bounds.m_height);
				}
			}
		}
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}

	void ColorPresetList::RebuildImages()
	{
		const int size = FromDIP(SwatchSizeDip);
		const int imageHeight = FromDIP(SwatchTopPaddingDip) + size + FromDIP(SwatchBottomPaddingDip);
		auto images = std::make_unique<wxImageList>(size, imageHeight, true);
		images->Add(CreateSwatch(0, true));
		for (const auto& preset : m_presets) images->Add(CreateSwatch(preset.argb, false));
		images->Add(CreateSwatch(m_customColor, false));
		AssignImageList(images.release(), wxIMAGE_LIST_NORMAL);
		Refresh(false);
	}

	void ColorPresetList::UpdateAppearance()
	{
		// wxWidgets supplies the Explorer theme and dark-mode window opt-in.
		SetBackgroundColour(GetParent()->GetBackgroundColour());
		SetTextColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
	}

	void ColorPresetList::UpdateLayout()
	{
		if (m_presets.empty() || GetClientSize().x <= 0) return;
		wxWindowUpdateLocker lock(this);
		const auto hwnd = static_cast<HWND>(GetHandle());
		int cellWidth = FromDIP(72);
		for (long index = 0; index < GetItemCount(); ++index)
			cellWidth = std::max(cellWidth, GetTextExtent(GetItemText(index)).x + FromDIP(12));
		const int cellHeight = FromDIP(SwatchTopPaddingDip + SwatchSizeDip + SwatchBottomPaddingDip + 16) + GetCharHeight();
		// A settings refresh can send a size event without changing our size.
		// Keep the measured row spacing instead of moving later rows to the
		// provisional spacing and back on every refresh.
		const auto spacing = ListView_GetItemSpacing(hwnd, FALSE);
		if (LOWORD(spacing) != cellWidth) ListView_SetIconSpacing(hwnd, cellWidth, cellHeight);
		Arrange(wxLIST_ALIGN_TOP);
		// Match the row stride to the native item bounds, without an extra gap
		// between selection rectangles. Swatch padding stays inside the image.
		int rowHeight{};
		for (long index = 0; index < GetItemCount(); ++index)
		{
			wxRect bounds;
			if (GetItemRect(index, bounds)) rowHeight = std::max(rowHeight, bounds.height);
		}
		if (rowHeight > 0 && rowHeight != HIWORD(ListView_GetItemSpacing(hwnd, FALSE)))
		{
			ListView_SetIconSpacing(hwnd, cellWidth, rowHeight);
			Arrange(wxLIST_ALIGN_TOP);
		}
		int height = rowHeight > 0 ? rowHeight : cellHeight;
		for (long index = 0; index < GetItemCount(); ++index)
		{
			wxRect bounds;
			if (GetItemRect(index, bounds)) height = std::max(height, bounds.GetBottom() + FromDIP(4));
		}
		if (GetMinSize().y == height) return;
		SetMinSize(wxSize(0, height));
		if (m_layoutPending) return;
		m_layoutPending = true;
		CallAfter([this] {
			m_layoutPending = false;
			GetParent()->Layout();
			if (auto* page = wxDynamicCast(GetParent(), wxScrolledWindow)) page->FitInside();
		});
	}

	void ColorPresetList::Invoke(long index, bool edit)
	{
		if (m_invoking || index < 0 || index >= GetItemCount()) return;
		Focus(index);
		m_preferCustomSelection = index == GetCustomIndex();
		m_invoking = true;
		auto reset = wil::scope_exit([this] { m_invoking = false; });
		wxCommandEvent event(edit ? wxEVT_CUSTOM_COLOR_EDIT_REQUESTED : wxEVT_COLOR_PRESET_INVOKED, GetId());
		event.SetEventObject(this);
		event.SetInt(static_cast<int>(index));
		ProcessWindowEvent(event);
	}

	bool ColorPresetList::DrawHighlightedItem(HDC dc, long index, bool selected, bool focused) const
	{
		const auto hwnd = static_cast<HWND>(GetHandle());
		RECT bounds{}, label{};
		POINT icon{};
		if (!ListView_GetItemRect(hwnd, index, &bounds, LVIR_BOUNDS)
			|| !ListView_GetItemPosition(hwnd, index, &icon)
			|| !ListView_GetItemRect(hwnd, index, &label, LVIR_LABEL)) return false;
		const int saved = SaveDC(dc);
		if (!saved) return false;
		auto restore = wil::scope_exit([&] { RestoreDC(dc, saved); });
		wxBrush background(GetBackgroundColour());
		FillRect(dc, &bounds, reinterpret_cast<HBRUSH>(background.GetResourceHandle()));

		// Use the same themed selection state even when the list or its window
		// loses focus. Native drawing otherwise switches to LISS_SELECTEDNOTFOCUS.
		const auto theme = OpenThemeData(hwnd, L"ListView");
		auto closeTheme = wil::scope_exit([&] { if (theme) CloseThemeData(theme); });
		const bool themed = theme && SUCCEEDED(DrawThemeBackground(theme, dc, LVP_LISTITEM,
			selected ? LISS_SELECTED : LISS_HOT, &bounds, nullptr));
		if (!themed) FillRect(dc, &bounds, GetSysColorBrush(COLOR_HIGHLIGHT));
		if (const auto images = ListView_GetImageList(hwnd, LVSIL_NORMAL))
		{
			LVITEMW item{};
			item.mask = LVIF_IMAGE;
			item.iItem = static_cast<int>(index);
			if (ListView_GetItem(hwnd, &item)) ImageList_Draw(images, item.iImage, dc, icon.x, icon.y, ILD_TRANSPARENT);
		}
		SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
		SetBkMode(dc, TRANSPARENT);
		const auto textColor = themed ? GetTextColour() : wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
		SetTextColor(dc, RGB(textColor.Red(), textColor.Green(), textColor.Blue()));
		const auto text = GetItemText(index);
		DrawTextW(dc, text.wc_str(), static_cast<int>(text.length()), &label, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		// Keep the keyboard focus cue separate from the persistent selection.
		if (focused) DrawFocusRect(dc, &bounds);
		return true;
	}

	bool ColorPresetList::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result)
	{
		const auto* header = reinterpret_cast<NMHDR*>(lParam);
		if (header->hwndFrom == static_cast<HWND>(GetHandle()))
		{
			if (header->code == LVN_ITEMCHANGED && !m_settingSelection)
			{
				const auto* change = reinterpret_cast<NMLISTVIEW*>(lParam);
				if ((change->uChanged & LVIF_STATE) && ((change->uNewState ^ change->uOldState) & LVIS_SELECTED))
				{
					// Let native input finish before restoring the applied selection.
					// A click's invocation may update it before this callback runs.
					CallAfter([this] { SetCurrentSelection(m_currentSelection); });
				}
			}
			if (header->code == NM_CUSTOMDRAW)
			{
				// wxWidgets' dark custom painter draws report columns and a small
				// image list. Use native geometry and theme for our icon view.
				const auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
				*result = CDRF_DODEFAULT;
				if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) *result = CDRF_NOTIFYITEMDRAW;
				else if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
				{
					const auto index = static_cast<long>(draw->nmcd.dwItemSpec);
					const bool selected = IsSelected(index);
					const bool focused = HasFocus() && GetFocusedItem() == index
						&& !(SendMessageW(static_cast<HWND>(GetHandle()), WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
					if ((selected || focused) && DrawHighlightedItem(draw->nmcd.hdc, index, selected, focused)) *result = CDRF_SKIPDEFAULT;
				}
				return true;
			}
			if (header->code == NM_CLICK)
			{
				const auto* click = reinterpret_cast<NMITEMACTIVATE*>(lParam);
				if (click->iItem >= 0) Invoke(click->iItem);
				else SetCurrentSelection(m_currentSelection);
				*result = 0;
				return true;
			}
			if (header->code == NM_DBLCLK)
			{
				// Fixed colors were applied by the first click; only Custom has an editor.
				const auto* click = reinterpret_cast<NMITEMACTIVATE*>(lParam);
				if (click->iItem == GetCustomIndex()) Invoke(click->iItem, true);
				*result = 0;
				return true;
			}
		}
		return wxListView::MSWOnNotify(idCtrl, lParam, result);
	}

	void ColorPresetList::OnKeyDown(wxKeyEvent& event)
	{
		const auto current = GetFocusedItem();
		if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER || event.GetKeyCode() == WXK_SPACE)
		{
			if (!event.IsAutoRepeat()) Invoke(current, current == GetCustomIndex() && IsSelected(current));
			return;
		}
		UINT direction{};
		switch (event.GetKeyCode())
		{
		case WXK_LEFT: direction = LVNI_TOLEFT; break;
		case WXK_RIGHT: direction = LVNI_TORIGHT; break;
		case WXK_UP: direction = LVNI_ABOVE; break;
		case WXK_DOWN: direction = LVNI_BELOW; break;
		case WXK_HOME: case WXK_END: break;
		default: event.Skip(); return;
		}
		const auto hwnd = static_cast<HWND>(GetHandle());
		// We consume navigation keys, so explicitly reveal the keyboard cue.
		SendMessageW(hwnd, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
		const auto next = event.GetKeyCode() == WXK_HOME ? 0
			: event.GetKeyCode() == WXK_END ? GetItemCount() - 1
			: current < 0 ? 0 : ListView_GetNextItem(hwnd, current, direction);
		if (next >= 0)
		{
			Focus(next);
			if (current >= 0) RefreshItem(current);
			RefreshItem(next);
		}
	}
}
