#pragma once
#include "Util.hpp"

namespace OpenGlass::dwmcore
{
	// CColorBrush
	struct CColorBrush_GetColor_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 104, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 0, .build = 0, .revision = 0 }
			};
		}
	};

	// Render target info
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
	// GetCurrentZ: slot ?
	// slot_index × sizeof(ULONG_PTR)
	struct ID2DContextOwner_GetCurrentZ_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 8, .build = 0, .revision = 0 }
			};
		}
	};
	// ID2DContextOwner vtable from CDrawingContext::Create (at this + 24).
	// GetCurrentRenderTargetInfo: slot ?
	// slot_index × sizeof(ULONG_PTR)
	struct ID2DContextOwner_GetCurrentRenderTargetInfo_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
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
				Util::OffsetInfo{ .offset = 25 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CD3DDevice
	// Decompile CD3DDevice::CreateTexture → trace the ID3D11Device::CreateTexture2D call,
	// the ID3D11Device* is loaded from *(QWORD *)(this + N)
	struct CD3DDevice_GetDevice_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 68 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 69 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
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
	// CDeviceTextureTarget::GetTexture2D at vtable slot ?
	// slot_index × sizeof(ULONG_PTR)
	struct IDeviceTexture_GetTexture2D_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 15 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDeviceTextureTarget::GetShaderResourceView at vtable slot ?
	// slot_index × sizeof(ULONG_PTR)
	struct IDeviceTexture_GetShaderResourceView_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::FlushD2D
	// *((QWORD *)this + 4) → CD3DDevice* (non-virtual, raw this)
	struct CDrawingContext_GetD3DDevice_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 4 * sizeof(ULONG_PTR), .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::Create / CDrawingContext::FlushD2D
	// ID2DContextOwner vtable at this + 16; this + N cast to ID2DContextOwner*
	struct CDrawingContext_GetD2DContextOwner_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::GetWorldTransform3x2 (virtual, slot 0 of ID2DContextOwner vtable)
	// this adjusted to CDrawingContext + 16 (ID2DContextOwner subobject)
	// *(QWORD *)(this + N/8) → matrix stack data pointer; *(DWORD *)(this + N/4) → count
	// Actual base offset = subobject_offset(16) + adjusted_offset
	struct CDrawingContext_GetWorldTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 280, .build = 0, .revision = 0 }
			};
		}
	};
	// CDrawingContext::UpdateDeviceTransform
	// CMILMatrix::Multiply((char *)this + N, ...) — device transform matrix embedded inline
	struct CDrawingContext_GetDeviceTransform_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 88, .build = 0, .revision = 0 }
			};
		}
	};
	// COcclusionContext::IsCurrent (~30B)
	// *((QWORD *)this + 2) == *((QWORD *)g_pComposition + 111) → global frame ID comparison
	struct COcclusionContext_GetFrameId_Offsets
	{
		consteval static auto operator()()
		{
			return std::array{
				Util::OffsetInfo{ .offset = 16, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 1268, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 24, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 1132, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 1124, .build = 0, .revision = 0 }
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
				Util::OffsetInfo{ .offset = 584, .build = 0, .revision = 0 }
			};
		}
	};
}
