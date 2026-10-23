#pragma once
#include "Util.hpp"

namespace OpenGlass::uDWM
{
	struct CVisual;
}

namespace OpenGlass::CaptionText
{
	struct CWindowState : Util::ComRefCounted<CWindowState>
	{
		uDWM::CVisual* sourceVisual{};
		bool active{};
		bool maximized{};
		LONG windowRectLeft{};
	};
	using CWindowStatePtr = winrt::com_ptr<CWindowState>;
	using WindowStateMap = std::unordered_map<uDWM::CVisual*, CWindowStatePtr>;

	CWindowStatePtr GetOrCreateWindowState(WindowStateMap& states, uDWM::CVisual* visual);
	const CWindowState& GetWindowState(const WindowStateMap& states, uDWM::CVisual* visual);
	void ShareWindowState(WindowStateMap& states, uDWM::CVisual* source, uDWM::CVisual* clone);
	int CenterOffset(int centerCaption, bool isTrimmed, double visualWidth, double textWidth, double visualX, double parentWidth, double scale);

	inline D2D1_RECT_F CalculateDWriteBitmapBounds(const DWRITE_TEXT_METRICS& metrics, const DWRITE_OVERHANG_METRICS& overhangs)
	{
		// Match DWM's tight surface bounds, including its one-DIP antialiasing margin.
		return
		{
			std::floor(-overhangs.left) - 1.f,
			std::floor(-overhangs.top) - 1.f,
			std::ceil(metrics.layoutWidth + overhangs.right) + 1.f,
			std::ceil(metrics.layoutHeight + overhangs.bottom) + 1.f
		};
	}

	struct TextGlowParameters
	{
		int size{};
		int intensity{};
		COLORREF color{};
		MARGINS contentMargins{}, sizingMargins{};
	};

	// Update caller-owned parameters in place; each mode only writes the fields it uses.
	void CalculateRealizedTextGlowParams(int textGlowMode, TextGlowParameters& params);
}
