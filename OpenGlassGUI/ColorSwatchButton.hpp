#pragma once

#include "pch.h"

namespace OpenGlass
{
	class ColorSwatchButton final : public wxToggleButton
	{
	public:
		ColorSwatchButton(wxWindow* parent, wxWindowID id, const wxString& label, DWORD argb);
		[[nodiscard]] DWORD GetColor() const noexcept { return m_argb; }
		void SetColor(DWORD argb);
		void SetValue(bool value) override;
		bool MSWOnDraw(WXDRAWITEMSTRUCT* item) override;

	private:
		void RebuildBitmap();
		void OnDpiChanged(wxDPIChangedEvent& event);
		void OnSystemColorChanged(wxSysColourChangedEvent& event);

		DWORD m_argb{};
		wxBitmap m_normalBitmap;
		wxBitmap m_selectedBitmap;
	};
}
