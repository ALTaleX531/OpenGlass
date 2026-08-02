# Real-OS release validation

Use this gate before claiming support for a new layout interval or publishing a release. Exact OS build/revision, module PE/PDB identities, and observed compositor capabilities are authoritative; marketing labels are not.

Static schema validation, an x64 Release build, and an IDA semantic audit are prerequisites, not substitutes for a real-OS run. PE size and projected-wrapper machine code may be inspected while changing the projection mechanism, but they are diagnostics rather than fixed release gates. A virtual machine can pre-screen installation and failure paths but cannot satisfy GPU, DWM recovery, or multi-display coverage.

## Recovery prerequisites

Before first injection, record all of the following. Mark the run `BLOCKED` if any item is missing.

- Use a non-primary test machine that can tolerate desktop loss, with local administrator access and an operator present.
- Verify WinRE or Safe Mode access and store the BitLocker recovery key off the machine.
- Create a usable system image or restore point; uninstalling one Windows update is not a recovery plan.
- Export relevant HKCU and HKLM DWM/OpenGlass configuration.
- Retain the previous official installer and matching `OpenGlassHost.exe`/`OpenGlass.dll` pair with hashes.
- Prepare an offline or Safe Mode procedure to disable `OpenGlassHost` if the desktop cannot start.
- Verify the service-stop path and `Ctrl`+`Win`+`Shift`+`Q` emergency recovery before risk testing.
- Record the operator, recovery owner, recovery entry point, and recovery-asset location.

Do not perform first injection through the machine's only remote connection or in an unattended environment.

## Required scenarios

| ID | Scenario | Required observation |
|---|---|---|
| RV-01 | Clean install, cold symbol cache, first start | Inject only a valid DWM target; fail safely on symbol/projection errors; no restart loop |
| RV-02 | Reboot, sign-in/out, session switch | Correct per-session injection and HKCU isolation |
| RV-03 | Visual and occlusion stress | Overlap, move, resize, minimize, Snap, maximize, Task View, activation, multiple displays and DPI without DWM instability or repeatable corruption |
| RV-04 | GUI transaction behavior | Immediate writes work; Save commits; Revert and unsaved close restore; HKCU/HKLM precedence is correct |
| RV-05 | Service lifecycle | Pause prevents new injection, Continue resumes, Stop unloads without reinjection, Start recovers |
| RV-06 | Recovery | Emergency shortcut and service stop recover DWM and prevent a crash loop |
| RV-07 | Upgrade and signing | Host and DLL are from the same build and use a compatible signing policy; no stale-file mixture |
| RV-08 | Uninstall | Service and injection are removed; both retain-config and delete-config paths behave as selected |
| RV-09 | GPU residency | Interactive effects and normal desktop use remain stable for the declared test duration |
| RV-10 | Unsupported or damaged input | Mismatched versions, symbols, or artifact pairs are rejected without injection or DWM failure |

For a new right boundary, run the relevant scenarios on a supported version immediately before the boundary, the first available version at or after it, and the newest version included in the support claim. A screenshot of an idle desktop is not runtime coverage. `OpenGlassRenderTest.exe` is an interactive GPU exercise, not an automated test.

## Non-waivable failures

Mark the candidate `FAIL` for any DWM crash, hang, black screen, restart loop, wrong-process or wrong-session injection, failed recovery, stable rendering corruption, incorrect registry transaction, Host/DLL mismatch, unsafe projection failure, or incomplete uninstall. Missing exact binary identity, recovery evidence, or a required scenario also blocks the corresponding support claim.

Only a non-functional issue that does not affect safety or the stated support surface may be recorded as `PASS WITH KNOWN ISSUE`.

## Report contract

Record:

- release version, commit, branch, configuration, date, operator, and final `PASS`, `FAIL`, or `BLOCKED`;
- installer, Host, DLL, and GUI hashes, file versions, signer, and signature result;
- physical/virtual platform, CPU, GPU/driver, displays/DPI, OS edition, architecture, exact build/revision, and KB;
- dwmcore/uDWM paths, hashes, PE versions, and PDB GUID/age;
- projection, value/unit, semantic evidence, independent cross-check, confidence, and right-boundary interval;
- recovery assets and readiness, each scenario's expected/actual result, service/DWM lifecycle, loaded-DLL evidence, Event Log/WER/dumps, and relevant screenshots or recordings;
- registry values before/after, effective source, Save/Revert/close result, failure recovery, and all coverage gaps;
- separate offset-audit, real-OS validation, and release approvals. Automation cannot sign for the real-OS test operator.
