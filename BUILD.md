# Build standalone RunClock

Use Node.js 24, Python 3, Zig 0.14.1 and PS5 Payload SDK v0.43. Set ZIG and PS5_PAYLOAD_SDK to absolute toolchain paths, then run:

```sh
python3 scripts/build.py
```

The output is **dist/RunClock.elf**. The build compiles the C timing worker, embeds it with the RunClock icon, builds the native renderer/input modules and links the self-contained QuickJS coordinator ELF. There is no installed-menu dependency or external runtime service.

Exact static download/catalog inputs are under build-inputs. Their full corresponding source is in RunClock_Dependency_Source_v1.0.2-beta.3.zip. SQLite 3.46.1 is the reviewed rebuilt input (7,576,386 bytes, SHA-256 c40f492901fe1d4f86e632ae79e08bb48ea3a6e1e58dbc9935efbe80451f6fd4); its build flags retain THREADSAFE/WAL and omit loadable extensions. Other library pins remain enforced by the upstream preparation scripts. Original SDK source is also retained in app/third_party.

## Host verification

```sh
ZIG=/absolute/path/to/zig python3 scripts/test.py
```

The integration test drives the real native timing/storage worker through RunClock's own coordinator and authenticated loopback HTTP bridge. It verifies live clock updates and control validity independently, then writes real encoded frames for the C native-input test. It makes no claim to emulate ShellUI or physical firmware behavior.

The startup recovery test also drives the real state store, job queue, native adapters and service graph with substituted console probes. It covers boot/process expiry, undispatched intents, same-boot reinjection refusal and error messages. A C receipt-policy test checks which abandoned native intents are safe to retry.

The engine is adapted from GPL-licensed ProsperoMenu. Source files retain upstream copyright notices; standalone-specific entry, deck routing and clock-redraw validation are local changes. Build output directories, node_modules and private records must be excluded from source releases.

The native stability test also exercises held/skipped chord input, pending-scene readiness, unknown focus, raw game-route capture expiry and retention of previous launch logs. Console probes are substituted; these checks do not reproduce a PS5 freeze.

The resident-memory regression models SDK whole-entry protection and checks that sealing the code buffer preserves adjacent writable mappings. It also covers isolation/protection failures before publication. See BOOTSTRAP_FIX.md.

For test 3, engine/scripts/prepare-resident-crt.py verifies the bundled SDK v0.43 source archive and prepares the resident-only CRT. Its IOMMU discovery is replaced by a fresh, checked coordinator handoff. The standalone coordinator and other helpers still use the stock SDK CRT. No new source download or persistent kernel-address cache is used. See RESTART_FIX.md.

For test 4, the resident SDK bridge is explicitly naked. Building rejects a linked bridge with a C stack prologue or incorrect argument/gateway/return instructions. Host verification now needs ZIG set to Zig 0.14.1 so the real bridge is compiled with the resident target and executed through a mock gateway. No PS5 operating-system syscall is executed by that host fixture. See SYSCALL_FIX.md.

Test 5 links only RunClock render/input hooks in the resident, omitting dashboard/cheats hook modules. The main resident entry is non-returning on rejection, and the coordinator issues one preparation per launch. Protection diagnostics are shared through bootstrap ABI 15; the timer wire protocol is unchanged. See MINIMAL_STARTUP.md.

RunClock v1.0.2-beta.3 retains the test-6 instrumented startup profile, recorded as buildIdentity.bootstrapDiagnostic=true. It retains ptrace ownership through the first signal or an exact phase-23 checkpoint, disarms the checkpoint before detaching, and logs the capture after detach. It adds logging/libc setup stages and uses bootstrap ABI 16. The production observer lifecycle and actual x86 checkpoint are covered by host tests. Set PROSPEROMENU_BOOTSTRAP_TRACE=0 only for a separate normal build; that is not the supplied diagnostic artifact and has a different binary hash. See STARTUP_DIAGNOSTIC.md.

The beta-3 packaging records the five successful maintainer startup tests against the exact test-6 baseline hash. Version/display changes require a rebuilt timer worker and coordinator. The resident renderer/input executable and allocated data sections match that baseline. Only non-runtime debugger metadata containing the build directory differs; the other embedded native helpers are byte-identical. Do not mark the new ELF as physically verified solely because its baseline passed.
