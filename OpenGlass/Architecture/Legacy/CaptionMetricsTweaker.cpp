#include "pch.h"
#include "Shared.hpp"
#include "CaptionMetricsTweaker.hpp"

using namespace OpenGlass;

namespace OpenGlass::CaptionMetricsTweaker
{
	void MyCButton_SetSize(uDWM::CButton* This, const SIZE* size);
	HRESULT MyCTopLevelWindow_UpdateNCAreaPositionsAndSizes(uDWM::CTopLevelWindow* This);

	decltype(&MyCButton_SetSize) g_CButton_SetSize_Org{ nullptr };
	decltype(&MyCButton_SetSize)* g_CButton_SetSize_Org_Address{ nullptr };
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_UpdateNCAreaPositionsAndSizes, decltype(&MyCTopLevelWindow_UpdateNCAreaPositionsAndSizes)> g_CTopLevelWindow_UpdateNCAreaPositionsAndSizes_Org{};

	enum CaptionButtons : UINT
	{
		Disabled = 0,
		WindowsVista,
		Windows7,
		Windows8
	} g_captionButtons{ 0 };
	SIZE CalculateButtonSize(int cySize, int buttonType);
}

SIZE CaptionMetricsTweaker::CalculateButtonSize(int cySize, int buttonType)
{
	enum
	{
		LoneButton = 0,
		MinButton,
		MaxButton,
		CloseButton,
	};

	auto [heightRatio, loneWidthRatio, closeWidthRatio, maxWidthRatio, minWidthRatio] = std::make_tuple(0.f, 0.f, 0.f, 0.f, 0.f);

	switch (g_captionButtons)
	{
	case CaptionButtons::WindowsVista:
		std::tie(heightRatio, loneWidthRatio, closeWidthRatio, maxWidthRatio, minWidthRatio) =
			std::make_tuple(0.94736844f, 2.3157895f, 2.3157895f, 1.3157895f, 1.3684211f);
		break;
	case CaptionButtons::Windows7:
	default:
		std::tie(heightRatio, loneWidthRatio, closeWidthRatio, maxWidthRatio, minWidthRatio) =
			std::make_tuple(0.95238096f, 2.3333333f, 2.3333333f, 1.2857143f, 1.3809524f);
		break;
	case CaptionButtons::Windows8:
		std::tie(heightRatio, loneWidthRatio, closeWidthRatio, maxWidthRatio, minWidthRatio) =
			std::make_tuple(0.95454544f, 1.6363636f, 2.2272727f, 1.2272727f, 1.3181819f);
		break;
	}

	SIZE buttonSize = { 0, static_cast<LONG>(std::round(static_cast<float>(cySize) * heightRatio)) };

	switch (buttonType)
	{
	case CloseButton:
		buttonSize.cx = static_cast<LONG>(std::round(static_cast<float>(cySize) * closeWidthRatio));
		break;
	case MaxButton:
		buttonSize.cx = static_cast<LONG>(std::round(static_cast<float>(cySize) * maxWidthRatio));
		break;
	case MinButton:
		buttonSize.cx = static_cast<LONG>(std::round(static_cast<float>(cySize) * minWidthRatio));
		break;
	case LoneButton:
		buttonSize.cx = static_cast<LONG>(std::round(static_cast<float>(cySize) * loneWidthRatio));
		break;
	default:
		break;
	}

	return buttonSize;
}

HRESULT CaptionMetricsTweaker::MyCTopLevelWindow_UpdateNCAreaPositionsAndSizes(uDWM::CTopLevelWindow* This)
{
	const auto hr = g_CTopLevelWindow_UpdateNCAreaPositionsAndSizes_Org(This);

	if (!g_captionButtons)
	{
		return hr;
	}

	auto data = This->GetData();
	if (!data)
	{
		return hr;
	}

	auto maximized = This->IsWindowMaximized();
	auto toolWindow = This->IsToolWindow();
	auto loneButton = This->IsLoneButton();

	auto& visibleMargins = This->GetFrameOutsideMargins(maximized);
	auto& borderMargins = This->GetBorderMargins();

	int cxLeft = borderMargins.cxLeftWidth ? borderMargins.cxLeftWidth : data->GetFrameThickness();

	auto UpdateButton = [&](int buttonType, int offsetRight, int offsetTop, SIZE buttonSize)
	{
		if (auto button = This->GetButton(buttonType); button)
		{
			MARGINS inset = { 0x7FFFFFFF, offsetRight, offsetTop, 0x7FFFFFFF };

			g_CButton_SetSize_Org(button, &buttonSize);
			button->SetInsetFromParent(inset);
			button->GetGlyphOpacity() = 1.f;

			return true;
		}
		return false;
	};

	int cySize = GetSystemMetricsForDpi(SM_CYSIZE, data->GetWindowDPI());

	int offsetRight = maximized ? borderMargins.cxRightWidth + 2 : (borderMargins.cxRightWidth ? borderMargins.cxRightWidth - 2 : data->GetFrameThickness() - 2);
	int offsetTop = maximized ? visibleMargins.cyTopHeight - 1 : visibleMargins.cyTopHeight + 1;

	auto closeButtonSize = loneButton ? CalculateButtonSize(cySize, 0) : CalculateButtonSize(cySize, 3);
	auto maxButtonSize = CalculateButtonSize(cySize, 2);
	auto minButtonSize = CalculateButtonSize(cySize, 1);

	if (UpdateButton(3, offsetRight, offsetTop, closeButtonSize) && !toolWindow)
	{
		offsetRight = closeButtonSize.cx + offsetRight;
	}

	if (UpdateButton(2, offsetRight, offsetTop, maxButtonSize))
	{
		offsetRight += maxButtonSize.cx;
	}

	if (UpdateButton(1, offsetRight, offsetTop, minButtonSize))
	{
		offsetRight += minButtonSize.cx;
	}

	UpdateButton(0, offsetRight, offsetTop, minButtonSize);

	if (toolWindow)
	{
		int cySmSize = GetSystemMetricsForDpi(SM_CYSMSIZE, data->GetWindowDPI());
		SIZE toolButtonSize = { cySmSize , cySmSize };

		offsetTop = (borderMargins.cyTopHeight - toolButtonSize.cy - 4 > offsetTop) ? borderMargins.cyTopHeight - toolButtonSize.cy - 4 : offsetTop;
		UpdateButton(3, offsetRight, offsetTop, toolButtonSize);
		offsetRight = toolButtonSize.cx + offsetRight;
	}

	if (auto iconVisual = This->GetIconVisual(); iconVisual && cxLeft > 0)
	{
		iconVisual->SetInsetFromParentLeft(cxLeft);
	}

	if (auto textVisual = This->GetTextVisual(); textVisual)
	{
		if (auto iconVisual = This->GetIconVisual(); iconVisual && cxLeft > 0)
		{
			cxLeft += iconVisual->GetWidth() ? iconVisual->GetWidth() + 5 : 0;
		}
		MARGINS inset = { cxLeft, offsetRight, visibleMargins.cyTopHeight, 0x7FFFFFFF };
		textVisual->SetInsetFromParent(inset);
	}

	return hr;
}

void CaptionMetricsTweaker::MyCButton_SetSize(uDWM::CButton* This, const SIZE* size)
{
	if (Shared::g_captionHeight.has_value())
	{
		const SIZE replacedSize
		{
			size->cx,
			std::min(size->cy, static_cast<LONG>(Shared::g_captionHeight.value() * uDWM::CDesktopManager::GetInstance()->GetDPIValue()))
		};
		return g_CButton_SetSize_Org(This, &replacedSize);
	}

	return g_CButton_SetSize_Org(This, size);
}

void CaptionMetricsTweaker::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		g_captionButtons = static_cast<CaptionButtons>(GlassEngine::GetDwordFromRegistry(L"CaptionButtons", 0));
	}
}

void CaptionMetricsTweaker::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionMetricsTweaker))
	{
		return;
	}

	const auto CVisual_SetSize_Org = Util::force_cast_from(Projection::Invoke<&uDWM::CVisual::SetSize>);
	for (auto& vf : std::span{ uDWM::CButton::vftable, 20 })
	{
		if (vf == CVisual_SetSize_Org)
		{
			g_CButton_SetSize_Org_Address = reinterpret_cast<decltype(g_CButton_SetSize_Org_Address)>(&vf);
			HookHelper::PatchPointerT(g_CButton_SetSize_Org_Address, MyCButton_SetSize, &g_CButton_SetSize_Org);
		}
	}

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_UpdateNCAreaPositionsAndSizes_Org, &MyCTopLevelWindow_UpdateNCAreaPositionsAndSizes },
		},
		true
	);
}

void CaptionMetricsTweaker::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionMetricsTweaker))
	{
		return;
	}

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_UpdateNCAreaPositionsAndSizes_Org, &MyCTopLevelWindow_UpdateNCAreaPositionsAndSizes },
		},
		false
	);

	SwitchToThread();

	if (g_CButton_SetSize_Org)
	{
		HookHelper::PatchPointerT(g_CButton_SetSize_Org_Address, g_CButton_SetSize_Org);
	}
}
