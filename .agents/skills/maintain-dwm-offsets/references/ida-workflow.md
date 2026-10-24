# IDA MCP workflow

Use the connected ida-pro-mcp tool schemas as the API authority, as client-side prefixes may differ. The current API uses `idb_list` and `idb_open`, and requires an explicit `database` argument for analysis tools. Keep analysis read-only unless the user asks to annotate or change the IDB.

## Start and route

1. Call `idb_list({})`. Its `sessions` includes both adopted sessions and discovered GUI/worker instances. A discovered entry may have `session_id: ""` and `adopted: false`, so `is_active: true` does not make it a routable session.
2. Match the requested sample by full path and module. Reuse an existing non-empty session ID only after identity verification. To adopt a discovered GUI, call `idb_open` with its exact reported `input_path`, `mode: "prefer_gui"`, `run_auto_analysis: false`, `build_caches: false`, and `init_hexrays: false`. The reported path can be an `.i64` database. Do not substitute a similarly named DLL from another directory. Check `success`, `error`, and the returned `session.session_id` before continuing.
3. Set `database` to that returned session ID for every subsequent analysis call. It is an opaque routing token, not a file path, port, module name, or label. Do not invent IDs or call removed global-selection tools.
4. Call `server_health({database: session_id})` to verify the input/IDB path, module, image base, and readiness. Use `survey_binary({database: session_id, detail_level: "minimal"})` as the first binary-analysis query, especially for large DWM databases, to verify binary identity alongside actual PE and PDB checks when used.
5. Before each small query batch, confirm that its `database` still maps to the intended sample. Recheck health when switching samples, reconnecting, reopening, or observing a stale-session error. Treat an unexpected path, module, image base, or hash as a routing failure rather than binary evidence. An IDB path and its original PE path can differ. Reconcile them with survey metadata and exact PE identity.
6. Keep multi-sample audits serialized under the repository policy. Explicit session routing replaces the old global-selection protocol, but it does not authorize concurrent work on user databases. On a stale session, rediscover and verify the replacement before retrying. Never fall back to an empty `database` or the active GUI.

### Opening and cleanup

`idb_open` defaults to `prefer_headless`, which ignores a running GUI when choosing a new session. Use `prefer_gui` explicitly when the requested sample is already open in IDA. It can spawn a worker if no matching GUI remains, so inspect the resulting session/backend instead of assuming adoption succeeded. `force_gui` can launch a GUI, while `force_headless` only uses workers. Do not launch another IDA process just to probe connectivity.

Opening a new sample can create working database files and automatic analysis changes the database. For an audit of an existing IDB, disable the three open/warmup flags above and use available analysis. If the database is not ready, report that limitation instead of silently reanalyzing it. When the task needs initial analysis of a raw PE, use a disposable working copy outside the source tree and preserve the original sample and its identity. Worker idle TTL is a resource setting, not an identity guarantee.

Record which sessions predated the task and which workers it created. Leave pre-existing sessions open. To release a task-created session, use `idb_close({database: session_id, save: false})` explicitly: owned workers terminate, whereas adopted instances detach without being killed. Never call `idb_save` or close with `save: true` during an audit-only task. Unsaved operation is not permission to annotate or patch. Never delete loose IDA working files while the database is open.

Do not hard-code ports in repository files or reports. Ports are session routing details, not sample identity.

## Read-only capability map

All analysis calls below require the verified `database` argument. Read their current schemas before constructing filters, as similarly named tools do not necessarily accept the same query shape.

| Need | Tools and limits |
| --- | --- |
| Function/name discovery | `lookup_funcs` with `queries` (string or array), and `func_query` / `entity_query` for narrow filtered searches |
| Decisive function | `decompile` takes one `addr` string. Use `analyze_batch` with a `queries` array and selected sections for multiple functions |
| Machine instructions | `disasm` with `addr`, `offset`, `max_instructions`. Follow `cursor.next` until complete when all paths matter |
| Callers and references | `xref_query` with `direction: "to"`, `xref_type: "code"`, and pagination inside `queries`, and `xrefs_to`, `callees`, `callgraph`, `basic_blocks` for supporting paths |
| Data and anchors | `get_bytes`, `get_int`, `get_string`, `find_bytes`, `find`, `find_regex`. Signatures and `trace_data_flow` are candidate discovery, not semantic proof |
| Existing types | `type_query`, `type_inspect`, `read_struct`, `stack_frame`. Inferred/decompiler types still require ABI evidence |

Inspect per-item errors and truncation flags, not just transport success. `analyze_batch` summaries cap callers, xrefs, blocks, and instructions. Use paginated targeted queries to establish exhaustive coverage for a hook audit. A capped call graph cannot prove that every caller was inspected.

Avoid all IDB mutations during an audit-only request, including rename/comment/bookmark operations, `diff_before_after`, type application or inference, `make_data`, code/function definition, patches, undefinition, and save. Tool convenience or a before/after preview does not make an operation read-only.

### Python fallback

Prefer typed read tools. When a missing capability requires IDAPython, pass the same verified `database` to `py_eval` and inspect both `stderr` and the returned result. Locals persist across calls by default. Use `new_locals: true` for self-contained audit snippets, explicitly import dependencies, and avoid carrying addresses or sample identity from earlier queries. Resetting locals does not undo IDB changes. Globals are rebuilt on each call, so do not rely on persistent locals being visible inside nested function scopes.

For a larger reviewed read-only script, `py_exec_file` accepts an absolute `file_path` visible to the IDA host and executes with one shared globals namespace. Review the script for database/file mutations before execution. Neither Python tool is a sandbox or an exception to the audit-only boundary.

## Query sequence

1. Gate the class or capability with exact and wildcard symbol searches.
2. Use byte patterns, historical instructions, and register choices only to discover candidates. Compiler inlining and register allocation make them unsuitable as final proof.
3. Decompile the smallest decisive semantic function first.
4. Inspect disassembly when pseudocode hides units, adjusted `this`, bit fields, or an indirect call.
5. Follow xrefs to one independent confirmation path.
6. Compare the same semantic functions across samples, and never compare raw addresses.

## Audit optimized hook contracts

When the selected item is an inline hook, whole-program optimization may let callers rely on callee-preserved volatile registers beyond the public x64 ABI. Audit the exact binary across direct callers, call wrappers, and tail-jump thunks outward to semantic callers:

1. Track volatile GPRs (`RCX`, `RDX`, `R8`, `R9`, `R10`, `R11`), `XMM0` through `XMM5`, and EFLAGS from before the hooked call until they are overwritten or semantically consumed downstream.
2. Prove downstream feasible semantic use on real execution paths. Reject false positives such as value-independent idioms (`sbb reg,reg`), unconsumed scalar-move (`movss`/`movsd`) upper XMM lanes, arguments overwritten immediately in the next callee, or dead condition codes.
3. Verify that the hooked callee and any intervening wrappers actually preserve the register on that path in the exact binary. Unresolved indirect-call boundaries remain unverified, so never assume preservation.
4. Keep the verified calling dependency separate from the projected Symbol function-pointer ABI. The remedy is a version-specific custom physical dispatcher, not an altered logical ABI.

For example, in uDWM 26100.8972, `UpdateMarginsDependentOnStyle` invokes a three-argument `SetMargin` wrapper that calls the hooked six-argument helper, and the wrapper relies on the helper preserving `RCX`, which the outer caller reuses. While the six-argument prototype remains correct, a plain C++ dispatcher can clobber `RCX` because the public ABI does not guarantee preservation. The `CustomDispatchDetour` shim preserves `RCX` across Legacy builds because doing so is ABI-compatible, but this observed dependency is verified only for 26100.8972, not proof across other builds.

State whether callers depend on private register preservation across the hooked call and how an unmitigated dispatcher would clobber execution. If a batch request is rejected or too large, split it into smaller read-only requests. A missing exact symbol is a cue to search semantic anchors, callers, strings, or constructor patterns, not proof of removal.
