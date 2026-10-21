# Retained regression tests

Follow [the repository testing policy](../AGENTS.md#testing-policy). Keep permanent tests focused on behavior that changes regularly, is used regularly, or has a demonstrated recurring regression. Use temporary checks in ignored `.archive/` for individual maintenance operations and migrations.

The Python suite covers:

- Projection generation and its contracts: selected declaration locations (including escaped JSON names), version boundaries, integer offsets, exact names, typed ABI/fallback rules, consumers (excluding comments and literals, including final comments without a newline), deterministic output, and explicit build inputs.
- Catalog identity, Required completeness, schema-compatible records, and the core incremental collection/resolver lifecycle.
- Platform range-group selection and validation, CPU-only descriptors and guarded consumers, empty generated declarations, Required activation and version gaps, common/specialized catalog selection, selected source branches and short-circuit conditions, whole-catalog replacement, selected-machine acquisition and CPU-specific CodeView receipts. These use synthetic fixtures and do not establish runtime support.
- Minimal PE/PDB pairing, acquisition identity, one shared audit-reason matrix covering GUID/DBI-age and version mismatches, unavailable/unsupported versions, missing/ambiguous Required Symbols, inactive Required success and combined failure ordering; thin entry-point success/failure checks preserving direct JSON/text reports and batch summaries; rejection of invalid optional headers and incomplete PE section tables, Layout selection/reporting, and deterministic PDB archive checks.
- Project filter completeness against repository imports, with no duplicate or external file nodes; imported shared files must be classified in consumer views too.

Test validation at its owning boundary. Shared version/binding behavior belongs with projection tests; do not repeat it through audit wrappers. Catalog key uniqueness needs one architecture-wide case, including independent module keys, rather than separate copies for each shard arrangement. Maintenance tests cover PDB-only admission, incremental preservation and resolver replacement. Acquisition tests use readable PDB fixtures to reject GUID/DBI-age mismatches before publication and retain the malformed-direct/compressed-success/cache-reuse regression. Internal report corruption, downstream format revalidation and speculative recovery paths do not need permanent fixtures. Range tests retain CodeView extraction/cache reuse and refusal of ignored or incorrect ranges. Explicit binary-audit tests retain actual PE RVA and header checks. Use temporary checks for individual download, publication and evidence-replay changes.

Run it from the repository root:

```powershell
python -m pip install -r requirements.txt
python -m unittest discover -s tests -p "test_*.py"
```

CI also runs the Symbol validator directly for both architectures. Avoid duplicating that full inventory scan in report-format tests. Test generic Symbol behavior with synthetic schemas rather than pinning individual production bindings or partition paths. When changing a command entry point or a rarely used acquisition path, exercise the affected command with a focused temporary check; a broad permanent mock suite is not required.

`OpenGlassTests/` retains C++ regression coverage for projection resolution/publication, hook lifetime, effective configuration, preview/recovery, package validation/resources, and representative color calculations. Catalog tests cover module/GUID/age matching, Optional absence, staging without early publication, and clearing a previously resolved Optional on the next successful commit; generated-table corruption belongs at the source-validation boundary. Python maintenance tests retain cross-build identity reuse. The PE/PDB pairing fixture uses distinct Info/DBI ages, a multi-digit hexadecimal key suffix, and mismatched GUID/DBI ages that must remain discovery-only. Registry tests use isolated test keys. Prefer explicit behavioral cases over copied production tables, repeated implementations of the expected algorithm, or exhaustive combinations that repeat an existing contract test.

See [the C++ test guide](../OpenGlassTests/README.md) for per-source test organization and `--list`, `--test Suite.Case`, and `--suite Suite` selection. Running `OpenGlassTests.exe` without arguments runs all cases, including in CI.

GUI layout, DPI, GPU rendering, and DWM integration need targeted temporary demos against the actual changed interface or runtime path on a suitable machine. Hidden copies of obsolete GUI layouts and a successful non-injecting test run do not establish those results. Record what was actually exercised and mark unavailable real-machine validation as not run.
