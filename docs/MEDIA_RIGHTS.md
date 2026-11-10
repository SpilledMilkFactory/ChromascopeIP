# Historical photographs — rights assessment (M1.8)

I inherited four original JPEG samples from the former SkittlesIP collaboration and retained their bytes for detection regressions. I cannot infer who pressed the shutter, who owns the copyright, or whether a prior collaborator granted permission for redistribution merely because the files were in a public GitHub repository. **No third-party rights clearance is recorded.**

| File | Origin | Copyright owner | Public distribution permission | Release state |
| --- | --- | --- | --- | --- |
| `samples/skittles/s1.jpg` | Archived SkittlesIP sample | Unverified | Unverified | Hold |
| `samples/skittles/s2.jpg` | Archived SkittlesIP sample | Unverified | Unverified | Hold |
| `samples/skittles/s3.jpg` | Archived SkittlesIP sample | Unverified | Unverified | Hold |
| `samples/skittles/s4.jpg` | Archived SkittlesIP sample | Unverified | Unverified | Hold |

The machine-readable [`fixture_provenance.json`](fixture_provenance.json) records original SHA-256 checksums and intentionally sets `release_bundle_allowed=false`. `scripts/check_provenance.py` validates this declaration; it does not prove ownership or convey permission. `--release` intentionally fails until a separate, reviewed rights-clearance change is made.

## Evidence required before authorizing redistribution

1. Establish the author/photographer for each image, including any prior collaborator contributions.
2. Obtain a dated, retrievable rights grant from each applicable copyright holder, or replace the image with a new independently created/relicensed fixture.
3. Record scope (source repo, public downloads, packaged binaries, modifications, commercial distribution, attribution), any restrictions, and evidence location **without committing private contact details**.
4. Update media terms and the fixture manifest in a dedicated PR only after verifying the evidence. Modify the audit validator to accept explicit approved statuses in that reviewed PR.
5. If rights cannot be resolved, do not include the four images in distributed packages; use verified replacement samples. Re-run regression tests with replacement fixtures and document any different expected results.

The existing public historical images require separate review as well; excluding them from a future installer does not itself settle past public distribution rights. Until clarified, I will not describe them as MIT-licensed or imply that their presence in Git history grants redistribution rights.

### Owner-supplied clearance record (complete privately, then publish a non-sensitive summary)

- Asset and SHA-256:
- Author/owner and supporting evidence:
- Rights holder's permission/grant date:
- Permitted uses and geographic/term restrictions:
- Attribution and license text required:
- Approved for repository distribution? (yes/no):
- Approved for installer/binary redistribution? (yes/no):
- Review date and PR reference:

**Current status: OPEN — human confirmation required.**
