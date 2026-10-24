# Effective presets and schema 3

The behavior contract is [Configuration.design.md](Configuration.design.md). This document specifies its package format and storage details.

Schema 3 consolidates this release's accent color, effective configuration, default-state and rights changes. Only published schemas 1 and 2 are legacy inputs. Unpublished intermediate formats are not supported.

A preset describes effective behavior, independent of the editor hive. Ordinary controls edit only the selected layer. Layer display, inheritance notices, and default semantics follow [Configuration.design.md#2-layer-display-inheritance-and-defaults](Configuration.design.md#2-layer-display-inheritance-and-defaults). Internal Override variants are not presented as separate setting controls or tooltip details. Accent RGB remains an independent original-user Windows state operation. The shared `EffectiveConfiguration` model captures runtime precedence without materializing consumer defaults or replacing automatic/theme sentinels with temporary values.

## Archive format

A package is a standard ZIP archive with a root `manifest.json`, an optional `LICENSE`, and an optional `assets/` directory when resources are present. It uses no custom file extension. The recommended filename is `<sanitized-name>-<uuid>.zip`.

```text
manifest.json
[LICENSE]
assets/
  theme-atlas.png
  theme-atlas.png.layout
  reflection.png
  material.png
```

New packages use schema 3, with no scope field and no `system_color` alias. Published schemas 1 and 2 remain readable. The top-level shape is:

```json
{
  "schema_version": 3,
  "catalog_version": 3,
  "uuid": "00112233-4455-6677-8899-aabbccddeeff",
  "name": "Preset name",
  "description": "Optional preset description",
  "author": { "name": "Author", "homepage": "" },
  "license": null,
  "rights": {
    "configuration": "openglass-attribution-v1",
    "license_scope": "image-assets",
    "sources": []
  },
  "accent_color": { "rgb": "#74B8FC" },
  "settings": {},
  "assets": {}
}
```

The empty objects above illustrate structure, not a complete valid package. For complete value types and default-state specifications, see [Configuration states](#configuration-states). The `assets` map pairs each referenced file path under `assets/` with its integer byte `size` and lowercase SHA-256 hash (`sha256`).

The manifest requires a lowercase canonical UUID, a non-empty name, and a non-empty author. The description may be empty, and the author homepage may be empty or an absolute HTTP(S) URL. Identical content with the same UUID is reused, while different content must receive a new UUID.

## Configuration states

Every setting covered by the package catalog version is present. DWORDs remain unsigned 32-bit JSON numbers, assets are `{"asset":"assets/reflection.png"}`, and uncustomized states are `{"state":"default"}`. Missing covered fields are invalid. Entries outside the recognized catalog surface or its version coverage are ignored for writes, retained in the content digest, and reported in reviews. A newer nonzero catalog version alone does not cause rejection. An older catalog never removes settings introduced later.

Transparency keeps the existing `ColorizationOpaqueBlend` field, and `EnableTransparency` is never a package setting. By default, capture inverts the original user's Windows `EnableTransparency` DWORD (zero becomes 1, nonzero becomes 0). Missing/invalid values capture as 1, matching their disabled runtime effect. Application inversely maps the package value to that same Windows setting, always for the original user: zero or Default explicitly writes `EnableTransparency=1`, and nonzero writes 0. The Windows value is never deleted. The two DWM `ColorizationOpaqueBlend` values are preserved. `--no-system-transparency` restores effective DWM capture and ordinary layer-specific application of `ColorizationOpaqueBlend`. The same mapping applies to accepted legacy packages without modifying their saved content or digest. Raw checkpoints record the actual destination, including type and absence. They are not preset-format values. Recovery restores existing raw values exactly and restores absence as DWORD 0 without deletion.

`accent_color` is `null` (enable Windows Automatic mode) or `{"rgb":"#74B8FC"}` (disable automatic coloring and apply this RGB). A missing `accent_color` field has the same automatic meaning as `null`. Present RGB in a preset means manual mode upon application, rather than an optional RGB write. Absent or `null` means automatic mode, not leaving the accent unchanged.

The ten [OpenGlassGUI managed settings](Configuration.design.md#6-openglassgui-treats-color-configuration-specially) are governed as a special policy group. The five Windows base color values are not schema 3 configuration entries. New capture reads only the three balance Overrides across both hives using HKCU/HKLM precedence. Uncustomized Overrides are never filled from Windows base balances. Schema 3 ignores base balance fields in package contents, reporting them in review while retaining them in the original digest. GUI balance writes target Overrides and never directly write or delete base balances. `ColorizationColorOverride` and `ColorizationAfterglowOverride` are excluded from new capture, update, and all application models. Validate package content and its digest first, retain ignored entries in the original content, and report them as ignored in review without converting their RGB or alpha into `accent_color` or intensity. An independent legacy afterglow cannot be converted into an Override and is reported as ignored. Manual writes both base RGB values from one accent choice, while Automatic requests wallpaper coloring.

Published schemas 1 and 2 validate their original archive digest before in-memory conversion. Legacy packages follow the mode implied by their converted main RGB. If no usable main RGB exists, automatic coloring is selected without inventing an RGB. Old `null` settings convert to `{"state":"default"}` uncustomized states after digest verification. Schema 1/2 import and explicit Merge may convert effective legacy base balances to balance Overrides when no valid explicit Override takes precedence, writing only Overrides and preserving all base values. Legacy Win7 main base alpha may derive missing intensity (`GlassOpacity`) only when valid `GlassOpacity` is absent. There is no implicit intensity conversion when `GlassOpacity` is already present. Ignored fields remain covered by their original content digest, and catalog version behavior remains intact.

The extraction dialog labels this independent RGB capture **Include accent color**. Its initial checkbox state is the inverse of the original interactive user's `Control Panel\Desktop\AutoColorization` DWORD (missing means disabled). Read failures and invalid types are reported. The user may override the initial selection: checked captures the current original-user RGB for later manual application even if currently automatic (it does not require current manual mode), while unchecked captures automatic mode.

Catalog 3 adds `EnableUxMitigation` as a DWORD compatibility bitmask. Catalogs 1 and 2 do not cover this setting, and applying them leaves the destination value unchanged. The package schema remains 3.

## Default and application

For ordinary settings, Default means no customization in the target scope. Applying `{"state":"default"}` deletes that scope's value and deliberately leaves normal inheritance intact. It does not force the program fallback: an HKCU default can inherit HKLM, and an HKLM default leaves existing HKCU configuration effective. No additional registry state controls inheritance.

The application planner emits raw before/after changes before mutation. Explicit ordinary values applied to HKLM clear only blocking values in the original interactive user's hive. Harmless same-value user values remain. Applying to HKCU writes user values and leaves HKLM intact. Ordinary defaults delete only the target layer in both cases. No other users are examined. Windows transparency follows the mapping above. Windows color synchronization remains a separate declared user-state operation, and the five base color values are never default cleanup targets.

Before each write the plan's raw precondition is checked again. A changed precondition fails the attempt. Registry transactions cannot serialize unrelated Windows processes or prevent subsequent external color writes.

Restore defaults and Merge are configuration maintenance operations in the **More** menu, distinct from preset application. Their scope, preview, and cleanup rules are specified in [Configuration.design.md#3-operation-boundaries](Configuration.design.md#3-operation-boundaries).

## Preview checkpoints

Preview journals, attempt recovery, and checkpoint lifecycles follow the [preview transaction contract](Configuration.design.md#4-one-preview-transaction-model). Windows accent and color operations follow the [Windows color contract](Configuration.design.md#6-openglassgui-treats-color-configuration-specially).

## Library, admission and resources

The library stores one self-contained current directory per stable entry ID under `%ProgramData%\OpenGlass\Presets\Library`. Protected library and configuration directories give SYSTEM and Administrators full access, and Users and Window Manager Read/Execute. It has no authoritative index or permanent revision history. `manifest.json`, optional `LICENSE` and `assets/` remain the package content. `.accepted.json` is a protected local admission record excluded from ZIP export and package digests. Its accepted digest must match the fully validated content before application. A ZIP cannot supply admission.

External ZIPs are validated before the **Trust and import** review and reopened for digest comparison afterward. Review includes the application scope, differences, color mode, cross-layer cleanup, resource previews, conversions, ignored settings and notices. Trust authorizes applying that content locally, but it does not verify authorship. Import never applies configuration. Batch import reviews all inputs before publication. Partial publication failures report the completed imports. Changed or unaccepted entries cannot apply until explicitly reviewed and accepted again through Properties or import. General UI layout and interaction follow [Configuration.design.md#5-a-small-preset-workflow](Configuration.design.md#5-a-small-preset-workflow).

Update replaces the selected entry, including imports, without changing the original external ZIP or requiring a copy. The new content has a new package UUID, while the local entry ID stays fixed. Publication verifies a complete staging directory, rechecks the expected previous digest under the library lock, and replaces via a temporary recovery directory. Recovery directories are removed after success, not retained as history. Delete removes actual library files. Cleanup failures are reported and retried. Export reads the saved content, never the current registry. Enumeration diagnoses broken entries independently.

Application resources live separately under `%ProgramData%\OpenGlass\Configuration\Machine` and `Configuration\Users\<original SID>`, with fixed `CustomThemeReflection.png`, `CustomThemeMaterial.png`, `CustomThemeAtlas.png` and optional `.png.layout` paths. Apply and Merge materialize target-layer resources there. Ordinary external resource paths remain supported, but the GUI does not create live references into a deletable library entry or another owner's configuration directory.

Eligible protected managed files use a hard link first, then copy on failure. Directory, manifest and source-notice files are never hard-linked. External writable files are captured by copying. Never rewrite published file contents, ACLs or attributes through a shared hard link: stage a new file and replace the directory entry. The active link and rollback copies outlive library deletion. Disk storage is freed after the last required link is released. Source notices accompany active resources without a revision ancestry graph.

File contents and absence participate in the same preview checkpoint as registry and accent state. Same-path changes are dirty and refresh Theme resources. Same content is a no-op. Atlas/layout are one operation. Failed attempts restore the preceding preview. Save releases the baseline and Revert restores it. A machine-wide GUI writer lock serializes GUI previews across sessions. Reading/browsing the library remains independent. A minimal durable operation record permits the original user to recover an interrupted operation before further edits. Completed previews after an abnormal exit become the next startup baseline. This is not an Unsaved draft or persistent session undo. File cleanup failures are distinct from incomplete recovery.

Selection and dirty state remain independent. Save/Revert manage configuration changes and retained source notices. Presets may combine with settings in either layer, so no current-preset identity is tracked or persisted. Delete does not change configuration or recreate a preset on Revert. Switching, Save, Revert and close never publish presets. Uninstall keeps preset library cleanup independent of machine and current-user configuration resources, and never touches other user profiles.

The first library access converts visible legacy index entries (or recognized deployments when no index exists), validates them and records admission. A completion marker prevents deleted entries from being reimported. Original resource paths remain intact for compatibility. No registry or other-user migration occurs. New Apply/Merge operations use fixed paths. Legacy retained directories are explicitly separate from the new library's deletion behavior.

## Author and attached terms

For new presets with LICENSE, **Author identifies the declared LICENSE rights holder**, without identity verification. Without LICENSE, Author identifies a reference source, not ownership. Without source metadata, New preset starts with `Untitled preset` and the last entered author, falling back to the original interactive user's account name. Both fields are editable. A suggested account name does not verify rights. Source and Update metadata retain their existing values. Legacy package author information retains its original meaning.

Metadata is editable for both local and imported entries. Known source notices and attached external terms are carried forward as flat metadata. Updating an entry can edit its own terms without discarding retained third-party terms. There is no import-based edit lock, required license-prefix template, local ownership history or ownership inference from equal image bytes. Resources carry notices so extracting after restart or library deletion retains known sources.

## Rights envelope

New schema 3 manifests include:

```json
"rights": {
  "configuration": "openglass-attribution-v1",
  "license_scope": "image-assets",
  "sources": []
}
```

`author` has the LICENSE-dependent meaning above, and `homepage` may be empty in schema 3. `sources` retains known author/source notices as UTF-8 strings. `license` remains `null` or `{"file":"LICENSE"}`. When `LICENSE` is present, it must contain nonempty UTF-8 text (whitespace alone is rejected). An absent image LICENSE grants no additional image rights. A preset without images needs no image LICENSE.

`rights.inherited_licenses` is an optional flat array of up to 256 retained UTF-8 texts. Each text remains present in LICENSE when creating/updating a package. It preserves attached terms without imposing a prefix layout, locking metadata, proving ownership or deciding license compatibility. Native editable terms are separate from these retained texts.

### OpenGlass Attribution v1

For configuration parameters, accent RGB and layout data contributed under this convention, the contributor grants permission to use, copy, modify and share that data for any purpose. Retain the author's identification and known-source notices in redistributed copies or their accompanying metadata. Do not impose proprietary restrictions on recipients' exercise of these permissions for that contributed configuration data. These permissions apply only to rights the contributor is authorized to grant. They do not grant rights to referenced images or other third-party content. The data is provided without warranty. This convention does not assert that every configuration value is independently copyrightable.

`LICENSE` governs referenced texture, material, reflection and atlas images only under the new convention. Identify applicable third-party image terms in that file. Layout data remains configuration even though it is stored alongside an atlas image.

Schema 3 snapshots derived from schema 1–2 instead use:

```json
"rights": {
  "configuration": "legacy-package",
  "license_scope": "package",
  "sources": ["Original author and known source"]
}
```

They retain their original license text and its original scope, including any express limitations. Importing or exporting an unchanged schema 1–2 package preserves its original manifest and does not add a `rights` field. Absence of a legacy LICENSE grants no new permissions. Schema conversion never silently relicenses legacy content. Known source notices from fixed configuration resources survive restart and extraction.

## Validation boundary

All formats continue through ZIP path/case/type/count/size/ratio checks, UTF-8 and content digest verification, PNG structure/CRC and WIC compatibility checks, layout grammar validation, protected ACL deployment and UUID/content conflict checks. ZIP input is checked for traversal, alternate data streams, duplicate casing, links and nonregular entries. PNG checks do not decode the complete DEFLATE pixel stream. Final decoding happens during rendering. Packages cannot contain scripts, shaders, DLLs, executables or network assets. After review, the archive is reopened and its normalized content digest checked before deployment or application. Local saves share snapshot validation and bypass ZIP generation. Exports read saved content instead of current registry/resource sources. The test executable uses isolated temporary library roots and in-memory registry models. It must not access the production library or install/inject/restart DWM.
