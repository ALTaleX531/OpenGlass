# Evidence and schema semantics

## Sample identity

Record all available identity before classifying a binary:

| Field | Use |
|---|---|
| Module and architecture | Prevent cross-module or x86/x64 comparisons |
| PE/file version | Establish the actual build and revision |
| PDB GUID and age | Distinguish recompiles with similar marketing labels |
| SHA-256 or MD5 | Make the analyzed artifact reproducible |
| Path or operator label | Discovery hint only; never primary proof |

Windows marketing releases, OS build trains, and compositor capabilities are separate axes. A marketing label does not prove that two binaries share layouts. Likewise, a missing legacy capability does not imply every unrelated class was removed.

## Confidence levels

- **Verified**: a semantic accessor/mutator and an independent constructor, dispatcher, caller, or callee agree.
- **Provisional**: one strong semantic access exists but no independent confirmation is available.
- **Inferred**: correlation with neighboring members, size deltas, or a prior build only. Never write inferred values to production schemas.
- **Unverified**: the relevant code path or sample identity cannot be established.

Optimized identical-code folding can merge functions. A merged symbol can support class or capability presence, but it does not by itself identify the intended vtable slot. Report ICF ambiguity explicitly.

## Layout intervals

Each schema `until: { build, revision }` case ends at an exclusive right boundary; the boundary is not the version where a feature first appeared. Cases are evaluated in schema order against the module's exact `(build, revision)`.

- A case with `until` covers supported versions before that boundary and after the preceding boundary.
- A case with `otherwise: true` is the explicit open-ended runtime fallback and must be last. It is not proof that the value remains correct on unanalyzed future builds.
- The absence of a terminal entry can be intentional when a member, class, or feature ceased to exist.
- Multiple boundaries for the same build must have increasing revisions.
- The schema's `offset` string is C++ source text. Linters and agents preserve it verbatim and never evaluate it.

Read `OpenGlass/ProjectionSchemas/README.md`, `OpenGlass/OSHelper.hpp`, and the target schema item before proposing a change.

## Cross-validation rules

Prefer independent semantic evidence:

1. Named setter/getter plus constructor initialization.
2. Producer plus consumer, such as a channel dispatcher and the object update routine.
3. Base constructor plus derived constructor or adjusted-interface method.
4. Two unrelated callers that use the member for the same role.

Do not count two wrappers around the same implementation as independent evidence. Do not derive one flag merely because another flag is currently adjacent.

## Hook calling-contract evidence

A correct PDB prototype establishes the declared function ABI, not the complete contract of an optimized internal call site. MSVC whole-program optimization can know the actual clobber set of a callee and reuse a register or condition code that the public Windows x64 ABI otherwise classifies as volatile. A detour inserts replacement, dispatch, and rundown code that the original compiler never analyzed, so an ordinary C++ dispatcher may violate that private contract even when every declared argument and return type is correct.

For each inline projected hook on the selected exact binary:

1. Enumerate every direct caller and any short call or tail-jump wrapper between the semantic caller and the hooked body.
2. Track `RCX`, `RDX`, `R8`, `R9`, `R10`, `R11`, `XMM0` through `XMM5`, and EFLAGS from before the hooked call until they are overwritten or semantically consumed. Include values forwarded as arguments to the next call.
3. Prove that a candidate affects a real downstream value or control decision on a feasible path. Syntactic register reads alone are insufficient.
4. Cross-check the callee and every intervening wrapper actually preserve the value on the relevant path in the same PE. A prior revision or a matching complete symbol name is not proof.
5. Classify a verified dependency separately from the projected function-pointer ABI. Recommend a custom physical dispatcher only for the exact version interval supported by the evidence.

Common false positives include path-insensitive CFG merges; `sbb reg,reg`, whose result is independent of the old destination value; an argument register cleared or overwritten immediately in the next callee; and scalar `movss`/`movsd` writes whose preserved lanes are never consumed. Record how each plausible candidate was eliminated.

The uDWM 26100.8972 `SetMargin` case is the reference example: `UpdateMarginsDependentOnStyle` calls a three-argument `SetMargin` wrapper; that wrapper calls the hooked six-argument helper and propagates the helper's preservation of `RCX`; the outer caller then reuses the original `RCX`. The declared six-argument prototype remains correct, but a normal C++ dispatcher can clobber the private live-through value. This evidence justifies the version-specific `CustomDispatchDetour` assembly shim and does not imply that other `SetMargin` revisions or DWM hooks share the contract.

## Audit record

For every item, record:

| Item | Value |
|---|---|
| Schema ID | Exact stable Layout, VtableSlot, or Symbol ID |
| Semantic role | Member, subobject displacement, or vtable slot |
| Value and unit | Byte offset or slot multiplied by pointer size |
| Primary evidence | Function and decisive expression/instruction |
| Cross-check | Independent evidence or `none` |
| Status | verified, provisional, removed, absent, ICF, or unverified |
| Suggested interval | `until` right boundary, `otherwise`, or none |

An audit is complete only when every projection consumed by the checked-out implementation has a record. A count copied from an older skill or generated manifest is not a completion criterion.
