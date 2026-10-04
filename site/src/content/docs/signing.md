---
title: Signing and notarization (macOS)
description: The Developer ID certificate and App Store Connect key the Build workflow uses to sign and notarize the macOS externals.
---

The workflow signs the `.mxo` bundles with a Developer ID certificate, submits
them to Apple's notary service and staples the tickets whenever the repository
has the secrets below. Without them every signing step is skipped and the
build is ad-hoc signed. Windows externals are not code-signed.

## Prerequisites

Prerequisites on the Apple side (a paid Apple Developer Program membership is
required for both):

1. **Developer ID Application certificate.** In Xcode, *Settings → Accounts →
   Manage Certificates → + → Developer ID Application* (or create it at
   developer.apple.com/account/resources/certificates). Then export it from
   Keychain Access as a `.p12` with a password: open *My Certificates*, select
   the certificate together with its private key, *File → Export Items*.
   The certificate's name is the signing identity, e.g.
   `Developer ID Application: Your Name (TEAMID1234)`; check it with
   `security find-identity -v -p codesigning`.
2. **App Store Connect API key for notarization.** At
   appstoreconnect.apple.com → *Users and Access → Integrations → App Store
   Connect API → Team Keys*, generate a key with the *Developer* role. Note the
   *Key ID* and the *Issuer ID*, and download the `AuthKey_<KEYID>.p8` file
   (it can only be downloaded once).

## Repository secrets

Then add these repository secrets (*Settings → Secrets and variables →
Actions*):

| Secret | Value |
| --- | --- |
| `APPLE_SIGNING_IDENTITY` | the certificate name, e.g. `Developer ID Application: Your Name (TEAMID1234)` |
| `APPLE_CERTIFICATE_P12` | the `.p12` file, base64-encoded: `base64 -i certificate.p12 \| pbcopy` |
| `APPLE_CERTIFICATE_PASSWORD` | the password chosen when exporting the `.p12` |
| `APPLE_API_KEY_ID` | the App Store Connect API *Key ID* |
| `APPLE_API_ISSUER_ID` | the App Store Connect API *Issuer ID* |
| `APPLE_API_KEY_P8` | the full contents of `AuthKey_<KEYID>.p8` (paste the text as is) |

## What the workflow does

What the workflow then does, in `.github/workflows/build.yml`:

1. Imports the certificate into a temporary keychain that is deleted at the
   end of the job.
2. Re-signs each `.mxo` with `codesign --options runtime --timestamp`, which
   replaces the ad-hoc signature from the build. The hardened runtime and a
   secure timestamp are both required by the notary service.
3. Zips the externals, submits them with `xcrun notarytool submit --wait`,
   fails the job (and prints the notary log) if the submission is not
   accepted, and staples the resulting tickets to the bundles with
   `xcrun stapler staple`.
4. Packages the stapled bundles; the release notes say whether the build was
   notarized.

To test the setup before tagging a release, run the workflow manually from
the *Actions* tab (it has a `workflow_dispatch` trigger) and check the
"Notarize and staple externals" step. Locally you can verify a downloaded
bundle with:

```sh
codesign --verify --strict --verbose=2 ear.objects.mxo
xcrun stapler validate ear.objects.mxo
```
