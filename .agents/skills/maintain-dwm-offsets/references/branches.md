# Branch architecture

Detect the current branch from Git when possible, then verify the source shape rather than trusting the branch name alone.

## legacy

The legacy line hooks the MIL compositor draw stream and carries multi-generation `OffsetInfo` arrays in `OpenGlass/dwmcoreProjection.Offsets.hpp` and `OpenGlass/uDwmProjection.Offsets.hpp`.

Audit the tables actually consumed by `dwmcoreProjection.hpp` and `uDwmProjection.hpp`. Preserve historical intervals. A new sample normally adds or adjusts a right boundary; it does not justify collapsing earlier evidence.

## milcomp

The milcomp line works at the Windows.UI.Composition visual layer for the build 28000+ family. It has a smaller/different projection surface even when files retain the same names.

Enumerate consumers before auditing. Do not copy the legacy checklist wholesale, and do not reintroduce legacy MIL brush projections merely because they exist on another branch.

## Detection hints

The bundled linter reports a source-shape classification. Treat it as routing assistance:

- legacy markers include historical multi-generation tables and legacy MIL projection names;
- milcomp markers include its reduced visual/compositor projection set;
- `unknown` requires manual inspection of projection consumers.

When supporting both branches in one change, validate each checkout or archive independently. Never switch branches over a dirty working tree.
