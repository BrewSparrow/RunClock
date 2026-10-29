# Third-party source and notices

- **StonedModder/Prospero-Menu**, revision 84dd26cd342a5d9b1df5a93942ca5d7f10c8d39e: GPL-3.0-or-later native ShellUI rendering, controller routing, firmware profiles, process validation, loader, runtime adapters and lifecycle foundation. Source and original notices are retained under engine/. The standalone application replaces the menu coordinator/entry and separates display redraws from action identity. Runtime paths/ports are private to RunClock.
- **ProsperoMenu C overlay SDK**: MIT, original notice retained in app/third_party/prospero-menu-sdk/LICENSE. The standalone worker retains the API framing internally and changes its default IPC root to its own coordinator. It is not a dependency on the separately installed menu application.
- **PS5 Payload SDK v0.43**: original GPL/BSD/MIT notices and corresponding source retained in app/third_party and the native dependency-source archive.
- **QuickJS and engine/vendor components**: retain their original licensing/notices in the complete engine source. The application uses QuickJS locally and does not install a JavaScript environment on another device.
- **curl 8.10.1, wolfSSL 5.7.2, libpsl 0.21.5, zlib 1.3.1, zstd 1.5.6, SQLite 3.46.1 and their build inputs**: full corresponding source and original archive identities/notices are in RunClock_Dependency_Source_v1.0.2-beta.3.zip. Some bindings are inherited from the engine; no downloader, FTP service, payload updater, cheat or package UI is wired into RunClock.
- Runtime npm package sources and integrity metadata are included in source/npm-sources. Original package-lock pins are retained. The compiler/build tools are not distributed as runtime dependencies.

The native dependency source content is unchanged from the previously supplied source bundle. Distribution of the standalone binary must include equivalent access to both its main source archive and that dependency-source archive. Do not remove original notices or present adapted upstream code as entirely new authorship.

Primary sources: https://github.com/StonedModder/Prospero-Menu and https://github.com/ps5-payload-dev/sdk .

## Resident SDK CRT adaptation

The resident renderer rebuilds SDK v0.43 CRT sources from the exact original archive in app/third_party. engine/scripts/prepare-resident-crt.py preserves the original GPL/copyright notices and replaces IOMMU discovery with the RunClock context callback. The original source plus preparation script and callback are included in the corresponding source archive. Other helpers and the coordinator retain the stock SDK CRT.

The resident CRT preparation also marks the original SDK __crt_syscall assembly bridge naked, preserving its stack ABI when rebuilt. The source patch, execution regression and compiled-bridge verifier accompany the original SDK source and notices.
