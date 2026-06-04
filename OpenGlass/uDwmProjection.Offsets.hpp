#pragma once
#include "Util.hpp"

namespace OpenGlass::uDWM
{
	// CVisual::SetRTLMirror
	struct CVisual_IsRTLMirrored_ByteOffset_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 36, .build = 0, .revision = 0 }
			};
		}
	};
	// CVisual::SetScale
	struct CVisual_GetScale_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 104, .build = 0, .revision = 0 }
			};
		}
	};
	// CVisual::SetSize
	struct CVisual_GetSize_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8 * (LONGLONG)sizeof(SIZE), .build = 0, .revision = 0 }
			};
		}
	};
	// CVisual::SetOffset
	struct CVisual_GetOffset_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 7 * (LONGLONG)sizeof(POINT), .build = 0, .revision = 0 }
			};
		}
	};
	// xref to VisualCollection::VisualCollection, find CContainerVisual::CContainerVisual:
	//   VisualCollection::VisualCollection((VisualCollection *)(this + 17));
	struct CVisual_GetVisualCollection_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 136, .build = 0, .revision = 0 }
			};
		}
	};
	// CVisual::SendSetOffset
	struct CVisual_GetVisualProxy_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 2 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CVisual::`vftable'
	struct CVisual_GetTransformParent_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CVisual::SetDirtyFlags
	struct CVisual_GetDirtyFlags_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8 * sizeof(DWORD), .build = 0, .revision = 0 }
			};
		}
	};

	// CRectangleVisual::SetRect
	struct CRectangleVisual_GetRect_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 192, .build = 0, .revision = 0 }
			};
		}
	};

	// CLegacyNonClientBackground::SetBorderRects
	struct CLegacyNonClientBackground_GetBorderInnerBounds_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 240, .build = 0, .revision = 0 }
			};
		}
	};
	// CLegacyNonClientBackground::SetBorderRects
	struct CLegacyNonClientBackground_GetBorderOuterBounds_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 256, .build = 0, .revision = 0 }
			};
		}
	};
	// CLegacyNonClientBackground::SetBorderColor
	struct CLegacyNonClientBackground_GetBorderColor_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 224, .build = 0, .revision = 0 }
			};
		}
	};
	// CLegacyNonClientBackground::SetCaptionColor
	struct CLegacyNonClientBackground_GetCaptionColor_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 208, .build = 0, .revision = 0 }
			};
		}
	};
	// CLegacyNonClientBackground::EnsureBorderSprite
	struct CLegacyNonClientBackground_GetBorderVisual_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 200, .build = 0, .revision = 0 }
			};
		}
	};
	// CLegacyNonClientBackground::EnsureCaptionSprite
	struct CLegacyNonClientBackground_GetCaptionVisual_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{.offset = 192, .build = 0, .revision = 0 }
			};
		}
	};

	// xrefs to CDWriteText::`vftable'{for `IText'} → find CDWriteText constructor
	// Constructor reveals the IText vtable subobject offset within CDWriteText.
	// Negative offset = -(subobject offset) to go from IText* back to CDWriteText*.
	struct IText_GetDWriteText_NegativeOffset_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = -184, .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CDWriteText::`vftable'{for `IText'} → decompile SetRTLReading on IText vtable
	// Look for byte bit-test and conditional branch on the memory operand
	struct IText_RTL_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 280, .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CDWriteText::`vftable'{for `IText'} → decompile SetReverseAlignment on IText vtable
	// Same pattern: byte bit-test at an adjacent absolute offset
	struct IText_Reverse_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 281, .build = 0, .revision = 0 }
			};
		}
	};
	// CDWriteText::CDWriteText
	// xrefs to CDWriteText::`vftable'{for `IText'}
	struct CDWriteText_GetTextInterface_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 184, .build = 0, .revision = 0 }
			};
		}
	};

	// CThemePartPrimitive::ShouldClone
	// CThemePartPrimitive::ShouldDraw
	struct CThemePartPrimitive_GetPartId_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 18 * sizeof(DWORD), .build = 0, .revision = 0 }
			};
		}
	};

	// CButton::SetVisualStates
	// WARNING: Use SetVisualStates (base/default glyph opacity), NOT
	// UpdateCurrentGlyphOpacity (runtime/current opacity, different DWORD offset).
	struct CButton_GetGlyphOpacity_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 75 * sizeof(DWORD),  .build = 0, .revision = 0 }
			};
		}
	};
	
	// CWindowData::IsGhostWindow
	// xrefs to GetPropW — first parameter is the HWND, trace back to the CWindowData offset
	struct CWindowData_GetHwnd_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 5 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CWindowList::EnsureTopLevelWindow
	struct CWindowData_GetWindow_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 55 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	struct CWindowData_GetSystemBackdropType_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 51 * sizeof(DWORD), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::CalculateOutsideMargins
	// xrefs to GetSystemMetricsForDpi
	struct CWindowData_GetWindowDPI_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 87 * sizeof(DWORD), .build = 0, .revision = 0 }
			};
		}
	};
	// CWindowList::BlurBehindChange
	// data[x] & y
	struct CWindowData_GetNonClientAttribute_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 672, .build = 0, .revision = 0 }
			};
		}
	};
	// CWindowList::ForceIconicRepresentationChange
	// CWindowList::AlphaChange
	// data[x] & y
	struct CWindowData_GetClientBlurAttribute_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 673, .build = 0, .revision = 0 }
			};
		}
	};
	struct CWindowData_GetExtendedFrameMargins_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 96, .build = 0, .revision = 0 }
			};
		}
	};
	
	// CWindowList::EnsureTopLevelWindow
	struct CTopLevelWindow_GetData_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 87 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateIcon
	// (CImage *)this[x];
	struct CTopLevelWindow_GetIconVisual_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 65 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CDWriteText::Create
	struct CTopLevelWindow_GetDWriteTextVisual_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 63 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateNCAreaBackground
	// xrefs to CLegacyNonClientBackground::Create
	struct CTopLevelWindow_GetLegacyVisual_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 32 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateClientBlur
	// xrefs to CSolidRectangleVisual::Create
	struct CTopLevelWindow_GetClientBlurVisual_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 35 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CWindowBorder::Create
	struct CTopLevelWindow_GetWindowBorder_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 26 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::SetSuppressBorderUpdates (could be inlined)
	// CTopLevelWindow::UpdateWindowVisuals
	// if ( this[x] )
	//     goto y;
	struct CTopLevelWindow_GetIsBorderUpdatesSuppressed_ByteIndex_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 792, .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CText::SetRTLReading/CVisual::SetRTLMirror
	// CTopLevelWindow::UpdateNCAreaPositionsAndSizes
	// this[184] & 4
	struct CTopLevelWindow_IsWindowMaximized_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 184, .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::GetButtonHeightAndOffset
	// CTopLevelWindow::UpdateNCAreaButton
	// xrefs to CTopLevelWindow::HasThinRenderedBorder
	struct CTopLevelWindow_StateDwordIndex_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 142 * sizeof(DWORD), .build = 0, .revision = 0}
			};
		}
	};
	// CTopLevelWindow::UpdateColorizationColor
	// CTopLevelWindow::UpdateNCAreaBackground
	// if ( y1 == 0x7FFFFFFF && y2 == 0x7FFFFFFF && y3 == 0x7FFFFFFF && y4 == 0x7FFFFFFF )
	//  z = this[x]
	// else
	//  z = this[w]
	struct CTopLevelWindow_GetCaptionColorizationParameters_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 70 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to CButton::Create
	struct CTopLevelWindow_GetButton_BasePointerIndex_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 59 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::GetBorderMargins/CTopLevelWindow::_GetRightFrameThickness
	// if ( y <= 0 )
	//     y = data[x]
	struct CWindowData_GetFrameThickness_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 112, .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateNCAreaPositionsAndSizes
	// x = x1;
	// if ( !zoomed )
	//     x = x2;
	// y = this[x]
	struct CTopLevelWindow_GetFrameOutsideMargins_Zoomed_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 620, .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateNCAreaPositionsAndSizes
	// x = x1;
	// if ( !zoomed )
	//     x = x2;
	// y = this[x]  (Normal path, same function as Zoomed)
	struct CTopLevelWindow_GetFrameOutsideMargins_Normal_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 604, .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::UpdateNCAreaBackground
	// if ( this[x1] == 0x7FFFFFFF && this[x2] == 0x7FFFFFFF && this[x3] == 0x7FFFFFFF && this[x4] == 0x7FFFFFFF )
	//  z = this[y]
	// else
	//  z = this[w]
	//
	// (x = min(x1, x2, x3, x4))
	struct CTopLevelWindow_GetFrameInsideMargins_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 147 * sizeof(DWORD), .build = 0, .revision = 0 }
			};
		}
	};
	// CTopLevelWindow::GetBorderMargins
	// margins = (MARGINS)this[x]
	struct CTopLevelWindow_GetBorderMargins_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 572, .build = 0, .revision = 0 }
			};
		}
	};

	// CDesktopManager::Initialize
	// CGraphicsDeviceManager::Create → stored at this + N
	struct CDesktopManager_GetGraphicsDeviceManager_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 7 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDesktopManager::Initialize
	// CCompositor::Create(connection, &this->compositor)
	struct CDesktopManager_GetCompositor_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 6 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDesktopManager::Initialize / CLivePreview::_FadeOutToGlass
	// *((QWORD *)CDesktopManager::s_pDesktopManagerInstance + N) → CWindowList*
	struct CDesktopManager_GetWindowList_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 53 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDesktopManager::Initialize
	// WICCreateImagingFactory_Proxy(567, &this->wicFactory)
	struct CDesktopManager_GetWICFactory_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 30 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDesktopManager::InitializeHighContrast
	// this[26] = IsHighContrastMode
	struct CDesktopManager_GetIsHighContrastMode_BoolIndex_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 27, .build = 0, .revision = 0 }
			};
		}
	};
	// CWindowList::CheckForMaximizedChange
	// s_pDesktopManagerInstance[N] = hasMaximized
	struct CDesktopManager_HasMaximizedWindows_BoolIndex_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 21, .build = 0, .revision = 0 }
			};
		}
	};
	// CDesktopManager::SetupDPIValues
	// *((double *)this + N) = GetDpiForSystem() / 96.0
	struct CDesktopManager_GetDPIValue_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 52 * (LONGLONG)sizeof(double), .build = 0, .revision = 0 }
			};
		}
	};
	
	// CDesktopManager::EnsureDCompositionInteropDevice → D2D1CreateFactory → stores at this + N
	struct CGraphicsDeviceManager_GetD2DDevice_Index_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 4 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};

	// CDesktopManager::EnsureDCompositionInteropDevice/CCompositor::InitializeInteropCompositor
	// xrefs to RoGetActivationFactory("Windows.UI.Composition.Compositor")
	// QueryInterface GUID_d14b6158_c3fa_4bce_9c1f_b61d8665eab0
	// Retrieves IDCompositionDesktopDevicePartner from CCompositor's DComp device at index 4
	struct CCompositor_GetInteropCompositorDCompDevicePartner_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 4 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
}
