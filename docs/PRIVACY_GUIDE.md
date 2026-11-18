# ChromascopeIP — Privacy, local data and consent guide (M10.8 / #74)

ChromascopeIP's **first-party processing runtime** reads user-selected local media and trusted ONNX files, performs local CPU inference/processing, and does not implement a network upload, telemetry, cloud inference, account, face recognition, biometric identity or persistent person-identity database. This describes reviewed project code, **not** every OS service, mounted filesystem, OpenCV/FFmpeg plugin, driver, shell, other software or future version. Build/bootstrap scripts can download development dependencies. It is not an air-gap certification. See [M7.6 privacy contract](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M7_6_LOCAL_PRIVACY.md), [security policy](../SECURITY.md) and [M10.5 audit](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_5_THREAT_BOUNDARY_AUDIT.md).

## When data is accessed

- Opening **Still image** reads the selected existing media file; loading a registered classical filter does not invoke NanoDet. Explicit `object`/`person` processing requires a separately provided, integrity-checked local ONNX model.
- Opening **Recorded video** reads a selected local file. Source audio is not played, and annotated AVI exports intentionally **drop audio**. The original source remains where it was stored.
- Selecting the **Live camera** workspace or calling `--camera-backends` does not open hardware. Camera Start / CLI `--camera` / `--camera-detect` **do**; you must have permission to capture people or spaces.
- Selecting model file paths or restoring saved GUI preferences is **not** permission to load a model or activate inference/camera hardware. `--privacy-info` is a no-media-read standalone CLI command.

## Storage and persistence boundaries

| Data | Default | Explicit persistence / caveat |
| --- | --- | --- |
| Original still/video/model | Remains at source path | ChromascopeIP does not delete the original |
| Derived frame/tensor/detection/track | Held in bounded process memory | Some buffers/OS swap/driver/GPU allocations may outlive display removal; not securely wiped |
| GUI file browsing and navigation | Session-local | No auto ImGui `.ini` or log persistence; OS/window manager may retain state |
| Validated GUI preferences | May be read at launch | **Only** Save preferences writes non-sensitive schema-v1 values |
| Annotated image, JSON, JSONL, AVI | Not written without an export request | User-chosen explicit export persists and may reveal faces/people, locations or personal data |
| Camera PNG frames | Not written in default capture | `--camera-save-dir NEW` explicitly writes PNGs; shell redirect can separately retain JSONL |
| M10.7 soak reports | No automatic tests on normal startup | Explicit create-only aggregate report, no raw frames/boxes/paths; source inputs themselves remain available |
| Temporary tracking IDs | Optional per live session | No identity match/history persisted by this feature |

Preference locations: Linux `$XDG_CONFIG_HOME/chromascopeip/preferences-v1.conf` or `$HOME/.config/chromascopeip/preferences-v1.conf`; Windows `%APPDATA%\ChromascopeIP\preferences-v1.conf`. The versioned format rejects unknown/invalid values rather than silently adopting them. It does not store input/output paths, model paths, camera indices, detections or tracking IDs. Delete saved preferences separately from clearing the running session. See [M10.2](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_2_VERSIONED_PREFERENCES.md).

## Clearing a session and deleting outputs

GUI **Clear loaded media, model and results** requests cancellation and clears owned application state, including previews and transient detection state. Live **Stop**, workspace switch and reset request camera release; native driver calls may block. These actions **do not remove** original files, previously created exports, a separately saved preference file, application crash dumps, OS pagefile/swap data, third-party caches, terminal history, shell redirections, sync/backup copies or existing `imgui.ini` created by older versions. Review storage with your normal OS tools; delete retained files explicitly according to your needs.

## Safety, accuracy, and consent

Object/person bounding boxes are coarse category predictions with uncalibrated scores. **Temporary numeric track IDs are not unique real-world identities**; they can switch or reset. No facial recognition or identity matching is implemented. Real-time capture can include bystanders: obtain consent when necessary, avoid publicizing raw screenshots and JSONL, and choose retention policies appropriate for your environment.

Media/model parser and driver threats remain described in [M10.5](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_5_THREAT_BOUNDARY_AUDIT.md); M10.6 fuzz tests do not prove third-party codec safety; M10.7 repeated live-camera success does not prove all-device availability or deterministic driver release. Consult [local data rights](MEDIA_RIGHTS.md), [model provenance](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M5_7_MODEL_SECURITY.md), and [troubleshooting](TROUBLESHOOTING.md) before distributing recordings, models or images.

## Privacy verification checklist

Use `chromascope-cli --privacy-info`; start the GUI in an empty working directory and check it does not create `imgui.ini` or a log; switch to camera workspace and verify CAMERA OFF; save/reload/delete preferences explicitly; export media only to a controlled directory and verify no implicit output is created. Perform genuine hardware checks where authorized. Automated CTests cover contracts but do not constitute a privacy or network-forensics certification.

## M12.5 — privacy release-review boundary

The [M12.5 review](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/M12_5_RELEASE_SECURITY_SIGNOFF.md) confirms the documented
first-party privacy design (opt-in camera, local inference, no application
telemetry, explicit export/save) as a **source-level review**, not a verified
absence of all OS or third-party networking, crash dumps, codec data retention,
or device-driver behavior. Any release privacy statement must preserve those
limits and should not assert certified isolation or secure erasure.

## 2026-11-18 decoder review

The corrected Linux OpenCV build excludes MagicYUV, CineForm and SANM decoding
and excludes audio decoders during stream probing. Recorded-video processing
remains local; source audio is ignored. Camera activation, model selection,
tracking, preferences and explicit export controls are unchanged. FFmpeg may
write diagnostic messages to stderr for an excluded stream. This restriction
does not sandbox native parsers or certify absence of OS/network behavior.
The [current source clearance report](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/M11_9_M12_5_CLEARANCE_REPORT.md)
describes remaining release blockers.
