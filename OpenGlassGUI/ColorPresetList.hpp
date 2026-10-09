#pragma once

#include "pch.h"
#include "ColorizationPresets.hpp"

namespace OpenGlass
{
	void DrawAutomaticColorSwatch(wxGraphicsContext& context, const wxRect2DDouble& bounds);
	wxDECLARE_EVENT(wxEVT_COLOR_PRESET_INVOKED, wxCommandEvent);
	wxDECLARE_EVENT(wxEVT_CUSTOM_COLOR_EDIT_REQUESTED, wxCommandEvent);

	class ColorPresetList final : public wxListView
	{
	public:
		explicit ColorPresetList(wxWindow* parent);
		void SetFamily(ColorizationPresets::Family family);
		void SetCustomColor(DWORD argb);
		[[nodiscard]] DWORD GetCustomColor() const noexcept { return m_customColor; }
		[[nodiscard]] long GetCustomIndex() const noexcept { return static_cast<long>(m_presets.size()) + 1; }
		[[nodiscard]] const ColorizationPresets::Preset* GetPreset(long index) const noexcept;
		void SetCurrentSelection(long index);
		void SetAutomaticDescription(const wxString& description);

	protected:
		bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result) override;

	private:
		wxBitmap CreateSwatch(DWORD argb, bool automatic) const;
		void RebuildImages();
		void UpdateAppearance();
		void UpdateLayout();
		bool DrawHighlightedItem(HDC dc, long index, bool selected, bool focused) const;
		void Invoke(long index, bool edit = false);
		void OnKeyDown(wxKeyEvent& event);

		std::span<const ColorizationPresets::Preset> m_presets;
		DWORD m_customColor{ 0xFF000000 };
		long m_currentSelection{ -1 };
		bool m_preferCustomSelection{};
		bool m_settingSelection{};
		bool m_invoking{};
		bool m_layoutPending{};
	};
}
