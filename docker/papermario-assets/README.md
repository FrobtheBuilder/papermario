# Paper Mario Asset Pipeline (Docker)

This container runs the `papermario-decomp` setup required to extract assets from a baserom on Linux tooling from Windows.
It clones upstream `papermario` inside the container so host line endings do not affect the build scripts.

## Windows usage

From repo root:

```powershell
.\scripts\Build-AssetsDocker.ps1 -RomPath C:\path\to\papermario.us.z64 -Version us
```

To also build the ROM after configure:

```powershell
.\scripts\Build-AssetsDocker.ps1 -RomPath C:\path\to\papermario.us.z64 -Version us -BuildRom
```

## Linux/macOS usage

```bash
./scripts/build-assets-docker.sh /path/to/papermario.us.z64 us
```

With ROM build:

```bash
./scripts/build-assets-docker.sh /path/to/papermario.us.z64 us --build-rom
```

## Environment details

- Base image: Ubuntu 24.04
- Installs system packages used by `install_deps.sh`
- Installs Rust and required tools: `pigment64`, `crunch64-cli`
- Clones upstream `papermario` repo inside container
- Clones `tools/splat` on-demand if missing
- Runs `./install_compilers.sh` if toolchains are absent
- Syncs generated output back to host `papermario-decomp` (`assets/<version>`, `ver/<version>/build`, `ver/<version>/baserom.z64`)
