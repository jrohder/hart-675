# AGENTS.md

## Cursor Cloud specific instructions

This is a **PlatformIO / ESP32 (Arduino framework) firmware** project for the Hart Communicator 675. There is no host-side server, database, or container — the product runs on the physical ESP32 after flashing. Standard build/run commands live in `README.md` ("Build & Upload (PlatformIO)").

### Toolchain / environment
- The update script installs PlatformIO via `pip install --user platformio`. The `pio` binary lives in `~/.local/bin`, which is added to `PATH` via `~/.bashrc` (interactive shells). For non-interactive contexts, invoke `~/.local/bin/pio` or export the path first.
- The first `pio run` downloads the `espressif32` platform + Xtensa toolchain (hundreds of MB, a few minutes). This is cached under `~/.platformio` afterward; the update script intentionally does **not** pre-build.

### Build / run
- Build: `pio run` (single env `huzzah32`). Success prints memory usage and creates `.pio/build/huzzah32/firmware.bin`.
- `pio run --target upload` and any real end-to-end HART/USB/TCP/WiFi behavior require the **physical Adafruit HUZZAH32 + AD5700 hardware**; the firmware cannot run inside this VM.
- There is **no unit-test or lint framework**; the compiler is the primary correctness check. (`pio check` is available for static analysis only if a checker like cppcheck is installed.)

### DD profiler (host-runnable core component)
- `tools/dd-profiler/ddparser.js` is the single source of truth for turning a HART DD/EDD `.zip` package into a Hart 675 profile JSON. It runs unchanged in Node, in the browser, and is mirrored verbatim into `src/DDParserJS.h`.
- Validate offline against a package: `node tools/dd-profiler/run.js <package.zip>` (requires the `unzip` CLI). Tier B = menus/variables extracted; Tier A = identity + generic pages.
- After editing `ddparser.js`, regenerate the embedded header with `python3 tools/dd-profiler/gen_header.py` (writes `src/DDParserJS.h`) and rebuild.

### Previewing the embedded web dashboard without hardware
- The dashboard SPA is embedded as a `PROGMEM` raw string in `src/WebPages.h` (between `R"HTML(` and `)HTML"`). The DD-import parse + review flow is **fully client-side** (custom ZIP reader + `DDParser`) and only calls the backend at the final save step.
- To preview it in a browser, serve the extracted HTML as `index.html` plus `tools/dd-profiler/ddparser.js` at `/ddparser.js`; `/api/*` calls fail gracefully (the SPA tolerates a missing device). The Profiles page → "Parse Package(s)" exercises the real parser engine.
