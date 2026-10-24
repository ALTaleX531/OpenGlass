# Retained regression tests

Follow [the repository testing policy](../AGENTS.md#testing-policy). Keep permanent tests focused on behavior that both changes regularly and is used regularly, or has a demonstrated recurring regression. Use temporary checks in ignored `.archive/` for individual maintenance operations and migrations.

Run the Python test suite from the repository root:

```powershell
python -m pip install -r requirements.txt
python -m unittest discover -s tests -p "test_*.py"
```

The Python suite covers:

- Projection generation and its contracts: selected declaration locations (including escaped JSON names), version boundaries, integer offsets, exact names, typed ABI/fallback rules, consumers (excluding comments and literals, including final comments without a newline), deterministic output, and explicit build inputs.
- Catalog identity, Required completeness, schema-compatible records, and the core incremental collection/resolver lifecycle.
- Platform range-group selection and validation, CPU-only descriptors and guarded consumers, empty generated declarations, Required activation and version gaps, common/specialized catalog selection, selected source branches and short-circuit conditions, whole-catalog replacement, selected-machine acquisition and CPU-specific CodeView receipts. These use synthetic fixtures and do not establish runtime support.
- Minimal PE/PDB pairing, acquisition identity, one shared audit-reason matrix covering GUID/DBI-age and version mismatches, unavailable/unsupported versions, missing/ambiguous Required Symbols, inactive Required success and combined failure ordering, thin entry-point success/failure checks preserving direct JSON/text reports and batch summaries, and rejection of invalid optional headers and incomplete PE section tables, Layout selection/reporting, and deterministic PDB archive checks.
- Project filter completeness is verified against repository imports, with no duplicate or external file nodes. Imported shared files must be classified in consumer views too.

Test validation at its owning boundary. Shared version/binding behavior belongs with projection tests, so do not repeat it through audit wrappers. Catalog key uniqueness needs one architecture-wide case, including independent module keys, rather than separate copies for each shard arrangement. Maintenance tests cover PDB-only admission, incremental preservation, cross-build identity reuse, and resolver replacement. Acquisition tests use readable PDB fixtures to reject GUID/DBI-age mismatches before publication and retain the malformed-direct/compressed-success/cache-reuse regression. PE/PDB pairing fixtures use distinct Info/DBI ages, multi-digit hexadecimal key suffixes, and mismatched GUID/DBI ages that remain discovery-only. Internal report corruption, downstream format revalidation and speculative recovery paths do not need permanent fixtures. Range tests retain CodeView extraction/cache reuse and refusal of ignored or incorrect ranges. Explicit binary-audit tests retain actual PE RVA and header checks. Use temporary checks for individual download, publication and evidence-replay changes.

CI also runs the Symbol validator directly for both architectures. Avoid duplicating that full inventory scan in report-format tests. Test generic Symbol behavior with synthetic schemas rather than pinning individual production bindings or partition paths. When changing a command entry point or a rarely used acquisition path, exercise the affected command with a focused temporary check, as a broad permanent mock suite is not required.

For C++ testing, see [the C++ test guide](../OpenGlassTests/README.md).

GUI layout, DPI, GPU rendering, and DWM integration need targeted temporary demos against the actual changed interface or runtime path on a suitable machine. Hidden copies of obsolete GUI layouts and a successful non-injecting test run do not establish those results. Record what was actually exercised and mark unavailable real-machine validation as not run.
