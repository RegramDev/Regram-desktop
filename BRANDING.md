# Regram Desktop

Regram is an independent Telegram client based on Telegram Desktop. Upstream source history, copyright statements, GPLv3 license with OpenSSL exception, and third-party notices remain intact. Nagram-Qt and AyuGramDesktop sources are integrated and credited in `docs/regram/references.md`. Regram uses the user-supplied monochrome portrait as the default identity. The original image is retained as `Telegram/Resources/branding/regram-source.jpg`; platform encodings preserve its full square composition. All selectable icon themes render Regram artwork; legacy theme identifiers remain for preference compatibility. Nagram artwork is not licensed for third-party branding and is excluded.

| Identity | Value |
| --- | --- |
| Display name | Regram Desktop |
| Executable / macOS bundle | Regram / Regram.app |
| CMake target | Telegram |
| macOS bundle ID | dev.regram.desktop; Xcode Debug uses dev.regram.desktop.debug |
| Linux desktop / D-Bus ID | dev.regram.desktop |
| Qt application name / Linux WM class | RegramDesktop |
| Windows application user model ID | RegramDev.RegramDesktop |
| Windows installer ID | A546C479-F931-54ED-8D86-4C1B199C377E |
| Portable folder | RegramForcePortable |
| macOS data | Application Support/Regram Desktop |
| Windows data | AppData/Roaming/Regram Desktop |
| Linux data | System application data location for RegramDesktop |

No Telegram profile or legacy Telegram portable folder is imported automatically. Automatic registration of Telegram links is skipped by default. Protocol compatibility with `tg` and `tonsite` remains available through explicit registration.

Upstream automatic updates and crash submission are disabled. Regram store distribution and its own update infrastructure are not configured. The macOS bundle uses the Regram iOS Icon Composer document. In-app logos and color variants use the Regram source image; QR marks use Regram vector artwork. Tray icons retain the original Telegram artwork on all platforms and are independent of the application icon preference. Platform installers are built and verified separately before release.

API credentials and signing material are local build inputs. Runtime account data belongs only in Regram's independent data location or disposable verification folders.

Product titles, menus, notifications, settings, previews and new export names use Regram. Source-project promotion and donation links are excluded from the product interface. Language packs containing legacy product branding fall back to the bundled text. Existing backup formats, preference keys and source namespaces remain compatible. Existing service credentials migrate by their configured credential reference to the Regram keychain identity. Source attribution remains in license notices and reference documentation.

The macOS deployment target is 12.0. Regram keeps the Telegram Desktop Qt interface. Custom Liquid Glass materials and the independent AppKit preview have been removed.
