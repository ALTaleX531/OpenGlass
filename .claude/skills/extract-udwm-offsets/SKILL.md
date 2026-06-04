---
name: extract-udwm-offsets
description: Extract all udwm.dll struct member and vtable offsets from the current IDA instance. Follow 7 phases in order — do NOT skip. Output ready-to-paste C++ for uDwmProjection.Offsets.hpp.
disable-model-invocation: true
allowed-tools: Bash(git *) mcp__ida-pro-mcp__*
---

# uDWM Offset Extraction

Extract offsets from the udwm.dll currently loaded in IDA Pro. Follow every phase, in order. Do not skip. Do not summarize until the final phase.

## Build-agnostic principles

- **uDWM is stable within codebase generations.** Offsets only change at recompile boundaries, not at every Windows revision. For example, 2004 through Win10 22H2 all share the 19041 binary; 24H2 and 25H2 share the 26100 binary; 26H1+ uses the 28100 binary where MIL infrastructure has been removed.
- **Constructor first** — each class's constructor reveals the full member layout. Decompile it before chasing individual accessors.
- **Named confirmation functions** — each offset has a specific function that accesses it (listed in each phase). Go there directly; do not infer from nearby code in unrelated functions.
- **No "Get" thunks** — uDWM member access is inlined. Do NOT search for `GetOffset`, `GetSize`, `GetHwnd` as standalone functions.

## Supporting files

- **[decompilation.md](decompilation.md)** — read BEFORE starting Phase 1. Covers pseudocode patterns, offset table interval semantics, codebase generation rules, `.hpp` output rules including feature gates.
- **[classes.md](classes.md)** — read when you need class hierarchy context. Covers uDWM class relationships, build watersheds, and codebase stability rules.

## Per-offset lookup guidance

Each offset struct in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp) now has inline comments documenting:
- The primary accessor function(s) and how to identify them
- Pseudocode patterns from the decompiler
- Cross-reference functions (xrefs)
- WARNING notes for easily confused members

**When re-extracting offsets for a new build**, read the comment above each struct first — it tells you exactly which function to decompile and what pattern to look for.

## Phase 0 — Identify the build

1. `survey_binary` → record `md5`, path hint
2. Check `OSHelper.hpp` for existing build constants
3. **Feature-based classification**

| Feature check | Means at least |
|---|---|
| MIL compositor removed; `CContainerVisual` base class restructured | Windows 11 26H1 (28100+) |
| `CContainerVisual` present (search `??0CContainerVisual` or check if CTopLevelWindow ctor takes `CWindowData*`) | Windows 11 24H2 |
| `CDWriteText` present (search `??0CDWriteText` or `CDWriteText::Create`) | Windows 11 22H2 |
| `CAcrylicSheet` present (search `??0CAcrylicSheet`) instead of `CAnimatedGlassSheet` | Windows 11 21H2 |

**Feature gates**:
- `GetSystemBackdropType` → 21H2+ only (WinUI 3 backdrop feature)
- `GetDWriteTextVisual_Index` → 22H2+ (not 24H2+; appears with CDWriteText)
- `_OnLegacy` variants (GetBorderGeometry/CaptionGeometry/CaptionBrush/BorderBrush) → 22H2+ (CLegacyNonClientBackground introduced)
- `CWindowBorder` / `GetWindowBorder_Index` / `GetIsBorderUpdatesSuppressed` → 21H2+ (present in 21H2 RTM; do NOT assume 22H2+)
- `CTopLevelWindow` base class: `CVisual` (pre-24H2) → `CContainerVisual` (24H2+)
- `CAnimatedGlassSheet` → pre-21H2; replaced by `CAcrylicSheet` in 21H2+

> [!IMPORTANT]
> The hpp entry's `.build` field is the interval **right boundary**, NOT the feature introduction point. A feature may exist in builds BEFORE the first hpp entry. Always search the binary for the class/function (e.g. `CWindowBorder`, `SetSuppressBorderUpdates`) to confirm existence rather than inferring absence from the hpp chain.

Output the build classification and expected codebase generation before continuing.

> [!IMPORTANT]
> **Do NOT blindly trust existing `.hpp` entries.** Every offset must be independently verified from the actual IDA data. However, when your reading disagrees with the `.hpp`, double-check your interpretation before claiming the `.hpp` is wrong — it has been validated across many builds. Also check for unit mistakes: a byte flag entry using `sizeof(ULONG_PTR)` is likely wrong (e.g. `128 * sizeof(ULONG_PTR)` should be just `128`).

## Phase 1 — CVisual (11 offsets)

**Gate check**: search for the CVisual constructor (`??0CVisual@@IEAA@XZ`). Decompile it. If absent, CVisual removed — skip phase.

For each offset, see the comment in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp) for the exact accessor function and pseudocode pattern.

### Step 1: Decompile the constructor

It initializes members to 0, 1.0f (float) or 0x3FF0000000000000 (double 1.0), or sentinel values like -1, -2, 0x7FFFFFFF. Each assignment reveals a member:

| Member | Constructor pattern |
|---|---|
| VisualProxy | `*(QWORD*)(this+N) = 0` — first pointer after vtable |
| TransformParent (member) | `*(QWORD*)(this+N) = 0` — adjacent pointer slot |
| VisualCollection | `*(QWORD*)(this+N) = &VisualCollection::vftable` (pre-22H2) or absent (22H2+) |
| Scale (D2D1) | `*(float*)(this+N) = 1.0f` ×2 (X and Y adjacent floats) |
| Scale (MilSizeD) | `*(double*)(this+N) = 1.0` (pre-22H2) or separate from D2D scale (22H2+) |
| Inset sentinels | `*(DWORD*)(this+N) = 0x7FFFFFFF` repeated 4 times in sequence |

Map each to a named offset by comparing with the existing `.hpp` entry that matches the same codebase generation.

### Step 2: Cross-validate with Set* functions

| Offset | Set* accessor | Assembly pattern |
|---|---|---|
| GetScale_D2D1_SIZE_F | `SetScale@CVisual` | `*((float*)this + N)` ×2 — two adjacent floats |
| GetSize | `SetSize@CVisual` | `[rcx+N]` for SIZE width/height — two DWORD compares then store |
| GetOffset | `SetOffset@CVisual` | `[rcx+N]` for POINT x/y — same pattern |
| GetDirtyFlags | `SetDirtyFlags@CVisual` | `[rcx+N]` DWORD read-modify-write |
| IsRTLMirrored | `SetRTLMirror@CVisual` | `[rcx+N]` byte bit-test (reads byte, toggles bit) |
| SetExcludeSubtree | Same function or nearby | Different bit at same or different byte |

### Step 3: Vtable slot offsets

Read the CVisual vtable (address from constructor: `*(QWORD*)this = &CVisual::vftable`). Decode 8-byte slots, look up function addresses.

| Offset | How to identify |
|---|---|
| GetTransformParent | Find `GetTransformParent@CVisual` in vtable → slot × 8 |
| _ValidateVisual | Find `ValidateVisual@CVisual` in vtable → slot × 8 |
| GetDirtyFlags (vtable) | Find `SetDirtyFlags@CVisual` in vtable → slot × 8 |

**Critical**: these are VTABLE SLOT indices, not member data offsets. The function body reads a member at a fixed offset (e.g. `[rcx+18h]` for GetTransformParent), but the `.hpp` offset is `slot_index × sizeof(ULONG_PTR)`.

## Phase 2 — CWindowData (15 offsets)

**Gate check**: functions like `CWindowList::BlurBehindChange` or `CLivePreview::_CollectWindows` reference CWindowData members. If absent, class removed.

Most CWindowData getters are inlined. See [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp) for the exact accessor function for each offset.

| Offset | Accessor | How to identify |
|---|---|---|
| GetHwnd | `CLivePreview::_CollectWindows` | `*(HWND*)(data+40)` — 5×PTR; HWND is 8 bytes on x64 |
| GetWindowContext | Constructor or `GetWindowBand` | `*(QWORD*)(this+N)` — 3×PTR |
| GetWindow_Index | `CWindowList::BlurBehindChange` | `*(QWORD*)(data+N)` → pointer back to CTopLevelWindow |
| GetAccentPolicy | `CTopLevelWindow::UpdateAccent` or `CAccent::UpdateAccentPolicy` | `*(QWORD*)(this+N)` |
| GetSystemBackdropType | 21H2+ only. `CWindowList::SetSystemBackdropType` (21H2+) or `CTopLevelWindow::GetEffectiveSystemBackdropType` (22H2+) | `*(DWORD*)(this+N×4)` — SetSystemBackdropType writes the DWORD directly |
| GetWindowDPI | `CTopLevelWindow::CalculateOutsideMargins` | `*(unsigned int*)(this+N×4)` |
| GetNonClientAttribute | `CWindowList::BlurBehindChange` | `*(BYTE*)(data+N)` bit-test — **not necessarily near ClientBlur** |
| GetClientBlurAttribute | Same function | `*(BYTE*)(data+N)` bit-test — verify independently, do NOT assume NonClient+1 |
| GetWindowRect | `CLivePreview::_CollectWindows` | `*(RECT*)(data+N)` |
| GetClientMargins | Constructor or `CalculateOutsideMargins` | `*(MARGINS*)(this+N)` |
| GetExtendedFrameMargins | Same | `*(MARGINS*)(this+N)` |

## Phase 3 — CTopLevelWindow (25+ offsets)

**Gate check**: find `Create@CTopLevelWindow` or the constructor. Record allocation size and vtable assignments.

**First**: decompile the constructor. All pointer-index slots are zero-initialized in sequence. The constructor also initializes MARGINS structures at fixed byte offsets.

**Key structural notes**:
- Pre-24H2: base class is `CVisual`, constructor takes no parameters
- 24H2-25H2: base class is `CContainerVisual`, constructor takes `CWindowData*`
- 26H1+ (28100): class hierarchy may be restructured; MIL-dependent members and methods removed. Verify all existing offsets against the binary — do not assume any survive from 26100.
- In 22H2+: CWindowData reverse-link established in constructor or `BlurBehindChange`: `*(QWORD*)(data + GetWindow_Index) = this`

For each specific offset, see the comment in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp).

## Phase 4 — CButton, CText, CAccent, CRenderData, CGeometry

For each offset, see the comment in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp) for the exact accessor function and pseudocode pattern.

Key gotchas documented in the offset comments:
- **CButton_GetGlyphOpacity**: Use `CButton::SetVisualStates`, NOT `UpdateCurrentGlyphOpacity`
- **CRenderDataVisual_GetInstructions**: Use `AddInstruction`/`ClearInstructions`, NOT `WriteInstruction`
- **CAtlasedImage_GetPartId**: Use `ShouldDrawAtlasImage`/`ShouldCloneAtlasImage`, NOT `SetDirtyFlags`
- **CVisual_GetScale_D2D1_SIZE_F**: Check whether `SetScale` takes `float` or `double` params
- **CVisual_GetScale_MilSizeD**: Same function — if `SetScale` takes `double`, MilSizeD is in use and D2D1 scale may not exist

## Phase 5 — CLivePreview, CDesktopManager, CCompositor

**Gate check**: verify `CLivePreview::Create` and `CDesktopManager::Initialize` exist. If absent, skip affected offsets.

For each offset, see the comment in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp).

Key structural notes:
- **CDesktopManager_GetCompositor** index shifted at 22H2 (index 5 → 6)
- **CDesktopManager_GetWICFactory** has mixed unit types across builds (absolute byte offset vs `sizeof(ULONG_PTR)` index)
- **CDesktopManager_GetLivePreview** index moved down by ~7 slots from 21H2 to 24H2
- **Pre-21H2 direct fields** (`GetD2DDevice_OnThis`, `GetDCompDevice_OnThis`): removed in 21H2+ when CGraphicsManager/CInteropManager took over. No `build=0`.
- **CCompositor_GetChannel**: pre-24H2 uses index 2, 24H2+ uses index 3

## Phase 6 — LivePreviewResource, CAnimatedGlassSheet

**Gate check**: `CLivePreview::_UpdateResourcesForMonitor` must exist. `CAnimatedGlassSheet` is pre-21H2 only; if using 21H2+, verify `CAcrylicSheet` instead.

For each offset, see the comment in [uDwmProjection.Offsets.hpp](../../../OpenGlass/uDwmProjection.Offsets.hpp).

Key gotchas:
- **LivePreviewResource_GetMonitorRect**: Use `_UpdateResourcesForMonitorHelper` (first `IntersectRect` call, third arg is MonitorRect), NOT `_UpdateResourcesForMonitor`
- **IsWindowBoundingRectNotEmpty** / **IsGlassBoundingRectNotEmpty**: BYTE flags at the end of the struct — use plain byte offset, NOT `×sizeof(ULONG_PTR)` or `×PTR`
- **CAnimatedGlassSheet**: All RECT offsets use `sizeof(RECT)` units. Atlas padding RECTs are found in `Initialize` near the end of the method

## Phase 7 — Final report

```
================================================================
COMPLETE uDWM OFFSET REPORT — <build description>
image_size: 0x...  functions: N  codebase: <17763|18362|19041|22000|22621|26100|28100>
================================================================

CHANGED FROM PREVIOUS BUILD:
  OffsetName: old → new (source function)

UNCHANGED:
  [✓] OffsetName  value  (source function)

SKIPPED — CLASS/FEATURE REMOVED:
  [✗] OffsetName — reason (e.g. "GetSystemBackdropType absent pre-21H2")

REMOVED IN THIS BUILD (member gone, class still exists):
  [✗] OffsetName — no code accesses this member

NOT VERIFIED (accessor not found, may be renamed):
  [?] OffsetName — reason

TOTAL: N verified (M changed, X unchanged), Y skipped/removed, Z not verified
================================================================
```

Most uDWM offsets only change at recompile boundaries. Within the same codebase generation, they are identical. The most volatile: CTopLevelWindow pointer indices, CWindowData size-based offsets, and MARGINS positions.

**Before emitting C++**: Apply output rules from decompilation.md. Member exists → `build=0` last entry. Member removed → no `build=0`. Feature gated (e.g. 21H2+) → first entry names the gate build.

Only after all phases complete and report is shown, ask whether the user wants C++ emitted.
