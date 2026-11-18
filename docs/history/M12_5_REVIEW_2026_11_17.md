# M12.5 (#88) — v1.0.0 dependency, license, vulnerability and privacy sign-off

**Reviewed 2026-11-17 · Decision: RELEASE SIGN-OFF BLOCKED (engineering qualification remains accepted).**

## Purpose and evidence boundary

The maintainer confirms successful fresh-VM GUI/CLI use on Windows 10/11 and
Debian 12/13 and Ubuntu 24.04/26.04. M12.4 reports **20 green GitHub Actions
checks** and an accepted baseline of **365 Python quality tests / 150 native
CTests (28 camera-labeled)**. Those results establish substantial *engineering*
qualification; they are not a dependency-vulnerability assessment, a human
license approval, a network-forensics examination or an authorization to publish.

This is a **source and documented-dependency review** of the M12.4 checkout,
plus a dated consultation of authoritative upstream and distribution security
resources. The exact five 1.0.0 CI binaries and their SBOM drafts were **not
supplied with this source archive**. Consequently this review cannot resolve
artifact-specific library composition, NOTICE completeness, patched runtime
versions, MSVC licensing entitlement or outstanding model/media rights.

**No historical commit reconstruction, additional desktop VM testing or new
per-platform JSON attestations are required to document this milestone.**
Only the genuine unresolved security and redistribution decisions below need
further action. Do not turn this document into a substitute for them.

## Four-part decision register

| Area | Established evidence | Decision / outstanding work |
| --- | --- | --- |
| Dependency identity | CMake pins OpenCV **5.1.0**; Windows CI pins GLFW **3.4** and ImGui **1.93.0**; Windows packages use selected Microsoft VC143 DLLs; Linux packages ship private OpenCV and depend on distribution codec/graphics libraries. M11.9 emits five artifact-derived CycloneDX 1.6 **drafts**. | **PARTIAL** — compare final binaries and exact resolved distro/codec versions against the same-run drafts; imports alone cannot enumerate static objects or dynamically loaded plugins. |
| Licenses and notices | First-party source is MIT; NanoDet weights and historical images are excluded from packages; source provenance and limited notice files are present. | **BLOCKED** — M11.9's 29 package/topic decisions are not human-approved. Verify the pinned OpenCV source's LICENSE/NOTICE/3rdparty attribution (not merely the standard Apache text copied from NanoDet), embedded Protobuf and codecs, GLFW/ImGui notices on *all applicable builds*, Microsoft VC143 REDIST entitlement, and codec/patent conditions. Historical photographs in the *public repository* have separately unresolved rights. |
| Vulnerabilities | CI runs ASan/UBSan on first-party CLI code, clang-tidy, malformed-input regressions and native CTests. Consulted Debian/Ubuntu vulnerability trackers on **2026-11-17** (details below). | **BLOCKED** — CVE-2026-8461 affects FFmpeg's MagicYUV decoder and is officially marked vulnerable on three supported Linux releases. Verify actual installed library versions, reachability, supported security fixes/backports, and all other relevant advisories; passing tests do not rule out known decoder vulnerabilities. |
| Privacy | Current first-party code is local-first. Camera activation and inference require explicit action; no built-in telemetry/account/network upload; media/JSON exports and preferences are user-initiated. M10.5/M10.8 document residual threats. | **REVIEWED FOR FIRST-PARTY DESIGN, NOT CERTIFIED** — OS, camera drivers, third-party codecs, crash dumps, redirected outputs and possible OS/network behavior are outside that source-only claim. Physical camera and privacy-sensitive outputs remain subject to consent and user retention policy. |

## Current urgent advisory: CVE-2026-8461 (FFmpeg / MagicYUV)

The published vulnerability describes an **out-of-bounds write** in the FFmpeg
`libavcodec` MagicYUV decoder, potentially allowing denial of service and
code execution when a malicious media file reaches the vulnerable decoder.

**Official tracker snapshot, consulted 2026-11-17** (not a substitute for
checking the installed package and latest advisory on release day):

| Target distribution | Official tracked state on 2026-11-17 | Primary source |
| --- | --- | --- |
| Debian 12 Bookworm | **Vulnerable** (tracker lists `7:5.1.9-0+deb12u1`). | https://security-tracker.debian.org/tracker/CVE-2026-8461 |
| Debian 13 Trixie | **Fixed** in `7:7.1.5-0+deb13u1`. | https://security-tracker.debian.org/tracker/CVE-2026-8461 |
| Ubuntu 24.04 Noble | **Vulnerable** (Ubuntu package status). | https://ubuntu.com/security/CVE-2026-8461 |
| Ubuntu 26.04 Resolute | **Vulnerable** (Ubuntu package status). | https://ubuntu.com/security/CVE-2026-8461 |

The upstream FFmpeg security information is at https://ffmpeg.org/security.html .
The vendor tracking statuses, rather than unqualified numeric upstream version
comparisons, should determine whether distro-backported fixes apply.

ChromascopeIP's Linux package **does not embed** the distribution's FFmpeg
shared objects; `videoio` can use distribution FFmpeg for local recorded-video
processing. That means this is a **potential downstream runtime exposure**,
not proof that ChromascopeIP's ZIP or `.deb` contains vulnerable code, and not
proof the vulnerable decoder is reachable in every current configuration.
However, no available first-party test demonstrates that MagicYUV is disabled
or unreachable. Do not assume its absence, or equate a clean recorded-video
smoke test with a security mitigation.

**Shortest honest remediation path:** check updated vendor trackers and the
real `libavcodec*` package on each Linux target; establish a vendor-patched
version, an enforceable decoder exclusion with tests, or a sufficiently
restricted shipped feature scope with documented limitations. Re-run affected
video/packaging checks after changing any decoder linkage/configuration.
If no effective mitigation can be established, leave the affected-target
security decision **BLOCKED**, rather than automatically publishing that
candidate as cleared. Debian 13's tracked fix is not blanket FFmpeg clearance:
it still requires the normal complete dependency/advisory review.

On a *target machine*, a quick non-mutating first check is:

```bash
dpkg-query -W 'libavcodec*' 'ffmpeg' 2>/dev/null || true
apt-cache policy libavcodec59 libavcodec60 libavcodec61 libavcodec62 2>/dev/null
```

Check the relevant actual library's vendor advisory and installed package
version; package names vary. These commands **do not** establish that a
particular decoder is enabled, patched, or exploited. Do not run `apt upgrade`
on a customer's system solely because of this document.

## Focused review — licensing and notices

- `scripts/package_debian12.py`, `package_debian13.py`, `package_ubuntu24.py`
  and `package_ubuntu26.py` currently copy
  `licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt` to a package file named
  `OpenCV-5.1.0-Apache-2.0.txt`. The standard Apache terms alone **do not prove**
  all OpenCV source-revision NOTICE and third-party component obligations.
- `scripts/package_windows_x64.py` uses the same Apache-text substitution,
  but also takes pinned GLFW and Dear ImGui license inputs. Confirm full
  static-transitive dependency inventory and permitted VC143 runtime DLLs.
- Validate the actual distributed `.deb` / ZIP notice directories and exact
  source commit's upstream licenses before asserting clearance. Use existing
  M11.9 `redistribution-audit.json` and `redistribution-review-template.json`
  for internal review **when available**, not to manufacture 29 approvals.
- Linux FFmpeg and GPU/graphics dependencies are distribution-managed in the
  current packages. This changes who distributes binaries; it does **not**
  remove the need to identify license, codec configuration and vulnerability
  exposure. Microsoft Visual Studio 2022 runtime redistribution is limited to
  qualifying licensed users and the REDIST list:
  https://learn.microsoft.com/visualstudio/releases/2022/redistribution and
  https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files .
- NanoDet ONNX weights are external; user-provided models carry their own
  terms. Historical `samples/skittles/s1.jpg` through `s4.jpg` have unproven
  original-photo rights. Their exclusion from binary packages does not prove
  the already-public source repository has permission to republish them.

## Focused review — vulnerability and privacy

- Check authoritative OpenCV source-release advisories, GitHub security
  advisories for GLFW/ImGui, exact embedded Protobuf version, Linux vendor
  packages, Microsoft VC143 runtime updates and the actually built Windows
  codec backend. A GitHub advisories page with no published advisories is **not
  evidence that a dependency has no vulnerabilities**.
- A source-only security review does not execute a live vulnerability scan of
  the CI-built binaries. No SCA report for the exact five 1.0.0 artifacts was
  supplied; all material findings must be triaged before final sign-off.
- The application does not itself implement cloud uploads or telemetry, but
  handles sensitive user-selected media, model files and camera frames. Default
  no-capture/no-export behavior, explicit save, temporary tracking and manual
  camera activation are covered by `docs/PRIVACY_GUIDE.md` and existing tests.
  The application does **not** securely erase OS swap, browser caches, crash
  dumps, redirected stdout or other software's diagnostic logs. Obtain consent
  as applicable and do not promise universal network isolation.
- Native sanitizers and fuzz seeds reduce some risks but do not sandbox
  OpenCV/FFmpeg decoders or prove that the entire application is memory-safe.

## Minimal release-gate decision (no duplicate VM bureaucracy)

**Engineering:** maintain successful M12.4 native, Python, sanitizer, static,
package, and 20-check same-run CI evidence. Reported Windows and Linux VM
acceptance remains valid at its originally observed scope.

**Before public v1.0.0 package publication, resolve the genuine blockers:**

1. **Security:** disposition CVE-2026-8461 for Debian 12 / Ubuntu 24.04 /
   Ubuntu 26.04, and review other applicable advisories against exact built and
   OS-provided versions. Do not call this a PASS based on CI alone.
2. **Rights:** resolve or appropriately restrict each applicable bundled
   third-party license/NOTICE/redistribution obligation. Do not auto-approve
   M11.9 review entries or imply the original photographs are MIT-licensed.
3. **Privacy and publication:** preserve first-party local-only defaults,
   disclose actual limitations, and keep final tagged artifact checksums,
   signing requirements and public GitHub Release as explicit later steps.

If an issue is unresolved, record it as **BLOCKED or UNKNOWN**, not PASS.
This M12.5 review does not create tags, upload releases, access cameras, run
network scans, or change production behavior. **No external legal opinion,
complete SCA clearance or universal privacy certification is asserted.**
