#pragma once
#include "Util.hpp"

// ============================================================================
// dwmcore Offset Stability Rules
// ============================================================================
// dwmcore.dll is recompiled at major build boundaries. Offsets only change at
// these recompile points:
//
//   17763 — Windows 10 1809 (minimum supported)
//   18362 — Windows 10 1903, 1909
//   19041 — Windows 10 2004, 20H2, 21H1, 21H2, 22H2
//   20348 — Windows Server 2022
//   22621 — Windows 11 22H2, 23H2
//   26100 — Windows 11 24H2, 25H2
//   28100 — Windows 11 26H1+ (MIL brush infrastructure partially removed)
//
// 26H1+ Gate: MIL brushes (CImageLegacyMilBrush, CSolidColorLegacyMilBrush,
//   CLegacyMilBrush) are REMOVED. IDrawingContext interface absorbed/removed.
//   COcclusionContext, CDrawingContext core, CD3DDevice still exist.
//   Always gate each class — do NOT assume existence.
//
// Tier 1 — COcclusionContext is the CANARY class: all 6 offsets are the most
// volatile across CUs. If they change, they tend to shift together by the same
// byte delta. Check COcclusionContext FIRST before auditing any other class.
//
// Tier 2 — CDrawingContext WorldTransform shifts between major builds.
//   CRenderData_GetResources grows ~8 bytes per major version.
//
// Tier 3 — Brush family stable within a recompile boundary (but removed 26H1+).
//   CShape vtable slots: consistent across derived classes, stable across builds.
//
// Tier 4 — CD3DDevice/CD2DContext: historically frozen across all Win11 builds.
//
// Interval semantics: each OffsetInfo's .build is the RIGHT boundary of the
// interval where its offset applies. {build=0, revision=0} = last supported.
// No build=0 entry → member was REMOVED (do not search for it).
// ============================================================================

namespace OpenGlass::dwmcore
{
	// Win10 only — absent in later builds
	struct CArrayBasedCoverageSet_GetAntiOccluderArray_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 52 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 49 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 }
			};
		}
	};
	// CArrayBasedCoverageSet::CalcVisibleArea
	// *(QWORD *)this → base pointer; *(DWORD *)(this + 24) → count
	// Always at offset 0 within CArrayBasedCoverageSet
	struct CArrayBasedCoverageSet_GetOccluderArray_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 24, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 0, .build = 0, .revision = 0 }
			};
		}
	};
	// CRenderData::TryDrawCommandAsDrawList
	// *(QWORD *)(renderData + 136) → resource array base pointer
	struct CRenderData_GetResources_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 104, .build = os::build_w10_1903, .revision = 0 },
				Util::OffsetInfo{ .offset = 120, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 128, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 136, .build = 0, .revision = 0 }
			};
		}
	};
	// CSolidColorLegacyMilBrush::GetRealizedColor (~63B)
	// *(D3DCOLORVALUE *)((char *)this + N) — solid color stored inline
	struct CSolidColorLegacyMilBrush_GetRealizedColor_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 88, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 96, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 104, .build = 0, .revision = 0 }
			};
		}
	};
	// CImageLegacyMilBrush::ProcessUpdate (pre-22H2) / TryDrawCommandAsDrawList
	// *((QWORD *)brush + N) → CImageSource* accessed at: *(QWORD *)(brush + 192)
	// Cross-check: CChannel::ImageLegacyMilBrushUpdate (sends image source handle via MIL cmd 561)
	struct CImageLegacyMilBrush_GetImageSource_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 31 * sizeof(ULONG_PTR), .build = os::build_w10_1903, .revision = 0 },
				Util::OffsetInfo{ .offset = 30 * sizeof(ULONG_PTR), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 31 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 31 * sizeof(ULONG_PTR), .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 23 * sizeof(ULONG_PTR), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 24 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// Pre-1903 only: 1809 stores opacity as double at *(double *)(this + N/8).
	// Removed in 1903 (switched to float). Caller gates: if (build < build_w10_1903).
	struct CImageLegacyMilBrush_GetOpacityValue_Double_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 15 * sizeof(double), .build = os::build_w10_1903, .revision = 0 }
			};
		}
	};
	// CSolidColorLegacyMilBrush::GetRealizedColor / CImageLegacyMilBrush::ProcessUpdate (pre-22H2)
	// *(float *)(this + N) — inline opacity (else-branch when float resource is null)
	// Cross-check: CChannel::ImageLegacyMilBrushUpdate (sends opacity via MIL cmd 551)
	//   and uDWM: CImageLegacyMilBrushProxy::Update (24H2+, opacity param)
	struct CImageLegacyMilBrush_GetOpacityValue_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 31 * sizeof(float), .build = os::build_w10_1903, .revision = 0 },
				Util::OffsetInfo{ .offset = 30 * sizeof(float), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 32 * sizeof(float), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 32 * sizeof(float), .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 16 * sizeof(float), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 18 * sizeof(float), .build = 0, .revision = 0 }
			};
		}
	};
	// CImageLegacyMilBrush::ProcessUpdate (pre-22H2) / TryDrawCommandAsDrawList
	// *((QWORD *)this + N) null-checked → CFloatResource* for opacity animation
	// Cross-check: CChannel::ImageLegacyMilBrushUpdate (sends float resource handle via MIL cmd 552)
	struct CImageLegacyMilBrush_GetFloatResource_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16 * sizeof(ULONG_PTR), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 17 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 17 * sizeof(ULONG_PTR), .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 9 * sizeof(ULONG_PTR), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 10 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CImageLegacyMilBrush::ProcessUpdate (pre-22H2): *(_OWORD *)(this + N) = cmd->viewport
	// CImageLegacyMilBrushGeneratedT::SetViewport (22H2+): _mm_loadu_si128(this + N/16)
	// Cross-check: CChannel::ImageLegacyMilBrushUpdate (sends viewport via MIL cmd 539)
	//   and uDWM: CImageLegacyMilBrushProxy::Update (24H2+, passes viewport as MilRectF)
	struct CImageLegacyMilBrush_GetViewport_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 160, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 168, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 168, .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 104, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 112, .build = 0, .revision = 0 }
			};
		}
	};
	// CImageLegacyMilBrush::ProcessUpdate (pre-22H2): *(_OWORD *)((char *)this + N) = cmd->viewbox
	// CImageLegacyMilBrushGeneratedT::SetViewbox (22H2+): _mm_loadu_si128(this + N/16)
	// Cross-check: CChannel::ImageLegacyMilBrushUpdate (sends viewbox via MIL cmd 541)
	//   and uDWM: CImageLegacyMilBrushProxy::Update (24H2+, passes viewbox as MilRectF)
	struct CImageLegacyMilBrush_GetViewbox_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 184, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 192, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 192, .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 120, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 128, .build = 0, .revision = 0 }
			};
		}
	};
	// CShape vtable: CRectanglesShape::GetTightBounds in vtable slot 6 (current) / slot 4 (pre-21H2)
	// slot_index × sizeof(ULONG_PTR)
	struct CShape_GetTightBounds_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 32, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 48, .build = 0, .revision = 0 }
			};
		}
	};
	// CShape vtable: CRectanglesShape::IsRectangles in vtable slot 8 (current) / slot 5 (pre-21H2)
	// slot_index × sizeof(ULONG_PTR)
	struct CShape_IsRectangles_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 40, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 64, .build = 0, .revision = 0 }
			};
		}
	};
	// CShape vtable: CRectanglesShape::GetRectangles in vtable slot 10 (current) / slot 6 (pre-21H2)
	// slot_index × sizeof(ULONG_PTR)
	struct CShape_GetRectangles_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 48, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 80, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::Create — IDrawingContext vtable at CDrawingContext + 16
	// Negative offset: IDrawingContext → CDrawingContext base = -16
	// Pre-2004: IDrawingContext was at +0 (absorbed into base vtable)
	struct IDrawingContext_GetDrawingContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 0, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = -16, .build = 0, .revision = 0 }
			};
		}
	};
	// xrefs to ConvertSDRBoostToSDRWhiteLevel
	// Checked in IsHDRRenderTarget / GetRenderTargetInfo: *(float *)(this + 16)
	struct RenderTargetInfo_GetSDRBoost_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
			};
		}
	};
	// ID2DContextOwner vtable from CDrawingContext::Create (at this + 24).
	// Read the vtable — function names in each slot are self-explanatory.
	// GetCurrentZ: slot 4 (current) / slot 3 (24H2) / slot 2 (pre-21H2)
	// slot_index × sizeof(ULONG_PTR)
	struct ID2DContextOwner_GetCurrentZ_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 32, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 24, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 8, .build = 0, .revision = 0 }
			};
		}
	};
	// ID2DContextOwner vtable from CDrawingContext::Create (at this + 24).
	// GetCurrentRenderTargetInfo: slot 10 (current) / slot 5 (24H2) / slot 7 (pre-21H2)
	// slot_index × sizeof(ULONG_PTR)
	struct ID2DContextOwner_GetCurrentRenderTargetInfo_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 80, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 40, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
			};
		}
	};
	// CD2DContext::GetDeviceContext / CD3DDevice structure
	// D2D1 device context embedded at CD3DDevice + N (via D2D1 API calls)
	struct CD2DContext_GetDeviceContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 29 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 30 * sizeof(ULONG_PTR), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 25 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 25 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CD3DDevice — historically frozen across all Win11 builds
	// Decompile CD3DDevice::CreateTexture → trace the ID3D11Device::CreateTexture2D call,
	// the ID3D11Device* is loaded from *(QWORD *)(this + N)
	struct CD3DDevice_GetDevice_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 79 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 74 * sizeof(ULONG_PTR), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 69 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 69 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CD3DSurface only used before Windows 10 2004
	// Sequential pointer members: Texture2D, RTV, SRV
	struct CD3DSurface_GetTexture2D_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 }
			};
		}
	};
	struct CD3DSurface_GetShaderResourceView_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 25 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 }
			};
		}
	};
	struct CD3DSurface_GetRenderTargetView_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 24 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 }
			};
		}
	};
	// IRenderTarget vtable only used before Windows 10 2004, not present in Win11+
	// slot_index × sizeof(ULONG_PTR)
	struct IRenderTarget_GetTargetSurfaceNoRef_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 13 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 }
			};
		}
	};
	struct IRenderTarget_GetSDRBoost_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 19 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 }
			};
		}
	};
	// IDeviceTarget vtable — known systemic ICF, unresolvable
	// This function slot has been merged with CPassthroughEffect/CKernelTransport symbols across builds
	// slot_index × sizeof(ULONG_PTR)
	struct IDeviceTarget_GetRenderTargetView_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 7 * sizeof(ULONG_PTR), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 22 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDeviceTextureTarget constructor — IDeviceTarget at +256, IDeviceTexture at +240
	// Negative offset = IDeviceTexture_offset - IDeviceTarget_offset
	struct IDeviceTarget_GetDeviceTexture_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = -24, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = -16, .build = 0, .revision = 0 }
			};
		}
	};
	// CDeviceTextureTarget has complex inheritance (possible diamond):
	//   CD2DBitmap → CDeviceResourceT → CMILCOMBaseT → ...
	//   9+ vtable pointers: ID2DBitmapCacheSource, IPixelFormat, IBitmapUnlock,
	//   IDeviceResource (standalone + embedded in combined vtables for
	//   IDeviceTexture and IDeviceTarget).
	// Focus on extracting the specific offset, not full inheritance graph.
	// CDeviceTextureTarget::GetTexture2D at vtable slot 0 (24H2) / slot 15 (earlier)
	// slot_index × sizeof(ULONG_PTR)
	struct IDeviceTexture_GetTexture2D_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 0 * sizeof(ULONG_PTR), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 15 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDeviceTextureTarget::GetShaderResourceView at vtable slot 1 (24H2) / slot 16 (earlier)
	// slot_index × sizeof(ULONG_PTR)
	struct IDeviceTexture_GetShaderResourceView_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 1 * sizeof(ULONG_PTR), .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 16 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CD2DContext::GetDevice
	// return (CD3DDevice *)(this - N) → N is the CD2DContext offset within CD3DDevice
	struct CD3DDevice_GetD2DContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 0, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::FlushD2D
	// *((QWORD *)this + 5) → CD3DDevice* (non-virtual, raw this)
	struct CDrawingContext_GetD3DDevice_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 48 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 5 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CD3DDevice — adjacent pointer to GetDevice
	// GetImmediateContext = GetDevice + 1×PTR
	struct CD3DDevice_GetImmediateContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 80 * sizeof(ULONG_PTR), .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 75 * sizeof(ULONG_PTR), .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 70 * sizeof(ULONG_PTR), .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 70 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::Create
	// IDrawingContext vtable assigned at this + 16 (pre-26H1; absorbed/removed in 26H1+)
	struct CDrawingContext_GetInterface_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 0, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::Create / CDrawingContext::FlushD2D
	// ID2DContextOwner vtable at this + 24; this + N cast to ID2DContextOwner*
	struct CDrawingContext_GetD2DContextOwner_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 24, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::FlushD2D / UpdateDeviceTransform
	// *((QWORD *)this + N/8) → COcclusionContext* — most volatile across builds
	struct CDrawingContext_GetOcclusionContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 6272, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 5936, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 5896, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 7944, .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 8072, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 7960, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::GetWorldTransform3x2 (virtual, slot 0 of ID2DContextOwner vtable)
	// this adjusted to CDrawingContext + 24 (ID2DContextOwner subobject)
	// *(QWORD *)(this + N/8) → matrix stack data pointer; *(DWORD *)(this + N/4) → count
	// Actual base offset = subobject_offset(24) + adjusted_offset
	struct CDrawingContext_GetWorldTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 480, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 408, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 432, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 368, .build = os::build_w11_22h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 400, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 288, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::UpdateDeviceTransform
	// CMILMatrix::Multiply((char *)this + N, ...) — device transform matrix embedded inline
	// Major recompile in 21H2 moved this from offset 3648 into early object (offset 96)
	struct CDrawingContext_GetDeviceTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 3648, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 96, .build = 0, .revision = 0 }
			};
		}
	};
	// Removed in 24H2+ — OcclusionContext now passed as parameter to GetOptimizedRect
	// Pre-24H2: CDirtyRegion + 16 stored the COcclusionContext pointer
	struct CDirtyRegion_GetOcclusionContext_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16, .build = os::build_w11_24h2, .revision = 0 }
			};
		}
	};
	// COcclusionContext::IsCurrent (~30B)
	// *((QWORD *)this + 3) == *((QWORD *)g_pComposition + 111) → global frame ID comparison
	struct COcclusionContext_GetFrameId_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 24, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext::FlushOcclusionRects
	// *(DWORD *)(this + N) read, then value += 2 written back — unique "+2" pattern
	// Adjacent DWORD (+4) is the push-skip counter — do NOT confuse
	// NOTE: GetCurrentZ, GetDeviceTransform, GetDeviceTransformFlag, and
	// GetArrayBasedCoverageSet tend to shift by the SAME byte delta across CUs.
	// If all four differ by the same amount, trust the data.
	struct COcclusionContext_GetCurrentZ_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 1032, .build = os::build_w10_1903, .revision = 752 },
				Util::OffsetInfo{ .offset = 1040, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 1456, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 1396, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1428, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1316, .build = os::build_w11_24h2, .revision = os::revision_24h2_with_25h2_code_staged },
				Util::OffsetInfo{ .offset = 1708, .build = os::build_w11_24h2, .revision = os::revision_kb5083631 },
				Util::OffsetInfo{ .offset = 1684, .build = os::build_w11_24h2, .revision = os::revision_kb5101684 },
				Util::OffsetInfo{ .offset = 1668, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext constructor / GetWorldTransform
	// *(QWORD *)(this + N) → matrix stack data; *(DWORD *)(this + N + 8) → count
	struct COcclusionContext_GetWorldTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8, .build = os::build_w10_1903, .revision = 752 },
				Util::OffsetInfo{ .offset = 16, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 24, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 32, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 32, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext::UpdateDeviceTransform
	// CMILMatrix::Multiply((char *)this + N, a2, result) — device-space matrix embedded inline
	struct COcclusionContext_GetDeviceTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 820, .build = os::build_w10_1903, .revision = 752 },
				Util::OffsetInfo{ .offset = 828, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 1248, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 1176, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1208, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1180, .build = os::build_w11_24h2, .revision = os::revision_24h2_with_25h2_code_staged },
				Util::OffsetInfo{ .offset = 1572, .build = os::build_w11_24h2, .revision = os::revision_kb5083631 },
				Util::OffsetInfo{ .offset = 1548, .build = os::build_w11_24h2, .revision = os::revision_kb5101684 },
				Util::OffsetInfo{ .offset = 1532, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext::FlushOcclusionRects / UpdateDeviceTransform
	// if (*((BYTE *)this + N)) → use device transform, else use nullptr
	// Invariant: DeviceTransformFlag = DeviceTransform_Offset - 4 or -8
	struct COcclusionContext_GetDeviceTransformFlag_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 816, .build = os::build_w10_1903, .revision = 752 },
				Util::OffsetInfo{ .offset = 824, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 1244, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 1172, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1204, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 1172, .build = os::build_w11_24h2, .revision = os::revision_24h2_with_25h2_code_staged },
				Util::OffsetInfo{ .offset = 1564, .build = os::build_w11_24h2, .revision = os::revision_kb5083631 },
				Util::OffsetInfo{ .offset = 1540, .build = os::build_w11_24h2, .revision = os::revision_kb5101684 },
				Util::OffsetInfo{ .offset = 1524, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext::FlushOcclusionRects
	// CArrayBasedCoverageSet::Add((char *)this + N, ...) — DynArray at byte offset N
	// Constructor initializes inline buffer at this + N + 16
	struct COcclusionContext_GetArrayBasedCoverageSet_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 392, .build = os::build_w10_1903, .revision = 752 },
				Util::OffsetInfo{ .offset = 400, .build = os::build_w10_2004, .revision = 0 },
				Util::OffsetInfo{ .offset = 408, .build = os::build_server_2022, .revision = 0 },
				Util::OffsetInfo{ .offset = 416, .build = os::build_w11_21h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 448, .build = os::build_w11_24h2, .revision = 0 },
				Util::OffsetInfo{ .offset = 568, .build = os::build_w11_24h2, .revision = os::revision_24h2_with_25h2_code_staged },
				Util::OffsetInfo{ .offset = 616, .build = os::build_w11_24h2, .revision = os::revision_kb5083631 },
				Util::OffsetInfo{ .offset = 592, .build = os::build_w11_24h2, .revision = os::revision_kb5101684 },
				Util::OffsetInfo{ .offset = 576, .build = 0, .revision = 0 }
			};
		}
	};
}
