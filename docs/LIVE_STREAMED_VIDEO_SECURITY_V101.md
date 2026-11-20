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
  Linux libcurl pins the validated numeric IP with `CURLOPT_RESOLVE`; Windows
  WinHTTP uses `WINHTTP_OPTION_RESOLUTION_HOSTNAME`. This preserves Host/SNI
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
- An optional **downloaded `.m3u8` master file** can be selected manually.
  This local text file is bounded to 256 KiB and must be a regular, non-symlink
  file. The user must still supply the original HTTPS master URL to resolve
  relative child references; the file never authorizes local segment reads.
  File loading occurs only after the explicit Start stream action and does not
  upload the downloaded manifest. A downloaded media playlist alone cannot
  provide a continuously updated live feed.
- A bounded queue, cancellation generation and local GUI texture rules from
  `AsyncCameraCapture` remain intact. Recording/inference use **the same frames**
  and require separate explicit user actions.

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
   the original URL as the HTTPS base. If unsupported by the native
   backend, confirm clear failure and no secret/URL in error output.
4. Reject an `http://`, private-IP, `.local`, credential URL, malformed URL,
   cross-origin HLS segment, redirect and iframe/webpage URL.
5. Verify Stop/clear and switching modes terminate stream access; verify that
   recorded files can contain on-screen local inference annotations when opted in.
6. Repeat on Debian 12/13, Ubuntu 24.04/26.04, and Windows 10/11 as appropriate.

**Status:** Source candidate for v1.0.1, **not release-qualified**. The
network/privacy model is deliberately more restrictive than a web browser.
