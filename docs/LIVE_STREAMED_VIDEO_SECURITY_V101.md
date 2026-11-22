# Live Streamed Video — v1.0.1 restricted-media preview candidate

This is a **new, explicitly opt-in** network mode. It is not a web browser,
website scraper, iframe renderer, anonymous VPN, or substitute for the
operator's authorization and site terms. Copy the **direct media feed URL**
from a feed provider; do not paste the surrounding HTML page URL.

## Privacy and consent

- Opening the program, switching workspaces, reloading preferences and typing
  a URL trigger **no DNS lookup and no network request**. Only **Start stream**
  authorizes network access. Stop, session clear, leaving this workspace and
  application shutdown request cancellation; in-flight platform HTTP work can
  take up to its configured timeout to finish.
- The pasted URL (including any opaque query token) is held **in memory only**,
  is not saved in schema-v1 Preferences, included in a diagnostic log, copied
  to recorded filenames, or uploaded to ChromascopeIP servers.
- The camera operator/CDN can still see the user's public IP address, TLS
  server name, request timing and ordinary HTTP headers. The user's operating
  system DNS resolver and certificates may involve trusted third parties.
  ChromascopeIP cannot guarantee anonymity or hide a direct connection.
- No JavaScript, webviews, cookies, authentication, embedded frames, ad code,
  analytics libraries, automatic preflight URLs, background feed discovery,
  browser Referer, or follow-redirect mode is involved. An optional, explicit
  **Firefox-compatible (User-Agent only)** request identity can be selected
  before Start, for providers permitting independent-client playback. This is
  not a Firefox session and does not supply a Referer or access rights.
- User-controlled local ONNX inference remains local; live recording is still
  separately opt-in and saves to local disk only when started. Its existing
  user-visible overlay burn-in applies to streamed frames as well.

## Security rules

- **HTTPS only** with a regular public DNS hostname and port 443. Host IP
  literals, URL user-info, private/internal host suffixes, ambiguous control
  characters and path encodings are rejected before DNS.
- The application resolves the hostname and rejects non-public DNS answers.
  Linux libcurl pins the validated numeric IP with `CURLOPT_RESOLVE`; the P5
  portable Windows build uses pinned static MSVC libcurl with Schannel and the
  same `CURLOPT_RESOLVE` policy, avoiding older WinHTTP option dependencies.
  Historical non-P5 Windows builds still use WinHTTP and fail closed when its
  DNS-pinning option is unavailable. This preserves Host/SNI
  and certificate validation while blocking DNS rebinding to private addresses.
- HTTPS peer and hostname checks remain **enabled**; redirects are denied.
  Proxies and ambient cookies are disabled. Individual requests have size and
  time budgets; DNS validation runs on the capture worker.
- The default HTTP User-Agent remains `ChromascopeIP/1.0 (+private-media-viewer)`.
  The session-only optional identity sends the Firefox 158 Linux User-Agent
  used by a successful one-header compatibility test. It is applied to every
  source, playlist and segment request in the selected stream and is not saved
  in preferences. A 200 for a master playlist does not establish that the
  operator permits use, or that media playlists and segments will succeed.
- Remote bytes are not handed to FFmpeg as URLs: only bounded **in-memory**
  content is decoded by the existing OpenCV build. A still-image dimension
  preflight rejects oversized geometry prior to image decode. This does not
  make untrusted media codecs memory-safe; decoder security updates remain
  necessary.
- The HLS reader supports exactly **one single-variant master playlist level**
  before a standard MPEG-TS media playlist. Each relative master child and
  segment resolves only against its original public HTTPS origin. No multi-
  bitrate variants, alternate audio/renditions, nested masters, substitutions,
  encryption, key requests, fMP4, redirects, cross-domain CDNs, or iframe loads.
  HLS media playlists may contain validated metadata-only discontinuity markers,
  `EXT-X-INDEPENDENT-SEGMENTS`, and exactly `EXT-X-KEY:METHOD=NONE` (no key
  retrieval); encrypted keys and unreviewed media features remain prohibited.
  Unsupported media directives are reported by a fixed directive name without
  echoing server-controlled text or URLs.
- An optional **downloaded `.m3u8` master file** can be selected manually.
  This local text file is bounded to 256 KiB and must be a regular, non-symlink
  file. The user must still supply the original HTTPS master URL to resolve
  relative child references; the file never authorizes local segment reads.
  File loading occurs only after the explicit Start stream action and does not
  upload the downloaded manifest. A downloaded media playlist alone cannot
  provide a continuously updated live feed.
- Live MPEG-TS playback now uses the media playlist's bounded `EXT-X-TARGETDURATION`
  to schedule reloads (target duration after changes, half after unchanged
  replies), rather than treating five ordinary non-advancing polls as fatal.
  The reader waits interruptibly and only retries the same validated HTTPS
  media-playlist origin; it never retries unlisted segments or redirects.
  After prolonged non-advancement (at least 90 seconds, capped at five minutes
  depending on target duration) it reports a stalled feed rather than
  silently appearing live forever. Stop remains explicit and interruptible.
- **Asynchronous HLS prefetch (v1.0.1):** a dedicated bounded worker now
  retrieves validated media segments before the synchronous OpenCV/FFmpeg
  demuxer needs them. A bounded cache retains the currently validated media
  playlist so that *all* advertised sequential segments are downloaded
  promptly; the target duration applies only to playlist reloads, never as a
  delay between already-advertised media files. At most four completed
  16 MiB-or-smaller segments may wait in the prefetch queue; the decoder
  separately retains at most four segments in its seek window. An explicitly verified discontinuity may hold
  one additional 16 MiB pending segment during the transition. These caps
  bound first-party in-memory buffering without granting FFmpeg any URL,
  cookies, credentials, redirects or additional network access. Prefetch is
  session-only and its worker is joined before the reader or network state
  is destroyed; an already-running HTTPS request may still take its configured
  maximum timeout to finish during cancellation. The decoder now receives
  contiguous AVIO reads across ordinary segment boundaries, avoiding a short
  read solely because one `.ts` file ended. Normal playback initially begins
  up to three listed segments behind the live edge (or the oldest advertised
  segment if fewer are available). FFmpeg's first AVIO read waits for a
  three-segment reserve, but no longer than eight seconds and not past a
  verified producer EOF; this adds intentional latency to absorb jitter.
  These limits can cost up to 64 MiB in completed-segment prefetch storage,
  plus the four-segment in-decoder seek window and one pending discontinuity
  segment. A source that advertises too few segments or delivers more slowly
  than realtime may still stall. Decoder recovery can still resynchronize
  at a newly advertised segment and may
  briefly display damaged H.264 frames if that segment lacks a keyframe.
- MPEG-TS payloads from successive HLS media sequences are passed to **one
  persistent OpenCV/FFmpeg demuxer**, instead of reopening a decoder for each
  short segment. This allows H.264 reference frames to survive ordinary
  segment boundaries and avoids repeated container probing. The decoder is
  supplied only with an in-memory `cv::IStreamReader` and **never** receives
  remote URLs, credentials, or access to the network. A four-segment rolling
  window (at most 64 MiB, using the existing 16 MiB per-segment download cap)
  supports bounded backward seeks during demuxer probing; it refuses seeks
  outside retained bytes or to the unknown end of a live stream.
- After startup, available HLS segments are fetched **in sequence** so the
  decoder retains inter-frame prediction dependencies. A missing/expired
  sequence or an explicit `EXT-X-DISCONTINUITY` instead causes a bounded
  decoder reset using the already verified next segment, rather than joining
  incompatible TS state. The initial live position is still near the current
  edge. During transient network delays, preview latency can increase; a
  feed is not guaranteed to have the same low latency as a full HLS player.
  Callbacks do not unwind C++ exceptions through FFmpeg, manual Stop remains
  interruptible, and the existing target-duration reload/stall limits apply.
- When OpenCV unexpectedly fails to decode the next video frame **without**
  a validated playlist boundary, the reader releases that decoder and starts
  a fresh in-memory decoder at the latest newly advertised HLS segment. It
  never replays a failed segment, never sends a remote URL to FFmpeg, and
  bounds consecutive failed restarts to four attempts before reporting an
  explicit error. A successfully decoded frame resets the attempt budget.
  TLS, DNS, HTTP-policy, playlist-policy, cancellation and byte-window failures
  still propagate immediately; they are not converted into decoder retries.
  Only a timed-out HTTPS HLS **master or media playlist** is eligible for two
  additional fetch attempts (three total). Each retains the 9-second HTTPS
  timeout, fresh public-DNS validation and address pinning, a 256-KiB response
  cap, and short cancellation-aware 250/500-ms waits. An exhausted timeout
  is still reported as a stream error; segment download failures, certificate
  failures and policy rejections are never retried. No new decoder, segment
  replay, or change to presentation-timestamp scheduling is introduced.
  This best-effort resynchronization can briefly interrupt preview and does
  not guarantee that a segment begins with an independently decodable keyframe.
- H.264 errors from malformed transport data or a source whose GOP cannot
  recover after a sequence gap are still possible; this is not a full adaptive
  HLS player and does not perform error-concealment or fix corrupt remote
  segments. The codec whitelist is unchanged; AAC audio remains disabled.
- A bounded queue, cancellation generation and local GUI texture rules from
  `AsyncCameraCapture` remain intact. Recording/inference use **the same frames**
  and require separate explicit user actions.

### M12.5 session-only HLS playback-cushion trial

The Live Streamed Video GUI now offers three **session-only startup** presets:
**Low delay** (three advertised segments behind the live edge, three-segment
prefetch reserve), **Balanced** (four behind, four-segment reserve, GUI default),
and **Smoother** (five behind, four-segment reserve). The existing four-segment
queue cap, four-segment seek window, verified HTTPS requests, 8-second maximum
reserve wait, cancellation, sequence continuity and codec policy are unchanged.
On feeds with fewer listed segments the selected position falls back to the
oldest advertised segment; this is not a guarantee of a fixed number of seconds.
Changing the preset requires stopping and restarting the stream. Selection is
not saved to preferences or included in recordings. Frame rate remains that of
the decoder; no time-stretching or skipping of reference pictures is performed.
A larger cushion may reduce ordinary network jitter, but cannot correct a
sustained download-rate deficit, irregular timestamps, or decoder crashes.
The separate FFmpeg/OpenCV private runtime remains unqualified for distribution.

## Supported input scope

| Feed supplied | Current behavior |
|---|---|
| HTTPS JPEG/PNG snapshot (including extensionless URLs) | Polls at most twice/second, decodes image only |
| HTTPS multipart MJPEG | Captures individual bounded JPEG frames; no HTML execution |
| HTTPS HLS `.m3u8` master with exactly one variant | Resolves one same-origin child media playlist; decodes bounded MPEG-TS segments in memory when supported |
| HTTPS HLS `.m3u8` media playlist | Fetches bounded playlist/segments and decodes in memory when supported |
| Downloaded `.m3u8` **master file**, with original HTTPS URL | Reads the local master as text after Start and resolves its child against the original HTTPS origin; no local segment loading |
| HTTPS bounded `.mp4` | Fetches a finite clip (maximum 32 MiB) and decodes in memory; end-of-file is **not** a true continuous live stream |
| Multi-variant/nested HLS master, fMP4, AES/DRM or cross-origin segment | Refused; explicit restricted-support limitation |
| `iframe`/webpage URL, browser-only viewer or JavaScript player | Refused; not a video payload |
| HTTP, RTSP, credentials, local/private address or certificate failure | Refused |

OpenCCTV and other services sometimes link to **iframes**, multi-variant
masters, authenticated sources or cross-domain CDNs. A copy-feed URL is not a
universal compatibility guarantee. For EarthCam-style single-variant master
files (such as a downloaded `playlist.m3u8` pointing to one relative
`chunklist_*.m3u8`), **prefer pasting the original HTTPS URL directly**.
The downloaded file is optional and does not contain the video frames. Our policy deliberately refuses some otherwise playable
streams rather than silently weakening consent, origin restrictions or network
privacy.

### Direct-feed access errors

The reader reports HTTP access denial, redirect refusal, rate limiting,
request timeout, byte-budget exhaustion and TLS failures separately. These
messages are generated locally from trusted status codes only; remote URLs,
query tokens, response bodies and headers are **never** included. The UI only
shows `STREAM LIVE` after a decoded frame arrives; connecting, buffering and
failed requests have distinct states. CTests are offline, so green tests do
not imply any third-party service grants direct access.

An `HTTP 403 Forbidden` response means that the remote server denied that
request. A saved master `.m3u8` may bypass re-fetching that initial *text*,
but it cannot authorize the child playlist or video segments. For an explicitly
permitted direct-client source, an opt-in Firefox-compatible User-Agent may
improve request compatibility, as demonstrated by a one-header comparison.
It does **not** bypass authentication, add a Referer, create a browser session,
or grant playback/recording rights. Do not use this mode to evade a provider's
access restrictions; obtain supported integration details when required.

### Cross-platform qualification

Linux builds require the system `libcurl4-openssl-dev` development package; the
resulting Debian/Ubuntu distributions link to the system libcurl runtime.
Windows builds use OS WinHTTP (Windows 10 version 21H1 or newer is required for
DNS pinning); Windows Media Foundation may not be able to decode a fetched HLS
MPEG-TS byte stream. Both platform's network and decoder behavior must be
qualified on real supported targets before publication; passing URL-policy
unit tests does **not** establish live-feed compatibility.

## Manual test checklist

1. Confirm switching into the workspace performs **no** request or camera open.
2. Paste a trusted HTTPS JPEG camera snapshot and click Start; verify a changing
   preview, Stop and no local camera permission prompt.
3. Test an approved same-origin MPEG-TS HLS feed, including a single-variant
   master. Optionally select its downloaded `.m3u8` text file while keeping
   the original URL as the HTTPS base. Observe at least 2 minutes of playback,
   including normal non-advancing playlist reloads. Verify live-edge catch-up,
   cancellation during a stalled playlist, and that no secret/URL enters errors.
   If unsupported by the native backend, confirm clear failure.
4. Reject an `http://`, private-IP, `.local`, credential URL, malformed URL,
   cross-origin HLS segment, redirect and iframe/webpage URL.
5. Verify Stop/clear and switching modes terminate stream access; verify that
   recorded files can contain on-screen local inference annotations when opted in.
6. Repeat on Debian 12/13, Ubuntu 24.04/26.04, and Windows 10/11 as appropriate.

**Status:** Source candidate for v1.0.1, **not release-qualified**. The
network/privacy model is deliberately more restrictive than a web browser.
