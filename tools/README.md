# Python tools and tests

All repository Python production modules live in the `tools` package. Tests live in `tests/`, including tests for the projection validators used by the DWM maintenance skill. `Scripts/` contains the Inno Setup and MSBuild packaging files, and `OpenGlassTests/` contains the independent C++ tests.

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

Install dependencies using the same Python 3.11+ interpreter used for tools and MSBuild's `ProjectionPython` (`OpenGlassPython` for packaging):

```powershell
python -m pip install -r requirements.txt
```

Virtual environments are supported. Activate the venv before installing packages or building, or configure `ProjectionPython` / `OpenGlassPython` to point to the virtual environment's Python executable. MSBuild consumes the prepared environment and does not install packages or access PyPI during builds. Both CI jobs prepare the environment via `.github/actions/prepare-python/` before invoking Python commands or MSBuild.

`requirements.txt` pins the shared dependencies:

- [Construct](https://construct.readthedocs.io/en/latest/intro.html): Parses the declarative MSF 7 directory and PDB identity fields without loading full symbol/type streams.
- [pefile](https://github.com/erocarrera/pefile): Reads PE headers, CodeView identities, sections, and version resources.
- [Pygments](https://pygments.org/docs/api/): Masks C++ comments and literals and locates JSON descriptor names.
- [pcpp](https://github.com/ned14/pcpp): Selects CPU conditional branches for lexical source inspection (not a full C++ compiler or semantic validator).

Follow the repository [dependency policy](../AGENTS.md#editing-conventions) when introducing dependencies. Build-time tools must have their dependencies installed before MSBuild executes.

## Test scope

Follow [the repository testing policy](../AGENTS.md#testing-policy) and [retained regression test scope](../tests/README.md). Validations requiring a real target environment cannot be replaced by synthetic unit tests. Report unavailable validation as not run.

## Command modules

| Command module | Purpose |
|---|---|
| `tools.projection_codegen` | Validate schemas and generate C++ into the selected intermediate directory |
| `tools.validate_projection_layouts` | Validate and query Layout cases and exact version selection |
| `tools.validate_projection_symbols` | Validate Symbol bindings, projected wrappers, and source consumers |
| `tools.audit_symbol_resolution` | Audit exact complete names against a PE/PDB pair |
| `tools.audit_winbindex_revisions` | Resolve indexed revisions through Range + PDB, and download a complete PE only with `--binary-audit` |
| `tools.dump_symbols` | Discover complete symbol names for an image |
| `tools.maintain_symbol_catalog` | Collect or verify the built-in symbol catalog |
| `tools.archive_pdbs` | Create the deterministic PDB archive |

For both projection validators, `--id` only filters reported descriptors. Layout validates the modules selected by `--module`, while Symbol always validates both modules and source consumers even if `--module` selects one. Successful validation exits 0. In `--format json` mode, schema validation failures and Symbol source validation failures are reported in `findings` and `summary.errors` on stdout with exit status 1, including when a schema failure prevents finding the requested ID. An unknown ID in otherwise valid input exits 2, with its error written to stdout for Layout and stderr for Symbol in JSON mode.

The remaining `tools` modules implement shared schema, source-checking, emission, and catalog logic. Import repository code explicitly with `from tools import ...` or `from tools.MODULE import ...`. Keep `tools/__init__.py` free of command execution. Do not add `sys.path` modifications, dynamic source loaders, or old-path compatibility launchers.

## CPU target selection

`--architecture legacy|milcomp` selects the DWM backend. Projection generation,
both validators, binary/revision audits and catalog `collect`/`verify` also accept
`--platform`, defaulting to `x64`. CPU names are case-insensitive, and MSBuild `Win32`
maps to `x86`. Recognizing a tool target does not add a supported runtime,
solution configuration or installer architecture.

Projection sources stay at `OpenGlass/ProjectionSchemas/<backend>/`. Each descriptor
owns platform groups under `cases` or `bindings`, each containing `ranges`.
`platform: null` or omission means common. The selected CPU group replaces the
whole common group, and version gaps do not mix the two. The loader validates the
selected ranges and ABI fields once. There is no same-name descriptor merge.
See [the source contract](../OpenGlass/ProjectionSchemas/README.md).

`OpenGlass/SymbolCatalogs/<backend>/` is the common catalog for every CPU. When
`SymbolCatalogs/<platform>/<backend>/` exists, that whole backend directory takes
precedence, including for x64. It must contain a complete index and its shards. Missing files do not fall back individually. Generation, collection, verification
and replay all use this choice. Collection writes to the chosen directory. Create a platform/backend directory before collection to maintain a separate corpus.
The selected corpus must match the selected projection bindings. Module/GUID/DBI-age
identity and corpus-wide key uniqueness remain unchanged. This repository keeps
its maintained corpus in the common directory, so no CPU-specific copy is required.

MSBuild forwards `$(Platform)` to codegen and tracks `projection_target.py` as a
generation input. Projects explicitly list their JSON sections and catalog shards.
Winbindex acquisition selects the target feed and PE machine. CodeView receipts,
index caches and default failure reports are partitioned by CPU, while PDB caches remain shared by filename/GUID/age. HTTP 206 remains required, and full images require
explicit binary auditing. CPU selection does not verify native facades, hooks,
assembly or compiler ABI behavior. The solution and runtime remain x64-only.

## Validation ownership

| Boundary | Responsibility |
|---|---|
| `projection_schema` | Own descriptor shape/name and platform-group selection, then validate selected ranges, ABI, identities and fallbacks, and provide source locations and shared version/binding selection |
| `projection_source_check` | Read, mask and CPU-select each source once for wrapper and real-consumer checks. Direct typed `Symbol_*` consumers need no wrapper; a wrapper declaration alone is insufficient |
| `pe_image` | Open PE files through pefile, and reject invalid optional headers and incomplete section tables at the input boundary |
| `symbol_catalog` | Share the persisted shard loader between generation and collection, validate persisted catalog format and current binding names/Required completeness, and check module/GUID/age uniqueness once across the architecture |
| `remote_codeview` / `audit_winbindex_revisions` | Require HTTP 206 and the requested byte range, with at most 1 MiB per metadata request. Check the selected PE machine and cache platform-specific CodeView receipts. Validate PDB GUID/DBI age before cache publication. Full PE acquisition is opt-in |
| `audit_symbol_resolution` | Resolve exact names/bindings and own shared audit success/failure reasons. Explicit image audit additionally checks actual version, PE/PDB pairing, RVA bounds/sections and discovery classification |
| `maintain_symbol_catalog` | Admit acquisition-validated PDBs with all active Required Symbols resolved and representable RVAs. Preserve the resolver and replay PDB identities/hashes/names/RVAs without a PE |
| `projection_emit` / catalog emission | Consume validated internal data and format C++ while preserving UTF-8 byte sequences and string-pool byte offsets, with no second format or descriptor-integrity validation |
| `file_io` | Stage output and publish only after writing/validation succeeds. Skip unchanged generated bytes, preserving build timestamps |

`tools.dbghelp` owns the shared native DbgHelp binding used by discovery and audit. It preserves exact `UNDNAME_COMPLETE` names and keeps symbol-server search/options in discovery while audits use their explicit PDB search path. The Construct description in `audit_symbol_resolution` reads GUID from Info stream 1 and age from DBI stream 3, preserving pairing even when the Info age differs. It reads only the directory and identity fields, without loading symbol/type streams or introducing a native PDB parser build dependency. Construct owns binary reads, pointer positioning and truncation errors, while the description owns the MSF layout. Unsupported or unreadable PDBs still become `AuditError`. `tools.file_io.atomic_output` owns staged replacement and cleanup, including validation before publishing downloaded binaries and ZIP output. Format-specific ZIP/gzip handling, HTTP fetching and SHA-256 hashing use the standard library.

`tools.pe_image` adapts pefile at the binary-input boundary: require a PE32/PE32+ optional header containing SizeOfImage and a complete declared section table. pefile can otherwise return guessed headers or partial sections with warnings, which must not pass binary-input validation. Callers retain module, hash, GUID/age, version and RVA/address-class matching. The Pygments adapter supplies a terminating newline so final `//` comments are masked too. pcpp evaluates compiler and Windows SDK CPU predicates and constant short-circuit results, preserving character constants during evaluation, and remaining literals are masked before consumer checks. Unresolved feature/configuration conditions retain their branches. Includes are not opened and ordinary macro bodies remain visible without expansion.

Collection trusts the shard loader's validated `records` list, validates retained records against the current schema once, and uses the identity set returned by the architecture-wide uniqueness check for new candidates. This includes records in later build shards, so another image with the same module/GUID/age keeps the existing record. Those in-process reports are not an external JSON import format: do not add report-shape checks or rehash the files downstream. Batch audits reuse validated schemas and one resolver identity. Publish the assembled shards without reloading the corpus merely to repeat validation. Standalone `verify` still validates persisted sources, and evidence replay compares newly audited records with those sources.

The download helper makes one attempt per URL. Missing or failed candidates remain in the local failure report and can be retried on a later invocation. The direct `.pdb` and compressed `.pd_` paths are distinct symbol-server formats. Both verify GUID/DBI age before cache publication. A failed direct download, malformed PDB or identity mismatch proceeds to the compressed format, and failures from both formats become one acquisition error. Matching cached PDBs are reused, while malformed or mismatched cached files are reacquired. A successful acquisition is returned directly instead of searched again.

## Binary-audit results

`inspect` owns the identity/version evidence reasons. Both command entry points consume `audit_symbol_resolution.audit_failures`: it combines Required resolution failures, image identity/version reasons and an unsupported-version reason, in that order. An empty list is the sole success decision. The PDB-only revision path uses the same helper with `binary_audit=False`, omitting image evidence. Commands only format reports and map this result to exit status.

Both binary-audit commands return 0 only for matching PDB GUID/DBI age and actual/requested versions, a supported version, and uniquely resolved active Required Symbols in the selected audit scope. Identity/version mismatches (including an unavailable file version), unsupported versions and missing/ambiguous Required resolution return 1. Inactive Required Symbols and absent Optional Symbols do not fail. `--id` restricts Symbol resolution to that ID, but it does not establish whole-module Required completeness. Other input/processing errors also return nonzero.

The direct command retains its full JSON/text report on audit failures, including discovery evidence and both expected CodeView and actual PDB identities. The revision command prints per-revision summaries using the shared reasons, and it does not expose the direct command's `--format` option. Exit code zero establishes binary resolution evidence, not ABI semantics or real-OS support.

PDB download and replay share `PdbIdentity.key()`. Its age suffix is hexadecimal, matching the runtime URL builder and [the symbol-store key convention](https://github.com/dotnet/symstore/blob/main/docs/specs/SSQP_Key_Conventions.md#pdb-signature-age), while JSON age remains a number. Do not independently concatenate a decimal age into cache paths.

MSBuild runs command modules with the repository root as its working directory and lists generation inputs explicitly. CI runs the same unified Python test command. Generated C++ remains a build artifact. Moving or editing tools does not change a schema version by itself.
