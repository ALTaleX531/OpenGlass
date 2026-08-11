# IDA MCP workflow

Use MCP operations by capability rather than assuming a particular client-side tool spelling. Keep analysis read-only unless the user asks to annotate or change the IDB.

## Start and route

1. List the reachable IDA instances.
2. Match instances by module and recorded path; do not rely on whichever instance is active.
3. Select one instance and survey it before deeper queries.
4. Record module, architecture, image base/size, function count, and hashes.
5. Immediately before every query or small batch, reselect the intended instance and confirm its module/path identity.
6. Query small groups of functions. Reconfirm the selected instance when switching samples.

The current MCP instance selection is shared global routing state. Do not analyze different IDA instances in parallel: another agent or request can switch the target between calls. Serialize instance work and treat an unexpected module, path, image base, or hash as a routing failure rather than binary evidence.

Do not hard-code ports in repository files or reports. Ports are session routing details, not sample identity.

## Read-only capability map

Use the available equivalent of:

- instance listing and selection;
- binary survey and metadata inspection;
- function/name queries with narrow filters;
- decompilation and disassembly;
- xrefs, callers, callees, and call graphs;
- byte, integer, string, and vtable reads;
- type inspection when trustworthy.

Avoid rename, comment, type application, patch, undefine, or IDB save operations during an audit-only request.

## Query sequence

1. Gate the class or capability with exact and wildcard symbol searches.
2. Use byte patterns, historical instructions, and register choices only to discover candidates; compiler inlining and register allocation make them unsuitable as final proof.
3. Decompile the smallest decisive semantic function first.
4. Inspect disassembly when pseudocode hides units, adjusted `this`, bit fields, or an indirect call.
5. Follow xrefs to one independent confirmation path.
6. Compare the same semantic functions across samples; never compare raw addresses.

## Audit optimized hook contracts

When the selected item is an inline hook, extend the normal query sequence beyond the callee prototype:

1. List all code xrefs to the hooked address. Follow direct call wrappers and tail-jump thunks outward until reaching semantic callers.
2. At each call site, inspect disassembly on each feasible successor path. Look for volatile GPR, XMM, or EFLAGS values that remain live after the call, including reuse as arguments to another function.
3. Inspect the hooked callee and every wrapper to confirm the candidate value is preserved on the relevant path. Do not assume the public ABI's volatile classification describes the optimizer's private clobber model.
4. Inspect the eventual consumer. A call-site register assignment is not evidence when the next callee ignores or overwrites that argument.
5. Compare an exact older or newer binary when available to determine whether the dependency is revision-specific. Re-run PE/PDB pairing for every comparison sample.

Automated data-flow scans are discovery aids. Manually reject infeasible CFG merges, algebraically value-independent read/write idioms such as `sbb reg,reg`, unused call arguments, and unconsumed upper XMM lanes. Also inspect condition-code readers (`jcc`, `cmovcc`, `setcc`, `adc`, and `sbb`) before the next flag-defining instruction. Report unresolved indirect-call boundaries as unverified rather than assuming preservation.

If the dependency is verified, capture the complete call chain, decisive instructions, preserved state, exact PE/PDB identity, affected version interval, and why ordinary dispatcher code can clobber it. Keep this evidence separate from the declared Symbol signature; the implementation remedy is normally a narrowly scoped custom physical dispatcher, not a fabricated ABI variant.

If a batch request is rejected or too large, split it into smaller read-only requests. A missing exact symbol is a cue to search semantic anchors, callers, strings, or constructor patterns—not proof of removal.

## Sample-label caveat

The specimens examined while designing this skill illustrated why identity matters. Samples labeled `25H2` and `26H1` retained several uDWM entry points while object fields moved; dwmcore lost some legacy-MIL paths while retaining other core classes. These observations are workflow examples only. Do not reuse their values without verifying PE/PDB identity and re-reading the active binary.
