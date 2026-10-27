# Build standalone RunClock

Use Node.js 24, Python 3, Zig 0.14.1 and PS5 Payload SDK v0.43. Set ZIG and PS5_PAYLOAD_SDK to absolute toolchain paths, then run:

```sh
python3 scripts/build.py
```

The output is **dist/RunClock.elf**. The build compiles the C timing worker, embeds it with the RunClock icon, builds the native renderer/input modules and links the self-contained QuickJS coordinator ELF. There is no installed-menu dependency or external runtime service.

Exact static download/catalog inputs are under build-inputs. Their full corresponding source is in RunClock_Dependency_Source_v1.0.0-beta.1.zip. SQLite 3.46.1 is the reviewed rebuilt input (7,576,386 bytes, SHA-256 c40f492901fe1d4f86e632ae79e08bb48ea3a6e1e58dbc9935efbe80451f6fd4); its build flags retain THREADSAFE/WAL and omit loadable extensions. Other library pins remain enforced by the upstream preparation scripts. Original SDK source is also retained in app/third_party.

## Host verification

```sh
python3 scripts/test.py
```

The integration test drives the real native timing/storage worker through RunClock's own coordinator and authenticated loopback HTTP bridge. It verifies live clock updates and control validity independently, then writes real encoded frames for the C native-input test. It makes no claim to emulate ShellUI or physical firmware behavior.

The engine is adapted from GPL-licensed ProsperoMenu. Source files retain upstream copyright notices; standalone-specific entry, deck routing and clock-redraw validation are local changes. Build output directories, node_modules and private records must be excluded from source releases.
