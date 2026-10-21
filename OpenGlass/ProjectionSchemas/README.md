# Projection source organization

`legacy` and `milcomp` identify DWM backends, independently of the CPU target.
Generation selects one CPU with `--platform` (default x64) from these same
backend directories. The solution and runtime remain x64-only.

Each architecture owns independent schema-version-5 module manifests, `udwm.json` and `dwmcore.json`. A manifest contains module metadata (namespace, tag, recognized builds, supported version range, and module notes) and explicit `symbols` and `layouts` file lists. Paths are relative to the manifest. There is no recursive inclusion, directory discovery, or inline descriptor format.

Each descriptor occurs once. Layout `cases` and Symbol `bindings` contain platform
groups with a `ranges` array. `platform: null` or an omitted `platform` denotes the
common group. The selected CPU's group replaces the entire common group, regardless
of group order. If neither exists, the descriptor is omitted from that CPU's inventory.
Only the selected group's ranges and ABI fields are validated. Version gaps within
a specialization do not fall back to common ranges.

```json
{
  "name": "Widget_Field", "id": "Widget.Field", "kind": "field", "type": "int",
  "cases": [
    {"platform": null, "ranges": [{"offset": 16, "until": {"build": 26100}}, {"offset": 24, "otherwise": true}]},
    {"platform": "x86", "ranges": [{"offset": 8, "otherwise": true}]}
  ]
}
```

Symbol groups own `target`, `type`, `fallback` and `abi_compatibility` when applicable;
their ranges own exact `symbol_names` and version intervals. Layouts keep their
shared `type` on the descriptor; a group may override it. Names, IDs, kinds and other
descriptor metadata are not merged between entries. Duplicate handles or duplicate
common/selected CPU groups are errors. Each selected group needs non-empty ranges.

The loader normalizes the selected group before assigning indices/slots or checking
source consumers. Diagnostic locations refer to the descriptor's `name` value.
Empty inventories produce empty tables. Catalog selection is described in
[CPU target selection](../../tools/README.md#cpu-target-selection).

For a CPU-only Symbol or Layout, omit the common group. Guard its C++ handle uses,
facade members/wrappers and callers with matching compiler CPU conditions. An
omitted descriptor creates no slot or Required prerequisite. Source inspection
checks selected consumers and wrappers; the C++ compiler owns unresolved raw
Symbol and Layout references. Compiler CPU macro profiles can overlap; predicates
must match the intended group rather than assuming every macro is exclusive.

```text
<architecture>/
  udwm.json
  udwm/
    symbols/
      visuals.json
      window-frame.json
      ...
    layouts/
      CVisual.json
      CTopLevelWindow.json
      CWindowData.json
      ...
  dwmcore.json
  dwmcore/
    symbols/
      drawing-context.json
      occlusion.json
      ...
    layouts/
      CDrawingContext.json
      COcclusionContext.json
      ...
```

Symbol sections contain only a `symbols` array and group entries by resource or usage category, such as resources, geometry, drawing contexts, or occlusion. Layout sections contain only a `layouts` array and primarily group members by their owning class, with files such as `CVisual.json` and `CTopLevelWindow.json`. These partition strategies are independent: a Symbol category may span several classes, and a class's Layouts need no matching Symbol file. Keep each descriptor, including all of its version cases or bindings and evidence notes, together. Keep every platform group of a descriptor together; do not split by Windows version or duplicate its handle.

Every Layout case's `offset` is a signed 32-bit JSON integer measured in bytes, including vtable byte offsets and negative adjusted-`this` displacements. Use decimal numbers, never strings, `sizeof`, `offsetof`, or C++ arithmetic. The values describe the selected module's binary layout independently of compiler-side types. Validation enforces the integer representation and range once; codegen emits the numbers directly without expression evaluation or redundant generated range assertions.

Layout `until` boundaries use the same numeric version objects as Symbol bindings: `build` is a nonzero unsigned 32-bit JSON integer; `revision` is an unsigned 32-bit JSON integer and defaults to zero when omitted. For example, `"until": {"build": 26100, "revision": 4484}` is an exclusive right boundary. Numeric strings and C++ constant names such as `os::build_w11_24h2` are not supported. Schema loading does not parse `OSHelper.hpp`; Layouts and Symbols share version validation and one deduplicated numeric version pool in generated metadata. `otherwise: true` remains the explicit open-ended Layout case.

The loader combines the listed sections and validates each module as one inventory. Handle names remain unique across sections; stable-ID and target ranges are checked across the whole module, preserving disjoint Symbol variants that share a diagnostic ID. All existing ABI, interval, fallback, and consumer rules still apply. Descriptor order is deterministic by stable ID, binding range, and handle name, independent of section order. Source partitioning adds no runtime metadata, registry, locking, or dispatch layer. Layout and Symbol audit reports identify the editable section and declaration line.

Descriptor integrity is established before runtime. Schema validation owns non-overlapping binding intervals, ordered Layout cases and consistent fallbacks; codegen constructs table indices and slices from the validated inventory. `ModuleRegistry` consumes these internal generated tables directly. `Freeze` checks the actual module version, selects Layout cases and resets resolution; it does not revalidate generated indices or rescan binding intervals for overlaps.

The Python tools share `projection_schema.Version`, range checks and binding selection. Schema loading validates version representations once; emitters, catalog matching and query reports select from those validated ranges directly. Reporting does not repeat Layout uniqueness checks or impose uniqueness across independent modules. CLI argument parsing owns the architecture choice; internal emitters consume the loader's fixed module inventory. [Validation ownership](../../tools/README.md#validation-ownership) describes the full tool pipeline.

Codegen groups projected Symbols by kind and normalized target, emitting one `BindingSpec` per physical slot with its fallback and a span of candidate logical Symbol indices. ABI variants share a group; multiple bindings of one logical Symbol contribute only one candidate index. `PublishBindings` visits these groups and their candidates once, publishing the first committed resolved address or the group's fallback. Reset restores each slot's fallback. There is no runtime slot-identity search or grouping pass. Required/Optional status, Debug-only activation, version gaps, exact-name ambiguity and cross-module Validate/Commit still depend on actual resolution and retain their runtime behavior.

The built-in symbol catalog supplies a prevalidated dense RVA span for the loaded module's PDB identity. Its collection path stages candidates directly, without descriptor searches or a separate Required validation pass. Catalog collection leaves published slots untouched; PDB parsing begins with an explicit Reset to fallbacks and then collects candidates. Neither path publishes collected addresses until the common two-module commit succeeds. Missing-symbol reports are constructed only after that gate fails. See [the catalog contract](../SymbolCatalogs/README.md) for the generation and binary-evidence boundaries.

A successful commit replaces the complete resolved set, including clearing Optional Symbols that are now missing or ambiguous. Collection alone keeps the previous published set; a failed two-module validation publishes neither module's staged candidates. Reset remains the explicit operation that clears resolution and restores slot fallbacks before a new initialization or PDB parse.

When adding or moving a section, update its manifest and the architecture project's explicit `None` items and `.vcxproj.filters` entries. The generation target uses the project's JSON `None` items as inputs, covering both projection sections and symbol catalog shards. Keep each file listed separately so Visual Studio can edit the project; do not use item-list expansion or source wildcards in project items. Generated C++ stays under the selected project's `$(IntDir)\Generated\Projection`; it is never an editable inventory.

See [Legacy](legacy/README.md) and [MILComp](milcomp/README.md) for architecture-specific ranges, consumers, and verification commands. Keep values and semantic evidence independent between architectures.
