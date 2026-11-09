# Historical Skittles regression fixtures

I preserve the four original full-resolution JPEGs under `samples/skittles/`, rather than storing a second copy inside the test tree. The tests refer to these files using the absolute source-root path provided by CMake, so they work from any out-of-source build directory on Linux or Windows.

| Fixture | Resolution | Purpose |
| --- | --- | --- |
| `s1.jpg` | 4032 × 3024 | Wooden background; two orange and three red, including M0.1/M0.3 location regressions |
| `s2.jpg` | 4032 × 3024 | Wooden background; previously missed dark red |
| `s3.jpg` | 4032 × 3024 | Lined paper; orange/red classification boundary |
| `s4.jpg` | 4032 × 3024 | Lined paper; general baseline regression |

I record the byte-for-byte SHA-256 of each photograph in [`skittles.sha256`](skittles.sha256). The `chromascope_fixture_integrity` CTest verifies all four checksums before I treat the original-image regression evidence as valid. The C++ regression test separately checks decode dimensions, conservative object-count bands, per-color counts, and the known red/orange coordinates from M0.1/M0.3. It does not enforce identical timings or perfectly identical Hough coordinates across CPU architectures.

The synthetic test creates its own in-memory image deterministically. The CLI export test writes temporary artifacts inside the active build directory, never into `samples/` or the source checkout.

I don't assume that these photographs are covered by the source-code MIT license; their ownership and redistribution status are documented in [`docs/ORIGINS.md`](../../docs/ORIGINS.md).
