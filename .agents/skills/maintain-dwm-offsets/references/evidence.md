# Evidence and schema semantics

## Sample identity

Verify the binary module (`dwmcore.dll` or `uDWM.dll`), DWM backend (`legacy` or `milcomp`), CPU platform, and actual PE build/revision. Matching PDB GUID and age are required when resolving Symbols or citing PDB evidence. Layout semantic investigation can use an identified PE without an available PDB. File paths and marketing labels (such as `25H2` or `26H1`) are discovery hints only. They never prove shared binary layouts or feature presence.

## Semantic verification

Every production schema offset, slot, and ABI requires an independent semantic cross-check, such as an accessor agreeing with a constructor, dispatcher, or caller. Never write unresolved or inferred values, such as guesses from neighboring members, size deltas, or previous builds, to production schemas. Identical code folding (ICF) can merge identical functions. While an ICF symbol may indicate class or feature presence, ICF alone cannot prove an exact vtable slot.

## Layout intervals

Each schema `until: { build, revision }` uses the same JSON integer version fields as Symbol bindings: a nonzero unsigned 32-bit `build`, and an unsigned 32-bit `revision` defaulting to zero when omitted. Numeric strings and C++ constant names are unsupported. Each case ends at an exclusive right boundary, and the boundary is not the version where a feature first appeared. Cases are evaluated in schema order against the module's exact `(build, revision)`.

- A case with `until` covers supported versions before that boundary and after the preceding boundary.
- A case with `otherwise: true` is the explicit open-ended runtime fallback and must be last. It is not proof that the value remains correct on unanalyzed future builds.
- The absence of an open-ended `otherwise` entry means later versions resolve to `unsupported`. This bounds verified evidence without asserting removal. Claiming actual feature or member removal requires independent semantic proof.
- Multiple boundaries for the same build must have increasing revisions.
- The schema's `offset` is a signed 32-bit JSON integer measured in bytes, including negative adjusted-`this` displacements and vtable byte offsets. Numeric strings, `sizeof`, `offsetof`, and C++ arithmetic are not supported. Representation-only conversions must preserve the numerical values for the selected architecture.

Read `OpenGlass/ProjectionSchemas/README.md`, `OpenGlass/OSHelper.hpp`, `OpenGlass/ProjectionHelper.hpp`, and the target schema item before proposing a change.

## Cross-validation rules

Prefer independent semantic evidence:

1. Named setter/getter plus constructor initialization.
2. Producer plus consumer, such as a channel dispatcher and the object update routine.
3. Base constructor plus derived constructor or adjusted-interface method.
4. Two unrelated callers that use the member for the same role.

Do not count two wrappers around the same implementation as independent evidence. Do not derive one flag merely because another flag is currently adjacent.

## Hook calling-contract evidence

A public PDB prototype establishes the declared function signature, but whole-program optimization may allow callers to rely on callees preserving volatile registers (GPRs, XMM lanes, or EFLAGS). When an inline hook replaces such a call, an ordinary C++ dispatcher can clobber those registers unless preserved by a custom physical dispatcher. Follow [ida-workflow.md#audit-optimized-hook-contracts](ida-workflow.md#audit-optimized-hook-contracts) to audit callers, wrappers, downstream semantic use, and verified clobber boundaries on the exact binary.
