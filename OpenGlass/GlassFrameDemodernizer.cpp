#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameDemodernizer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameDemodernizer
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
	bool WINAPI MySetMargin(
		MARGINS* dstMargins,
		int cxLeftWidth,
		int cxRightWidth,
		int cyTopHeight,
		int cyBottomHeight,
		const MARGINS* srcMargins
	);

	decltype(&MyCTopLevelWindow_ValidateVisual) g_CTopLevelWindow_ValidateVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateNCAreaBackground) g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	decltype(&MySetMargin) g_SetMargin_Org{ nullptr };

	UCHAR g_callCDesktopManager_IsHighContrastMode_Instructions[]
	{
		// call ???
		0xE8, 0x00, 0x00, 0x00, 0x00,
		// mov     r15b, 1
		0x41, 0xB7, 0x01,
		// test al, al
		0x84, 0xC0
	};
	UCHAR g_callCDesktopManager_IsHighContrastMode_replacedInstruction[]
	{
		// mov     r15b, 1
		0x41, 0xB7, 0x01,
		// nop
		// nop
		// nop
		0x90, 0x90, 0x90,
		// move al, 0x01
		0xB0, 0x01,
		// test al, al
		0x84, 0xC0
	};
	UCHAR g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions[]
	{
		// call ???
		0xE8, 0x00, 0x00, 0x00, 0x00,
		// test al, al
		0x84, 0xC0
	};
	UCHAR g_callCTopLevelWindow_IsShadowNCAreaPart_replacedInstructions[]
	{
		// move al, 0x00
		0xB0, 0x00,
		// nop
		// nop
		// nop
		0x90, 0x90, 0x90,
		// test al, al
		0x84, 0xC0
	};
	std::unordered_map<UCHAR*, std::vector<UCHAR>> g_instructionsToReplace{};
	std::unordered_map<UCHAR*, std::vector<UCHAR>> g_instructionsBackup{};
	bool g_systemBackdrop{ false };
}

HRESULT GlassFrameDemodernizer::MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This)
{
	auto data = This->GetData();
	if (!data)
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}

	auto& systemBackdropType = data->GetSystemBackdropType();
	auto& extendedFrameMargins = data->GetExtendedFrameMargins();
	auto& nonclientAttribute = data->GetNonClientAttributeReference();
	auto& borderUpdatesSuppressed = This->GetIsBorderUpdatesSuppressed();
	const auto disableModernFrames = Shared::g_disableModernBorders;
	const auto old_borderUpdatesSuppressed = borderUpdatesSuppressed;
	const auto old_systemBackdropType = systemBackdropType;
	const auto old_extendedFrameMargins = extendedFrameMargins;
	const auto old_nonclientAttribute = nonclientAttribute;

	g_systemBackdrop = old_systemBackdropType >= DWMSBT_MAINWINDOW;
	systemBackdropType = DWMSBT_NONE;
	if (g_systemBackdrop)
	{
		// known issue: 
		// set cyTopHeight to any non zero value will cause compatibility issue with Outlook (new)
		// Outlook (new) has drawn its own titlebar buttons with native titlebar buttons hidden,
		// this will make the native titlebar buttons visible
		// 
		extendedFrameMargins.cyTopHeight = 0x7FFFFFFF;
		extendedFrameMargins.cxLeftWidth = 0x7FFFFFFF;
		extendedFrameMargins.cxRightWidth = 0x7FFFFFFF;
		extendedFrameMargins.cyBottomHeight = 0x7FFFFFFF;
		nonclientAttribute |= 8;
	}
	
	if (disableModernFrames)
	{
		borderUpdatesSuppressed = true;
		if (const auto windowBorder = This->GetWindowBorder(); windowBorder)
		{
			RETURN_IF_FAILED(windowBorder->EnableBorder(false));
		}
	}

	const auto scope = wil::scope_exit([&, old_borderUpdatesSuppressed, old_systemBackdropType, old_extendedFrameMargins, old_nonclientAttribute]
	{
		if (disableModernFrames)
		{
			borderUpdatesSuppressed = old_borderUpdatesSuppressed;
		}
		if (g_systemBackdrop)
		{
			extendedFrameMargins = old_extendedFrameMargins;
			nonclientAttribute = old_nonclientAttribute;
			g_systemBackdrop = false;
		}
		systemBackdropType = old_systemBackdropType;
	});

	return g_CTopLevelWindow_ValidateVisual_Org(This);
}

HRESULT GlassFrameDemodernizer::MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This)
{
	auto& highContrastMode = uDWM::CDesktopManager::GetInstance()->GetIsHighContrastMode();
	const auto old_highContrastMode = highContrastMode;

	highContrastMode = true;
	const auto highContrastFakeScope = wil::scope_exit([&highContrastMode, old_highContrastMode]
	{
		highContrastMode = old_highContrastMode;
	});

	return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
}

bool WINAPI GlassFrameDemodernizer::MySetMargin(
	MARGINS* dstMargins,
	int cxLeftWidth,
	int cxRightWidth,
	int cyTopHeight,
	int cyBottomHeight,
	const MARGINS* srcMargins
)
{
	return g_SetMargin_Org(
		dstMargins,
		cxLeftWidth,
		cxRightWidth,
		Shared::g_disableModernBorders ? std::max(0, cyTopHeight) : cyTopHeight,
		cyBottomHeight,
		srcMargins
	);
}

void GlassFrameDemodernizer::Update(GlassEngine::UpdateType type)
{
	if (uDWM::g_versionInfo.build < os::build_w11_21h2)
	{
		return;
	}

	if (type & GlassEngine::UpdateType::Theme)
	{
		Shared::g_disableModernBorders = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"DisableModernBorders", FALSE));
	}
}

void GlassFrameDemodernizer::Startup()
{
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::ValidateVisual", g_CTopLevelWindow_ValidateVisual_Org);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::UpdateNCAreaBackground", g_CTopLevelWindow_UpdateNCAreaBackground_Org);
	uDWM::g_projectionArray.ApplyToVariable("SetMargin", g_SetMargin_Org);

	UCHAR* CTopLevelWindow_UpdateWindowVisuals_Instructions{ nullptr };
	UCHAR* CDesktopManager_IsHighContrastMode_Instructions{ nullptr };
	UCHAR* CTopLevelWindow_IsShadowNCAreaPart_Instructions{ nullptr };
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::UpdateWindowVisuals", CTopLevelWindow_UpdateWindowVisuals_Instructions);
	uDWM::g_projectionArray.ApplyToVariable("CDesktopManager::IsHighContrastMode", CDesktopManager_IsHighContrastMode_Instructions);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::IsShadowNCAreaPart", CTopLevelWindow_IsShadowNCAreaPart_Instructions);

	auto i = 1'500;
	const auto CTopLevelWindow_UpdateWindowVisuals_Instructions_Previous = CTopLevelWindow_UpdateWindowVisuals_Instructions;
	bool callCDesktopManager_IsHighContrastMode_SecondTime{};
	do
	{
		*reinterpret_cast<DWORD*>(&g_callCDesktopManager_IsHighContrastMode_Instructions[1]) = static_cast<DWORD>(CDesktopManager_IsHighContrastMode_Instructions - (CTopLevelWindow_UpdateWindowVisuals_Instructions + 5));
		if (
			memcmp(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				g_callCDesktopManager_IsHighContrastMode_Instructions,
				sizeof(g_callCDesktopManager_IsHighContrastMode_Instructions)
			) == 0
		)
		{
			// in case we touched the inlined call part of CTopLevelWindow::GetBorderRect
			if (callCDesktopManager_IsHighContrastMode_SecondTime)
			{
				g_instructionsBackup.clear();
				g_instructionsToReplace.clear();
			}
			std::vector<UCHAR> backup(sizeof(g_callCDesktopManager_IsHighContrastMode_Instructions), 0);
			memcpy_s(
				backup.data(),
				backup.size(),
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				sizeof(g_callCDesktopManager_IsHighContrastMode_Instructions)
			);
			g_instructionsBackup.insert_or_assign(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				backup
			);
			g_instructionsToReplace.insert_or_assign(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				std::vector(std::begin(g_callCDesktopManager_IsHighContrastMode_replacedInstruction), std::end(g_callCDesktopManager_IsHighContrastMode_replacedInstruction))
			);
			callCDesktopManager_IsHighContrastMode_SecondTime = true;
		}

		CTopLevelWindow_UpdateWindowVisuals_Instructions += 1;
		i--;
	} while (i);

	CTopLevelWindow_UpdateWindowVisuals_Instructions = CTopLevelWindow_UpdateWindowVisuals_Instructions_Previous;

	i = 1500;
	do
	{
		*reinterpret_cast<DWORD*>(&g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions[1]) = static_cast<DWORD>(CTopLevelWindow_IsShadowNCAreaPart_Instructions - (CTopLevelWindow_UpdateWindowVisuals_Instructions + 5));
		if (
			memcmp(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions,
				sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions)
			) == 0
		)
		{
			std::vector<UCHAR> backup(sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions), 0);
			memcpy_s(
				backup.data(),
				backup.size(),
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_Instructions)
			);
			g_instructionsBackup.insert_or_assign(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				backup
			);
			g_instructionsToReplace.insert_or_assign(
				CTopLevelWindow_UpdateWindowVisuals_Instructions,
				std::vector(std::begin(g_callCTopLevelWindow_IsShadowNCAreaPart_replacedInstructions), std::end(g_callCTopLevelWindow_IsShadowNCAreaPart_replacedInstructions))
			);
			break;
		}

		CTopLevelWindow_UpdateWindowVisuals_Instructions += 1;
		i--;
	} while (i);

	for (const auto& [address, instructions] : g_instructionsToReplace)
	{
		HookHelper::PatchInstructions(
			address,
			instructions
		);
	}

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{ &g_SetMargin_Org, &MySetMargin },
		},
		true
	);
}

void GlassFrameDemodernizer::Shutdown()
{
	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{ &g_SetMargin_Org, &MySetMargin },
		},
		false
	);

	SwitchToThread();

	for (const auto& [address, instructions] : g_instructionsBackup)
	{
		HookHelper::PatchInstructions(
			address,
			instructions
		);
	}
	g_instructionsBackup.clear();
}
