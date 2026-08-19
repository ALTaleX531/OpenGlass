#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameDemodernizer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameDemodernizer
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
	extern "C" bool WINAPI MySetMargin(
		MARGINS* dstMargins,
		int cxLeftWidth,
		int cxRightWidth,
		int cyTopHeight,
		int cyBottomHeight,
		const MARGINS* srcMargins
	);
	extern "C" bool WINAPI MySetMargin_Impl(
		MARGINS* dstMargins,
		int cxLeftWidth,
		int cxRightWidth,
		int cyTopHeight,
		int cyBottomHeight,
		const MARGINS* srcMargins
	);

	Projection::ChainDetour<uDWM::Symbol_CTopLevelWindow_ValidateVisual, decltype(&MyCTopLevelWindow_ValidateVisual)> g_CTopLevelWindow_ValidateVisual_Org{};
	Projection::ChainDetour<uDWM::Symbol_CTopLevelWindow_UpdateNCAreaBackground, decltype(&MyCTopLevelWindow_UpdateNCAreaBackground)> g_CTopLevelWindow_UpdateNCAreaBackground_Org{};
	Projection::CustomDispatchDetour<uDWM::Symbol_SetMargin, decltype(&MySetMargin)> g_SetMargin_Org{};

	UCHAR g_callCDesktopManager_IsHighContrastMode_Instructions[]
	{
		// call ???
		0xE8, 0x00, 0x00, 0x00, 0x00,
		// test al, al
		0x84, 0xC0
	};
	UCHAR g_callCDesktopManager_IsHighContrastMode_replacedInstruction[]
	{
		// move al, 0x01
		0xB0, 0x01,
		// nop
		// nop
		// nop
		0x90, 0x90, 0x90,
		// test al, al
		0x84, 0xC0
	};
	UCHAR g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions[]
	{
		// lea eax, [???-12h]
		0x8D, 0x00, 0xEE,
		// cmp eax, 3
		0x83, 0xF8, 0x03,
		// ja short loc_xxxxxxxx
		0x77,
	};
	UCHAR g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_replacedInstructions[]
	{
		// lea eax, [???-12h]
		0x8D, 0x00, 0xEE,
		// nop
		// nop
		// nop
		0x90, 0x90, 0x90,
		// jmp
		0xEB,
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
	std::vector<HookHelper::InstructionPatch> g_instructionPatches{};
	std::vector<std::byte> g_atlasedImageNullSentinel{};
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

	g_systemBackdrop = (uDWM::g_versionInfo.build == os::build_w11_21h2 && old_systemBackdropType) || (uDWM::g_versionInfo.build > os::build_w11_21h2 && old_systemBackdropType >= DWMSBT_MAINWINDOW);
	systemBackdropType = (uDWM::g_versionInfo.build == os::build_w11_21h2 ? DWMSBT_AUTO : DWMSBT_NONE);
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
	uDWM::CAtlasedImage const** atlasedImagePtrs[3]{};

	auto& highContrastMode = uDWM::CDesktopManager::GetInstance()->GetIsHighContrastMode();
	const auto old_highContrastMode = highContrastMode;

	// windows 11 rtm lacks null pointer check when high contrast mode is enabled
	// so we manually add it here
	if (
		uDWM::g_versionInfo.build == os::build_w11_21h2 && 
		uDWM::g_versionInfo.revision == os::revision_21h2_rtm_0 &&
		(
			atlasedImagePtrs[0] = &This->GetNCAreaAtlasImage1(),
			atlasedImagePtrs[1] = &This->GetNCAreaAtlasImage2(),
			atlasedImagePtrs[2] = &This->GetNCAreaAtlasImage3(),

			std::any_of(
				std::begin(atlasedImagePtrs),
				std::end(atlasedImagePtrs),
				[](auto atlasedImagePtr) { return *atlasedImagePtr == nullptr; }
			)
		)
	)
	{
		const auto atlasedImageNullSentinel =
			reinterpret_cast<uDWM::CAtlasedImage const*>(g_atlasedImageNullSentinel.data());

		for (auto& atlasedImagePtr : atlasedImagePtrs)
		{
			if (*atlasedImagePtr == nullptr)
			{
				*atlasedImagePtr = atlasedImageNullSentinel;
			}
			else
			{
				atlasedImagePtr = nullptr;
			}
		}
	}
	else
	{
		memset(atlasedImagePtrs, 0, sizeof(atlasedImagePtrs));
		highContrastMode = true;
	}
	const auto highContrastFakeScope = wil::scope_exit([&highContrastMode, old_highContrastMode, &atlasedImagePtrs]
	{
		highContrastMode = old_highContrastMode;
		for (auto& atlasedImagePtr : atlasedImagePtrs)
		{
			if (atlasedImagePtr)
			{
				*atlasedImagePtr = nullptr;
				atlasedImagePtr = nullptr;
			}
		}
	});

	return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
}

bool WINAPI GlassFrameDemodernizer::MySetMargin_Impl(
	MARGINS* dstMargins,
	int cxLeftWidth,
	int cxRightWidth,
	int cyTopHeight,
	int cyBottomHeight,
	const MARGINS* srcMargins
)
{
	return g_SetMargin_Org.Dispatch(
		[](MARGINS* destination, int left, int right, int top, int bottom, const MARGINS* source)
		{
			return g_SetMargin_Org(
				destination,
				left,
				right,
				Shared::g_disableModernBorders ? std::max(0, top) : top,
				bottom,
				source
			);
		},
		dstMargins,
		cxLeftWidth,
		cxRightWidth,
		cyTopHeight,
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
	if (uDWM::g_versionInfo.build < os::build_w11_21h2)
	{
		return;
	}

	if (
		uDWM::g_versionInfo.build == os::build_w11_21h2 &&
		uDWM::g_versionInfo.revision == os::revision_21h2_rtm_0
	)
	{
		const auto sizeOffset = uDWM::CAtlasedImage_GetSize.offset();
		FAIL_FAST_HR_IF_MSG(E_UNEXPECTED, sizeOffset < 0, "CAtlasedImage size offset is negative");
		FAIL_FAST_HR_IF_MSG(E_UNEXPECTED, static_cast<ULONGLONG>(sizeOffset) > SIZE_MAX - sizeof(DWORD), "CAtlasedImage size offset overflows");

		g_atlasedImageNullSentinel.assign(static_cast<size_t>(sizeOffset) + sizeof(DWORD), std::byte{});
		auto atlasedImageNullSentinel =
			reinterpret_cast<uDWM::CAtlasedImage*>(g_atlasedImageNullSentinel.data());
		atlasedImageNullSentinel->GetSize() = 0;
	}

	auto CTopLevelWindow_UpdateWindowVisuals_Instructions =
		static_cast<UCHAR*>(uDWM::Symbol_CTopLevelWindow_UpdateWindowVisuals.get());
	const auto CDesktopManager_IsHighContrastMode_Instructions =
		static_cast<UCHAR*>(uDWM::Symbol_CDesktopManager_IsHighContrastMode.get());

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
	if (uDWM::g_versionInfo.build < os::build_w11_24h2)
	{
		i = 450'000;
		do
		{
			g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_replacedInstructions[1] = g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions[1] = CTopLevelWindow_UpdateWindowVisuals_Instructions[1];
			if (
				memcmp(
					CTopLevelWindow_UpdateWindowVisuals_Instructions,
					g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions,
					sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions)
				) == 0
			)
			{
				std::vector<UCHAR> backup(sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions), 0);
				memcpy_s(
					backup.data(),
					backup.size(),
					CTopLevelWindow_UpdateWindowVisuals_Instructions,
					sizeof(g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_Instructions)
				);
				g_instructionsBackup.insert_or_assign(
					CTopLevelWindow_UpdateWindowVisuals_Instructions,
					backup
				);
				g_instructionsToReplace.insert_or_assign(
					CTopLevelWindow_UpdateWindowVisuals_Instructions,
					std::vector(std::begin(g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_replacedInstructions), std::end(g_callCTopLevelWindow_IsShadowNCAreaPart_inlined_replacedInstructions))
				);
				break;
			}

			CTopLevelWindow_UpdateWindowVisuals_Instructions += 1;
			i--;
		} while (i);
	}
	else
	{
		const auto CTopLevelWindow_IsShadowNCAreaPart_Instructions =
			static_cast<UCHAR*>(uDWM::Symbol_CTopLevelWindow_IsShadowNCAreaPart.get());
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
	}
	FAIL_FAST_IF_FAILED_MSG(g_instructionsToReplace.size() == 2 ? S_OK : E_NOINTERFACE, "Unable to locate the complete frame-demodernizer instruction patch set");
	g_instructionPatches.clear();
	g_instructionPatches.reserve(g_instructionsToReplace.size());
	for (const auto& [address, instructions] : g_instructionsToReplace)
	{
		const auto backup = g_instructionsBackup.find(address);
		FAIL_FAST_IF_FAILED_MSG(backup != g_instructionsBackup.end() ? S_OK : E_UNEXPECTED, "Missing captured instruction bytes at %p", address);
		auto& patch = g_instructionPatches.emplace_back();
		patch.Prepare(
			address,
			backup->second,
			instructions
		);
	}
	for (auto& patch : g_instructionPatches)
	{
		HookHelper::GetCurrentHookTransaction().Apply(patch);
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{
				&g_SetMargin_Org,
				uDWM::g_versionInfo.build >= os::build_w11_24h2 ? &MySetMargin : &MySetMargin_Impl
			},
		},
		true
	);
}

void GlassFrameDemodernizer::Shutdown()
{
	if (uDWM::g_versionInfo.build < os::build_w11_21h2)
	{
		return;
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{
				&g_SetMargin_Org,
				uDWM::g_versionInfo.build >= os::build_w11_24h2 ? &MySetMargin : &MySetMargin_Impl
			},
		},
		false
	);

	for (auto& patch : g_instructionPatches)
	{
		HookHelper::GetCurrentHookTransaction().Apply(patch);
	}
}

void GlassFrameDemodernizer::Cleanup()
{
	g_instructionsBackup.clear();
	g_instructionsToReplace.clear();
	g_instructionPatches.clear();
	g_atlasedImageNullSentinel.clear();
}
