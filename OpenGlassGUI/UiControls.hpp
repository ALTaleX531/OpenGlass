#pragma once
#include "pch.h"

namespace OpenGlass
{
	// An empty background follows the parent. Own colours prevent children from
	// inheriting an RGB snapshot instead of using their native theme colours.
	inline void FollowSystemColours(wxWindow* window, std::optional<wxSystemColour> background,
		std::optional<wxSystemColour> foreground = std::nullopt)
	{
		auto update = [window, background, foreground]
		{
			window->SetOwnBackgroundColour(background
				? wxSystemSettings::GetColour(*background) : window->GetParent()->GetBackgroundColour());
			if (foreground) window->SetOwnForegroundColour(wxSystemSettings::GetColour(*foreground));
		};
		update();
		window->Bind(wxEVT_SYS_COLOUR_CHANGED, [update](wxSysColourChangedEvent& event)
		{
			update();
			// wxWidgets refreshes the top-level window after updating its children.
			event.Skip();
		});
	}

	// Helper class - now uses wxWidgets tooltips for better control
	class NativeSlider : public wxSlider
	{
	public:
		NativeSlider(wxWindow* parent,
			wxWindowID id,
			int value,
			int minValue,
			int maxValue,
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxSL_HORIZONTAL,
			const wxValidator& validator = wxDefaultValidator,
			const wxString& name = wxSliderNameStr)
		{
			Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
		}

		bool Create(wxWindow* parent,
			wxWindowID id,
			int value,
			int minValue,
			int maxValue,
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxSL_HORIZONTAL,
			const wxValidator& validator = wxDefaultValidator,
			const wxString& name = wxSliderNameStr)
		{
			return wxSlider::Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
		}
	};
}
