# AGENTS.md

Guidance for AI coding agents working in this repository.

## What this is

ESP-IDF (ESP32-S3) firmware driving a 128x64 RGB LED matrix built from
**DP3364S** column driver chips (PWM/SRAM) and TC7262 binary row decoders.
These panels look like HUB75 but are not: the DP3364S holds the frame in its
own SRAM and needs a config-register write sequence plus a specific command
handshake before it displays anything, and the chip is barely documented
(the only datasheet is linked in a comment in `components/dp3364s/dp3364s.c`
next to `REGS`).

Layering: `dp3364s` drives the panel hardware, `gfx` is a display-agnostic 2D
graphics library, `lua_gfx` bridges a sandboxed Lua interpreter to `gfx`, and
`main` wires it together and owns the render loop. Pictures are meant to be
decided by Lua scripts under `assets/`, loaded from a LittleFS partition.

## Build / flash / monitor

```
idf.py set-target esp32s3   # only needed once / when switching targets
idf.py build flash monitor
```

- Run `idf.py` from PowerShell or `cmd.exe`, not Git Bash/MSYS — ESP-IDF's
  export script refuses to run there.
- `assets/*.lua` are packed into a dedicated `assets` LittleFS partition (see
  `partitions.csv`) at build time via `littlefs_create_partition_image` in
  `main/CMakeLists.txt`. Editing a script requires a rebuild + reflash.
- No test suite, linter, or CI. Verifying a change means building/flashing
  and observing the panel, or checking `Logic Capture.sal` (a Saleae capture
  from protocol reverse-engineering) for signal-level questions.

## Architecture

### Panel driver (`components/dp3364s`)

Signal generation is entirely in hardware after startup. The LCD_CAM
peripheral runs as a 16-bit parallel i8080 port: its PCLK pin drives the
panel clock (an exact divider of 160 MHz, set by `LCD_CLK_DIV`), and the
other 13 bus bits are GPIO-routed panel signals (R1/G1/B1/R2/G2/B2, row
address A-E, LAT, OE — see `SBIT_*`/`BUS_PINS`). GDMA feeds the peripheral
from a descriptor chain that loops back on itself, so the CPU never touches
a pin again once started.

The descriptor chain has two phases: an init phase writes every chip
register once (`build_register_init`), then falls through into a display
phase that loops one frame of pixel data forever (`build_display_frame`).
Callers redraw into a back buffer and call `dp3364s_update()` to flip which
chain the loop points at next (double-buffered, tear-free).

Public surface is pixel-level only: `dp3364s_set_pixel()`/`dp3364s_clear()`
write a plain (x, y) framebuffer (gamma-corrected, 13-bit PWM duty,
`GAMMA` = 2.2); `dp3364s_update()` encodes and presents it. Scan order, chip
chaining, and bit serialization stay internal to the driver.

One protocol exception: the SDR command (enter single-edge mode) samples
both clock edges, so it's bit-banged directly on the GPIOs in `send_sdr()`
*before* `lcd_init()` hands the pins to the peripheral.

Panel geometry constants (`NUM_CHANNELS`, `NUM_ROWS`, `CHIPS_PER_CHAIN`,
`TOTAL_COLS`) and the wiring table (`PIN_*`) must match the physical
panel/wiring; GPIOs 1-14 avoid flash/PSRAM pins (26-37), USB (19-20), and
strapping pins (0, 45, 46) on the S3.

### Graphics (`components/gfx`)

Two layers, both display-agnostic — wired to a concrete backend only via a
`set_pixel` function pointer passed in by the caller, never linked to
`dp3364s` directly.

- `gfx_prim`: immediate-mode `gfx_draw_point`/`gfx_draw_line`/`gfx_draw_rect`
  against a `gfx_prim_context_t*` (size + `set_pixel`).
- `gfx_elem`: a retained-mode element tree on top (`gfx_elem_t` nodes with
  init/render callbacks, appearance/alignment, `preferred_size`, a computed
  `computed_bounds` rect, and parent/children/next_sibling links). There is
  no layout/arrange pass — `computed_bounds` must be set manually after
  `gfx_elem_create()` if it should differ from `preferred_size` at (0, 0).
  Concrete widgets live under `widgets/`.

### Lua scripting (`components/lua_gfx` + `main`)

`main.c` opens a locked-down Lua state — only `base`/`math`/`string`/`table`
(no `io`/`os`/`package`/`debug`), since scripts are meant to eventually
arrive from a web upload path rather than be trusted local files.
`lua_gfx_open()` registers `WIDTH`/`HEIGHT` as globals and a `gfx` table
(`draw_point`/`draw_line`/`draw_rect`) bound to a `gfx_elem_context_t*` via a
lightuserdata upvalue.

`call_lua(L, fn)` looks up a global by name and calls it if present,
silently doing nothing otherwise — scripts aren't required to implement any
particular callback (`setup`/`render`/`update` are conventions, not an
enforced interface).

## Gotchas

- Changing `LCD_CLK_DIV` trades panel clock speed for frame rate/flicker
  margin.
- `gdma_strategy_config_t` must keep `owner_check = false` and
  `auto_update_desc = false`, or the self-looping DMA chain stalls after one
  lap.
- `build_display_frame()` is called twice at startup (once to settle
  carried-over signal state, once for the real first frame) — intentional.
- The `lua` managed component builds with `LUA_32BITS=1` (32-bit
  `lua_Integer`/`lua_Number`) scoped `PRIVATE` to itself.
  `components/lua_gfx/CMakeLists.txt` re-applies it `PUBLIC` so every
  consumer of the Lua C API agrees with the prebuilt library's ABI; any new
  component that touches `lua.h` directly needs the same define, or
  `lua_pushinteger`/`luaL_checkinteger` silently read/write wrong values.
