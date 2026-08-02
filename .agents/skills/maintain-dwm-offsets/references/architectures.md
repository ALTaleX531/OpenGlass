# DWM architectures

The unified tree builds two explicit DLL projects. Require the requested architecture before selecting schemas or consumers; do not infer it from the current Git branch.

## legacy

The Legacy DLL hooks the MIL compositor draw stream and carries multi-generation Layout and Symbol schemas in `OpenGlass/ProjectionSchemas/legacy/`. Its private consumers live in `OpenGlass/Architecture/Legacy/`. Generated declarations appear only under that project's `$(IntDir)\Generated\Projection`; the `*.Offsets.hpp` files are compatibility include points, not inventories.

Audit the Layout and Symbol schema IDs actually consumed by `dwmcoreProjection.hpp`, `uDwmProjection.hpp`, and component Detours. Preserve historical intervals. A new sample normally adds or adjusts an exclusive `until` right boundary; it does not justify collapsing earlier evidence.

## milcomp

The MILComp DLL works at the Windows.UI.Composition visual layer for the build 28000+ family. Its schemas live in `OpenGlass/ProjectionSchemas/milcomp/` and its private consumers in `OpenGlass/Architecture/MILComp/`. It has a smaller/different projection surface even when files retain the same names.

Enumerate consumers and inspect the MILComp schema before auditing. Do not copy the legacy checklist or schema wholesale, and do not reintroduce legacy MIL brush projections merely because they exist in the same source tree.

## Selection rules

Pass `--architecture legacy` or `--architecture milcomp` to both schema validators and to projection code generation. The JSON report repeats the selected architecture. The build projects are `OpenGlass.Legacy.vcxproj` and `OpenGlass.MILComp.vcxproj`; both retain the standard `Debug`, `Release`, and `ReleaseSigned` configurations.

- legacy markers include historical multi-generation Layout descriptors and legacy MIL projection names;
- milcomp markers include its reduced visual/compositor projection set;
- an unfamiliar schema or detached checkout requires manual inspection of the selected architecture's consumers.

When changing shared projection mechanisms, validate both architectures independently. Never copy Layout values, Symbol ranges, complete symbol names, private types, or semantic conclusions between them merely because stable IDs or short PDB names match.
