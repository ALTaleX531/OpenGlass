#include "pch.h"
#include "CaptionText.hpp"
#include "MsstyleInternals.hpp"
#include "CustomThemeAtlasLoader.hpp"

namespace OpenGlass::CaptionText
{
	CWindowStatePtr GetOrCreateWindowState(WindowStateMap& states, uDWM::CVisual* visual)
	{
		auto& state = states[visual];
		if (!state)
		{
			state = Util::make_com_ptr<CWindowState>();
			state->sourceVisual = visual;
		}

		return state;
	}

	const CWindowState& GetWindowState(const WindowStateMap& states, uDWM::CVisual* visual)
	{
		static const CWindowState defaultState{};
		const auto it = states.find(visual);
		return it == states.end() || !it->second ? defaultState : *it->second;
	}

	void ShareWindowState(WindowStateMap& states, uDWM::CVisual* source, uDWM::CVisual* clone)
	{
		if (source && clone)
		{
			// The source remains authoritative; preview containers must not replace its active state.
			states[clone] = GetOrCreateWindowState(states, source);
		}
	}

	int CenterOffset(int centerCaption, bool isTrimmed, double visualWidth, double textWidth, double visualX, double parentWidth, double scale)
	{
		const int localOffset = std::max(
			static_cast<int>((visualWidth - textWidth) / 2.0),
			0
		);
		if (centerCaption <= 0 || isTrimmed)
			return 0;

		if (centerCaption == 1)
			return localOffset;

		// Win8 centers against the parent visual and uses a small scale-adjusted
		// guard band before falling back to the local visual width.
		int parentOffset = static_cast<int>((parentWidth - textWidth) / 2.0) - static_cast<int>(visualX);
		if (static_cast<double>(parentOffset + static_cast<int>(textWidth)) + scale * 5.0 > visualWidth)
			parentOffset = localOffset;

		return std::max(parentOffset, 0);
	}

	void CalculateRealizedTextGlowParams(int textGlowMode, TextGlowParameters& params)
	{
		if (textGlowMode == 0)
		{
			params.size = 0;
		}
		else if (textGlowMode == 1 || textGlowMode == 2)
		{
			const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();

			CustomThemeAtlasLoader::MyGetThemeMargins(
				themeHandle,
				nullptr,
				static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
				0,
				TMT_SIZINGMARGINS,
				nullptr,
				&params.sizingMargins
			);
			CustomThemeAtlasLoader::MyGetThemeMargins(
				themeHandle,
				nullptr,
				static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
				0,
				TMT_CONTENTMARGINS,
				nullptr,
				&params.contentMargins
			);
			params.size = std::max(
				{
					params.contentMargins.cxLeftWidth,
					params.contentMargins.cxRightWidth,
					params.contentMargins.cyTopHeight,
					params.contentMargins.cyBottomHeight
				}
			);
		}
		else
		{
			wil::unique_htheme themeHandle{ OpenThemeData(nullptr, L"CompositedWindow::Window") };

			if (params.size = HIWORD(textGlowMode); !params.size)
			{
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_TEXTGLOWSIZE, &params.size);
			}
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWINTENSITY, &params.intensity);
			GetThemeColor(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWCOLOR, &params.color);
		}
	}
}
