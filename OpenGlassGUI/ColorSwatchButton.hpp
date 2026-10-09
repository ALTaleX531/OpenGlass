#pragma once

#include "pch.h"

namespace OpenGlass
{
	void DrawAutomaticColorSwatch(wxGraphicsContext& context, const wxRect2DDouble& bounds);

	class ColorSwatchButton final : public wxToggleButton
	{
	public:
		ColorSwatchButton(wxWindow* parent, wxWindowID id, const wxString& label, DWORD argb, bool automatic = false);
		[[nodiscard]] DWORD GetColor() const noexcept { return m_argb; }
		void SetColor(DWORD argb);
		void SetValue(bool value) override;
		bool MSWOnDraw(WXDRAWITEMSTRUCT* item) override;

	private:
		void RebuildBitmap();
		void OnDpiChanged(wxDPIChangedEvent& event);
		void OnSystemColorChanged(wxSysColourChangedEvent& event);

		DWORD m_argb{};
		bool m_automatic{};
		wxColour m_surfaceBorder;
		wxColour m_selectionBorder;
		wxBitmap m_normalBitmap;
		wxBitmap m_selectedBitmap;
	};
}
