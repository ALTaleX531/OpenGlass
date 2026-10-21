# Built-in symbol catalog

The catalog contains exact PDB symbol-resolution results paired with a module's CodeView GUID/age. It is independent of the projection inventories in `../ProjectionSchemas/`. A record establishes exact name/RVA resolution, not private ABI semantics or real-OS support.

`<backend>/` is common to every CPU unless `<platform>/<backend>/` exists. A
specialized directory replaces the whole backend corpus, including for x64;
its missing index/shards are errors, not a request to mix common files. Collection,
verification and replay select the same directory as generation. The selected
corpus must match the selected projection groups. Key uniqueness and resolver
replacement apply within that corpus; runtime lookup remains module + PDB GUID +
DBI age. The maintained corpus stays in the common directory. See [CPU target selection](../../tools/README.md#cpu-target-selection).

## Source format

Unpublished catalog format 2 has one `index.json` per architecture. The index stores the architecture, the DbgHelp version and SHA-256 used for resolution, and an explicit list of `MODULE/BUILD.json` shards. Module builds come from each projection schema's `known_builds`.

Each record contains a numeric revision, PDB name/GUID/DBI age/SHA-256, and stable Symbol IDs with exact DbgHelp `UNDNAME_COMPLETE` names and RVAs. Architecture, module, build and resolver are inherited from the index/shard. There is no `image` object, PE hash, timestamp or size in a catalog record.

The sole lookup key is module + PDB GUID + DBI age, unique across all build shards within each architecture. PDB filenames locate downloads; PDB hashes support evidence replay. Neither is a second identity condition. Build/revision selects schema bindings separately from runtime identity matching. Runtime records contain only the key and a generated RVA span.

Every active Required Symbol must resolve uniquely. Missing Optional Symbols remain unresolved. The source loader validates bindings, complete names and nonzero 32-bit RVAs, and checks architecture-wide key uniqueness once. Emitters trust normalized records. There are no schema fingerprints, deltas or shared resolution sets. Codegen emits one RVA per logical Symbol slot, with zero for an absent Optional or inactive Symbol; constant-expression spans establish each record's bounds.

Runtime matches the loaded module's CodeView identity and stages base-plus-RVA addresses. An identity miss uses ordinary PDB resolution. Runtime does not repeat generated-table integrity checks or binary-audit checks. The common two-module Validate/Commit gate still owns Required validation and publication, including version activation and fallbacks.

## Collection and replay

Run from the repository root:

```powershell
python -m tools.maintain_symbol_catalog collect . --cache CACHE --dbghelp PATH_TO_DBGHELP
python -m tools.maintain_symbol_catalog verify .
python -m tools.maintain_symbol_catalog verify . --cache CACHE --dbghelp PATH_TO_DBGHELP
```

`collect` discovers Winbindex candidates for the selected CPU (default x64) and reads only DOS/NT/section headers, the debug directory and RSDS CodeView through HTTP Range, checking the selected PE machine. A server that ignores Range is reported as a failure; collection never falls back to a complete PE download. CodeView receipts are cached under `CACHE/codeview/` for x64 and `CACHE/codeview/<platform>/` for other targets. The current uDWM/dwmcore Winbindex indexes do not supply PDB identities themselves.

The CodeView PDB name/GUID/age selects the symbol-server URL. Acquisition validates the downloaded PDB's GUID from Info stream 1 and age from DBI stream 3 before publishing it to `CACHE/symbols/`, including compressed `.pd_` downloads. Symbol-server keys are derived when needed, with a hexadecimal age suffix. Collection checks exact names and Required completeness using the paired PDB alone; it does not require a complete PE or an image hash.

Existing schema-valid records survive omissions from the current feed. Candidates with an existing module/GUID/DBI-age key reuse that record across all build shards without downloading or resolving its PDB again. New records retain the candidate's indexed revision for binding selection. Collection consumes validated in-process results directly instead of rereading its own output or repeating acquisition validation. See [validation ownership](../../tools/README.md#validation-ownership).

`verify` checks committed format, shard coverage, keys, binding names, Required completeness and RVA representation. Supplying cache and DbgHelp additionally replays cached PDB identities, hashes and exact name/RVA results; it needs no PE and does not download missing PDBs. This does not establish independent image bounds, section type, ABI semantics or runtime support, and does not promise exhaustive Winbindex coverage.

Only successful records and their indexes are committed. Candidate feeds, CodeView receipts, PDB downloads and collection failures stay local. The failure report defaults to `CACHE/collection-failures.json`; `--missing-output PATH` overrides it. Failed candidates can be retried on a later invocation. Each architecture has one DbgHelp identity; changing it requires `collect --replace-resolver`, which clears its shards before recollection. Deleting an index while retaining records likewise requires restoring the index or explicit recollection.

## Explicit binary audit

Complete PE acquisition is opt-in:

```powershell
python -m tools.audit_winbindex_revisions . --architecture legacy --module udwm --build BUILD --min-revision REVISION --max-revision REVISION --binary-audit --cache CACHE --dbghelp PATH_TO_DBGHELP
python -m tools.audit_symbol_resolution . --architecture legacy --module udwm --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_PDB --configuration release
```

`--binary-audit` downloads/reuses the exact full PE, validates its Winbindex SHA-256, and checks actual file version, PE/PDB pairing, RVA bounds and Code/Data sections. `--image-root` is available only in that mode. Without `--binary-audit`, the revision tool follows Range + PDB resolution, like collection. A directly supplied `--image` is also an explicit full-binary audit. IDA semantic audits and real-OS validation remain separate layers; exact names and GUID/age pairing do not establish them.

Both entry points use the shared audit failure list: empty means success. Identity/version mismatches, unsupported versions and unresolved active Required Symbols return 1 while retaining their reports. See [binary-audit results](../../tools/README.md#binary-audit-results) for scope, report and exit-code semantics.
