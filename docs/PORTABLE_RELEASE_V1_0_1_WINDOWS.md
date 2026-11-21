# ChromascopeIP v1.0.1 Windows portable engineering candidate (P5)

This artifact is **not approved for public redistribution**. It is built on
GitHub-hosted Windows Server 2022 with pinned Visual Studio 2022 MSVC,
OpenCV 5.1.0, GLFW 3.4, and Dear ImGui 1.93.0.

## Extract and run

Unzip `ChromascopeIP-1.0.1-Windows-x64.zip`. Launch
`bin/chromascope-gui.exe` or `bin/chromascope-cli.exe` from the extracted
folder. The required selected Microsoft VC143 app-local CRT DLLs accompany
these executables under `bin`. The UCRT, Media Foundation, graphics drivers,
and other Windows system components remain host-provided.

Only the reviewed `object_detection_nanodet_2022nov.onnx` model is bundled
under `share/chromascopeip/models/`, together with `LICENSE`,
`NANODET_NOTICES.md`, model selection and provenance metadata. All other
unreviewed model weights and media are excluded.

## Qualification limitations

- An Actions green build is only a Windows Server 2022 build/CTest and
  relocated-CLI smoke result; it does **not** certify Windows 10 or Windows 11.
- The Windows static OpenCV build enables MSMF and does not include our
  private Linux FFmpeg backend. The supported Linux custom-HLS decoder is
  therefore **not yet qualified on Windows**. Test permitted HTTPS/HLS media
  and address any unsupported decoder path before P5 functional acceptance.
- Test local image input, recorded video, webcam devices, NanoDet model
  loading/inference, export, stop/restart and normal shutdown on Windows 10
  and Windows 11 x64.
- Microsoft VC143 redistributable DLL copying is subject to Visual Studio
  licensing conditions and the online Microsoft REDIST list. Building on a
  hosted runner or authenticating the DLL signature does not independently
  establish the distributor's rights. Record explicit clearance at P6.
- Keep the exact ZIP SHA-256 and PE dependency-closure report for the final
  release. Source/docs/permission evidence must accompany any publication.

Windows HLS may require a separately reviewed native FFmpeg integration; it
must not be considered functionally accepted solely because the Windows GUI
and CLI compile.
