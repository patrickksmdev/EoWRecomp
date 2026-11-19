# EoWRecomp

<p align="center">
  <img src="docs/icon/icon_1024.png" width="128" alt="EoWRecomp icon">
</p>

<h1 align="center">EoWRecomp</h1>

<p align="center">
  <b>Static recompilation of <i>The Legend of Zelda: Echoes of Wisdom</i> (Nintendo Switch, 2024)</b><br>
  <b>Current development focus: Linux x86-64 + Vulkan</b>
</p>

<p align="center">
  <img alt="status" src="https://img.shields.io/badge/status-v0.3.0-orange">
  <img alt="platform" src="https://img.shields.io/badge/target-Linux%20x86--64-blue">
  <img alt="graphics" src="https://img.shields.io/badge/graphics-Vulkan-red">
  <img alt="license" src="https://img.shields.io/badge/license-MIT-green">
</p>

---

EoWRecomp translates the game's ARM64 machine code into C ahead of time and recompiles it for the host PC. Its runtime replaces the Switch operating-system services used by the game and implements the NVN graphics API on top of Vulkan. This fork is built from [requiemjota88/EoWRecomp](https://github.com/requiemjota88/EoWRecomp) (`beta 1`) with backported improvements from [tcsenpai/EoWRecomp](https://github.com/tcsenpai/EoWRecomp) and [LAS Recomp](https://github.com/peiikai/LASRecomp), plus Linux-specific fixes.

> [!IMPORTANT]
> **Linux x86-64 (Vulkan) is the active tested target in this fork.** Windows, macOS, and Android builds inherited from upstream are not tested here.

> [!WARNING]
> **No game files are included.** You must provide your own legally obtained copy of *The Legend of Zelda: Echoes of Wisdom* v1.0.0 (title ID `01008CF01BAAC000`) and your own `prod.keys`. The repository does not provide game files, keys, or instructions for obtaining them.

## Linux progress (`v0.3.0`)

- **Tested playable through the 2nd dungeon (Gerudo Desert):** intro, Hyrule Castle Town, Suthorn Village, Still World (Rifts), 2D sections, first dungeon, and Gerudo Desert dungeon with audio, working save/load, and zero freezes.
- **Placed-memory guest heap (`VK_EXT_map_memory_placed`) & CPU draw optimizations:** maps host-cached GTT memory directly at `HEAP_BASE` (eliminating per-submit `amdgpu` userptr overhead on Linux RADV, `~50x` faster `vkQueueSubmit2`), plus cached `stencil_slots()` masks and gated debug checks in `draw_()` (**~66 FPS → 91–116 FPS** in heavy open areas on RX 7600).
- **Rift / Void rendering fix:** submission-tracked GPU-written buffer lifetimes (`gw_tab`, ported from `tcsenpai/EoWRecomp`) fix visual glitches after exiting Rift areas without address-specific workarounds.
- **Atomic kernel arbiter & 2 s watchdog:** replaced non-atomic load+store arbiter updates with atomic CAS in `runtime/kernel.c` and raised the Linux watchdog threshold to 2 s with thread dumps before `kernel_unstick()`.
- **GPU-side indirect draws (`vkCmdDrawIndirect` / `vkCmdDrawIndexedIndirect`):** eliminated mid-frame CPU flush stalls and added live tracking for GPU-written constant buffers.
- **Smart shader compilation & pre-warming:** pre-warms shaders on `nvnProgramSetShaders` during loading and waits synchronously only on critical fullscreen/compute passes (`Smart` / `Sync` / `Async` selectable in the `F1` menu).
- **VSync & FPS limit:** toggle VSync and cap FPS (`30` / `60` / `120` / `Unlimited`) from the `F1` → **Display** menu.
- **XCI extraction & AppImage:** HFS0 `.xci` support in `tools/nca_extract.py` and standalone x86-64 AppImages (`build/dist/EoWRecomp-v0.3.0-linux-x86_64.AppImage`).

### Known issues
- **Map screen >120 FPS:** keep `FPS limit` at `60` or `120` (running at `144+` fps currently inverts the analog stick on the map screen).
- **Untested platforms:** Windows, macOS, and Android builds are inherited from upstream and untested in this fork (automated Windows x64 builds are generated via GitHub Actions).

## Running & Building on Linux

### AppImage (recommended)
Download the latest AppImage from [Releases](https://github.com/patrickksmdev/EoWRecomp/releases):
```bash
chmod +x EoWRecomp-v0.3.0-linux-x86_64.AppImage
./EoWRecomp-v0.3.0-linux-x86_64.AppImage
```
On first launch, select the folder containing your `.nsp`/`.xci` and `prod.keys`. The launcher will extract ExeFS and compile `eowgame.so` locally.

### Building from source
See [docs/linux.md](docs/linux.md) and [docs/STATUS.md](docs/STATUS.md) for dependencies, or build the AppImage with `podman`:
```bash
tools/linux/appimage.sh x86_64
```

## How it works

- **Recompiler** (`tools/`): analyzes the game's ARM64 modules, resolves functions and jump tables, and generates C code.
- **Runtime** (`runtime/`): implements the Horizon services used by the game, including threads, synchronization, filesystem, input and audio.
- **Graphics**: reimplements NVN on Vulkan and translates Maxwell GPU shaders to GLSL.
- **Game package reader** (`runtime/nxpkg.c`): reads NSP/XCI/NCA packages using the user's keys. The RomFS can be read directly from the package instead of requiring a second multi-gigabyte copy.

## Release status: handing the project over

This is **beta 1**: a starting point for developers, not a finished game. The game boots, renders most of the world and is playable on the
test PC, with the open issues listed above. Everything needed to continue is in the repository:

- `docs/STATUS.md` is the detailed engineering log (in Portuguese): what works, what broke, hypotheses ruled out, and next steps.
- `CLAUDE.md` holds the project rules (no Nintendo files in the repo, never edit `generated/` by hand, every patch checks the original
  instruction word, a fix counts only once seen working).
- Tests live in `tools/` (`test_translate`, `test_shaders`, `test_simd`, `test_lift`, `test_lane.sh`, `test_stencil.sh`).
- **F9 debug capture:** press F9 in game to save the next frame's render passes (`build/pass_*.png`), the draw log, and, with
  `EOWRECOMP_SAVE_MSL=1`, the translated GLSL in `build/shaders`. This is how the rift and vegetation bugs were found.
- Debug switches all start with `EOWRECOMP_` (search `getenv("EOWRECOMP_` in `runtime/`).

Suggested next steps for contributors: finish the rift fill, move indirect draw arguments to the GPU, find the freeze's root cause, then
validate the Metal backend.

## Development status

See **[docs/STATUS.md](docs/STATUS.md)** for the detailed technical log:
- Windows test results and performance observations
- Vulkan/Radeon shader and pipeline investigations
- SUST and compute-shader progress
- Remaining graphics bugs and diagnostic instructions

## Credits

- **[EoWRecomp original project](https://github.com/Nycolazs/EoWRecomp)** by **Nycolazs** — original project and foundation.
- **[requiemjota88/EoWRecomp](https://github.com/requiemjota88/EoWRecomp)** — `beta 1` base used for this fork (GPU culling, stencil views, watchdog).
- **[tcsenpai/EoWRecomp](https://github.com/tcsenpai/EoWRecomp)** — backported `gen.py` jump-table, frame-pacing, pipeline cache, and storing-fs fixes.
- **[LAS Recomp](https://github.com/peiikai/LASRecomp)** by Peiikai and contributors — the foundation for the recompiler, runtime, NVN implementation, Maxwell shader translator and Vulkan renderer.
- [SDL 3](https://libsdl.org), [FFmpeg](https://ffmpeg.org), [Dear ImGui](https://github.com/ocornut/imgui), [Vulkan](https://www.vulkan.org), [volk](https://github.com/zeux/volk), [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator), [shaderc](https://github.com/google/shaderc), [SMAA](https://github.com/iryoku/smaa) and [AMD FidelityFX FSR 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR).
- Documentation and research references: [Switchbrew](https://switchbrew.org), [envytools](https://github.com/envytools/envytools), and the instruction decoders of Ryujinx and yuzu.

This project is developed with AI assistance from [Claude](https://www.anthropic.com/claude) (Anthropic), under human direction and testing. Commits made with Claude carry a `Co-Authored-By` trailer.

## Legal

EoWRecomp is an independent fan project and is not affiliated with, endorsed by, or sponsored by Nintendo. *The Legend of Zelda* and *Echoes of Wisdom* are trademarks of Nintendo. No game files, game code, assets or keys are included. You must provide your own legally obtained copy of the game.

## License

EoWRecomp's original code is licensed under MIT. Third-party components retain their respective licenses. This license grants no rights to Nintendo's game or other copyrighted content.
