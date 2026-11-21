# Sample photographs — maintainer authorship confirmation

**Status: OWNER_CONFIRMED — 2026-11-18.**

During the v1.0.0 clearance work, the maintainer stated:

> the 4 skittles pictures in samples/skittles are pictures I took with my own camera so there is no licensing issue there

This supplies the authorship evidence that was previously missing. The four
original files remain byte-identical and retain their historical regression
coverage. The statement supports their continued repository/source-archive
redistribution by the maintainer. It does not establish a broad MIT license for
third-party reuse of the photos, and does not approve dependencies or publication.

| File | Author/rights evidence | Current status |
| --- | --- | --- |
| `samples/skittles/s1.jpg` | Maintainer's own-camera statement above | OWNER_CONFIRMED |
| `samples/skittles/s2.jpg` | Same statement | OWNER_CONFIRMED |
| `samples/skittles/s3.jpg` | Same statement | OWNER_CONFIRMED |
| `samples/skittles/s4.jpg` | Same statement | OWNER_CONFIRMED |

The source-only `docs/fixture_provenance.json` records the original SHA-256
values, the statement, date and scope. `scripts/check_provenance.py --release`
now accepts the evidenced owner-confirmed state and still rejects missing or
modified bytes and unsupported rights declarations. Schema 1's unresolved state
remains fail-closed in regression tests. No reviewer or approval date was invented.

The previous OPEN finding used `release_bundle_allowed=false` because authorship
was then unknown. That historical finding is superseded by the owner's statement;
it was not evidence that someone else owned the images. No Git history rewrite,
source-archive exclusion, replacement images, or further owner permission is needed
on the facts now supplied. Historical v1.0.0 binary packages excluded photographs. The v1.0.1
portable Linux tarball and Windows ZIP explicitly include these four owner-made
photographs, with SHA-256-pinned contents and no other bundled media. The
maintainer provided authorization for this first-party distribution scope;
this does not extend the root source MIT license to the images.

For other user-selected media, obtain the applicable rights and consent yourself.
This statement covers only these four files and does not cover recordings,
training datasets, ONNX weights, or the two untracked local sample videos.
