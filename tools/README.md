# Python tools and tests

All repository Python production modules live in the `tools` package. Tests live in `tests/`, including tests for the projection validators used by the DWM maintenance skill. `Scripts/` contains the Inno Setup and MSBuild packaging files; `OpenGlassTests/` contains the independent C++ tests.

Run Python commands from the repository root using module execution:

```powershell
python -m pip install -r requirements.txt
python -m unittest discover -s tests -p "test_*.py"
python -m tools.projection_codegen --repo . --architecture legacy --output Cache\OpenGlass.Legacy\x64\Release\Generated\Projection
python -m tools.validate_projection_layouts . --architecture legacy
python -m tools.validate_projection_symbols . --architecture legacy
python -m tools.maintain_symbol_catalog verify .
```

Use `--help` on a command module for its arguments. Commands that inspect or download Windows symbols retain their Windows/DbgHelp requirements. The Python unit tests use local fixtures and do not download binaries or inject into DWM.

## Dependencies

Use mature third-party libraries when they substantially simplify a tool or replace lengthy custom implementations of common functionality. Use the standard library when it already offers a concise solution. Declare introduced dependencies in a repository dependency manifest and update the local installation instructions and CI installation steps together, before commands that need them run. Build-time tools must have their dependencies available before MSBuild invokes them.

`requirements.txt` pins the shared Python environment: [Construct](https://construct.readthedocs.io/en/latest/intro.html) parses the declarative MSF 7 directory and PDB identity fields; [pefile](https://github.com/erocarrera/pefile) reads PE headers, CodeView identities, sections and version resources; [Pygments](https://pygments.org/docs/api/) masks C++ comments/literals and locates JSON descriptor names; [pcpp](https://github.com/ned14/pcpp) selects CPU conditional branches for lexical source inspection. These checks are not a C++ compiler or semantic validator. Install these dependencies with the same Python 3.11+ interpreter used for tools and MSBuild's `ProjectionPython` (`OpenGlassPython` for packaging). A virtual environment is supported; activate it before installation/building or set these executable properties explicitly. MSBuild consumes the prepared environment and does not install packages or access PyPI during builds. Both CI jobs install the same requirements through `.github/actions/prepare-python/` before running Python commands or MSBuild.

`tools.dbghelp` owns the shared native DbgHelp binding used by discovery and audit. It preserves exact `UNDNAME_COMPLETE` names and keeps symbol-server search/options in discovery while audits use their explicit PDB search path. The Construct description in `audit_symbol_resolution` reads GUID from Info stream 1 and age from DBI stream 3, preserving pairing even when the Info age differs. It reads only the directory and identity fields, without loading symbol/type streams or introducing a native PDB parser build dependency. Construct owns binary reads, pointer positioning and truncation errors; the description owns the MSF layout. Unsupported or unreadable PDBs still become `AuditError`. `tools.file_io.atomic_output` owns staged replacement and cleanup, including validation before publishing downloaded binaries and ZIP output. Format-specific ZIP/gzip handling, HTTP fetching and SHA-256 hashing use the standard library.

`tools.pe_image` adapts pefile at the binary-input boundary: require a PE32/PE32+ optional header containing SizeOfImage and a complete declared section table. pefile can otherwise return guessed headers or partial sections with warnings, which must not pass binary-input validation. Callers retain module, hash, GUID/age, version and RVA/address-class matching. The Pygments adapter supplies a terminating newline so final `//` comments are masked too. pcpp evaluates compiler and Windows SDK CPU predicates and constant short-circuit results, preserving character constants during evaluation; remaining literals are masked before consumer checks. Unresolved feature/configuration conditions retain their branches. Includes are not opened and ordinary macro bodies remain visible without expansion.

## Test scope

Follow [the repository testing policy](../AGENTS.md#testing-policy): maintain lasting regression coverage primarily for frequently changed, frequently used behavior. Rarely changed, rarely used tools need only minimal useful tests; focused temporary tests can validate an individual change. Keep temporary tests and real-machine demos in ignored `.archive/`, and retain them in `tests/` only when they provide recurring value. Changes that require a real target environment must be exercised there with a minimal demo; report unavailable validation as not run.

The discovery command runs the retained Python suite. It does not imply that every helper, script, or one-off maintenance operation needs a permanent test, and it does not replace real-machine validation.

See [the retained regression coverage](../tests/README.md) for the Python and C++ suite boundaries.

## Command modules

| Command module | Purpose |
|---|---|
| `tools.projection_codegen` | Validate schemas and generate C++ into the selected intermediate directory |
| `tools.validate_projection_layouts` | Validate and query Layout cases and exact version selection |
| `tools.validate_projection_symbols` | Validate Symbol bindings, projected wrappers, and source consumers |
| `tools.audit_symbol_resolution` | Audit exact complete names against a PE/PDB pair |
| `tools.audit_winbindex_revisions` | Resolve indexed revisions through Range + PDB; download a complete PE only with `--binary-audit` |
| `tools.dump_symbols` | Discover complete symbol names for an image |
| `tools.maintain_symbol_catalog` | Collect or verify the built-in symbol catalog |
| `tools.archive_pdbs` | Create the deterministic PDB archive |

The remaining `tools` modules implement shared schema, source-checking, emission, and catalog logic. Import repository code explicitly with `from tools import ...` or `from tools.MODULE import ...`. Keep `tools/__init__.py` free of command execution. Do not add `sys.path` modifications, dynamic source loaders, or old-path compatibility launchers.

## CPU target selection

`--architecture legacy|milcomp` selects the DWM backend. Projection generation,
both validators, binary/revision audits and catalog `collect`/`verify` also accept
`--platform`, defaulting to `x64`. CPU names are case-insensitive; MSBuild `Win32`
maps to `x86`. Recognizing a tool target does not add a supported runtime,
solution configuration or installer architecture.

Projection sources stay at `OpenGlass/ProjectionSchemas/<backend>/`. Each descriptor
owns platform groups under `cases` or `bindings`, each containing `ranges`.
`platform: null` or omission means common. The selected CPU group replaces the
whole common group; version gaps do not mix the two. The loader validates the
selected ranges and ABI fields once. There is no same-name descriptor merge.
See [the source contract](../OpenGlass/ProjectionSchemas/README.md).

`OpenGlass/SymbolCatalogs/<backend>/` is the common catalog for every CPU. When
`SymbolCatalogs/<platform>/<backend>/` exists, that whole backend directory takes
precedence, including for x64. It must contain a complete index and its shards;
missing files do not fall back individually. Generation, collection, verification
and replay all use this choice. Collection writes to the chosen directory; create
a platform/backend directory before collection to maintain a separate corpus.
The selected corpus must match the selected projection bindings. Module/GUID/DBI-age
identity and corpus-wide key uniqueness remain unchanged. This repository keeps
its maintained corpus in the common directory; no CPU-specific copy is required.

MSBuild forwards `$(Platform)` to codegen and tracks `projection_target.py` as a
generation input. Projects explicitly list their JSON sections and catalog shards.
Winbindex acquisition selects the target feed and PE machine. CodeView receipts,
index caches and default failure reports are partitioned by CPU; PDB caches remain
shared by filename/GUID/age. HTTP 206 remains required, and full images require
explicit binary auditing. CPU selection does not verify native facades, hooks,
assembly or compiler ABI behavior. The solution and runtime remain x64-only.

## Validation ownership

| Boundary | Responsibility |
|---|---|
| `projection_schema` | Own descriptor shape/name and platform-group selection, then validate selected ranges, ABI, identities and fallbacks; provide source locations and shared version/binding selection |
| `projection_source_check` | Check wrapper and real consumer contracts against C++ source |
| `pe_image` | Open PE files through pefile; reject invalid optional headers and incomplete section tables at the input boundary |
| `symbol_catalog` | Validate persisted catalog format and current binding names/Required completeness; check module/GUID/age uniqueness once across the architecture |
| `remote_codeview` / `audit_winbindex_revisions` | Require HTTP 206 and the requested byte range; check the selected PE machine and cache platform-specific CodeView receipts; validate PDB GUID/DBI age before cache publication; full PE acquisition is opt-in |
| `audit_symbol_resolution` | Resolve exact names/bindings and own shared audit success/failure reasons; explicit image audit additionally checks actual version, PE/PDB pairing, RVA bounds/sections and discovery classification |
| `maintain_symbol_catalog` | Admit acquisition-validated PDBs with all active Required Symbols resolved and representable RVAs; preserve the resolver and replay PDB identities/hashes/names/RVAs without a PE |
| `projection_emit` / catalog emission | Consume validated internal data and format C++; no second format or descriptor-integrity validation |
| `file_io` | Stage output and publish only after writing/validation succeeds; skip unchanged generated bytes, preserving build timestamps |

Collection validates retained records against the current schema once and checks their keys across all architecture shards before processing candidates. New audit results reuse that architecture-wide identity set, including records in later build shards, so another image with the same module/GUID/age keeps the existing record. Those in-process reports are not an external JSON import format: do not add report-shape checks or rehash the files downstream. Batch audits reuse validated schemas and one resolver identity. Publish the assembled shards without reloading the corpus merely to repeat validation. Standalone `verify` still validates persisted sources, and evidence replay compares newly audited records with those sources.

The download helper makes one attempt per URL. Missing or failed candidates remain in the local failure report and can be retried on a later invocation. The direct `.pdb` and compressed `.pd_` paths are distinct symbol-server formats; both verify GUID/DBI age before cache publication. A failed direct download, malformed PDB or identity mismatch proceeds to the compressed format; failures from both formats become one acquisition error. Matching cached PDBs are reused; malformed or mismatched cached files are reacquired. A successful acquisition is returned directly instead of searched again.

Catalog records contain no image hash or complete-PE dependency. `collect` caches only CodeView receipts and PDBs; `verify --cache --dbghelp` replays PDBs already present locally. Full PE hashes, versions and sections are checked only by an explicit `--binary-audit` revision run or direct `audit_symbol_resolution --image`. Range support is required rather than silently downloading a full image.

## Binary-audit results

`inspect` owns the identity/version evidence reasons. Both command entry points consume `audit_symbol_resolution.audit_failures`: it combines Required resolution failures, image identity/version reasons and an unsupported-version reason, in that order. An empty list is the sole success decision. The PDB-only revision path uses the same helper with `binary_audit=False`, omitting image evidence. Commands only format reports and map this result to exit status.

Both binary-audit commands return 0 only for matching PDB GUID/DBI age and actual/requested versions, a supported version, and uniquely resolved active Required Symbols in the selected audit scope. Identity/version mismatches (including an unavailable file version), unsupported versions and missing/ambiguous Required resolution return 1. Inactive Required Symbols and absent Optional Symbols do not fail. `--id` restricts Symbol resolution to that ID; it does not establish whole-module Required completeness. Other input/processing errors also return nonzero.

The direct command retains its full JSON/text report on audit failures, including discovery evidence and both expected CodeView and actual PDB identities. The revision command prints per-revision summaries using the shared reasons; it does not expose the direct command's `--format` option. Exit code zero establishes binary resolution evidence, not ABI semantics or real-OS support.

PDB download and replay share `PdbIdentity.key()`. Its age suffix is hexadecimal, matching the runtime URL builder and [the symbol-store key convention](https://github.com/dotnet/symstore/blob/main/docs/specs/SSQP_Key_Conventions.md#pdb-signature-age); JSON age remains a number. Do not independently concatenate a decimal age into cache paths.

MSBuild runs command modules with the repository root as its working directory and lists generation inputs explicitly. CI runs the same unified Python test command. Generated C++ remains a build artifact; moving or editing tools does not change a schema version by itself.
