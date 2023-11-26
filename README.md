# 🛩️ fxpo – Fast X-Plane Orthoimages

Blazingly _fast_ orthoimagery for X-Plane 11 and 12. 

<p align="center">

  <img src="https://github.com/bcsongor/fxpo/assets/8850110/31dd298d-82b0-4eec-917d-44dfac393e8e" alt="fxpo screenshot">
</p>



## Motivation

Open-source projects like [Ortho4XP](https://github.com/oscarpilote/Ortho4XP) and [AutoOrtho](https://github.com/kubilus1/autoortho) are great tools to bring ortho scenery for X-Plane.
Ortho4XP covers the entire scenery building process from assembling vector data to downloading satellite imagery while AutoOrtho builds on top of Ortho4XP providing a solution to stream orthoimages on-demand.

Both, as the last step of the ortho scenery building process, download satellite imagery chunks, assembling and compressing them to a texture format that X-Plane can render.

_fxpo_ optimises this step which—for sufficiently high zoom levels—is also the most time-consuming.

## Is _fxpo_ for me?

_fxpo_'s optimisations heavily rely on SIMD-capable multicore CPUs, NVIDIA GPUs with high CUDA core counts and a sufficiently high-speed internet connection with low latencies towards orthoimage servers.

| Tile              | _fxpo_ | Ortho4XP | Speedup |
|-------------------|--------|----------|---------|
| `+57-006`, `ZL17` | 2m 13s | 16m 43s  | 7.5x    |

_(Averages over 5 consecutive runs.)_

Running on Intel Core i9-13900KS, NVIDIA GetForce RTX 4090 and
300 Mbps download speeds with ~4 ms latency toward `virtualearth.net` servers, _fxpo_ shows a 7x performance improvement over Ortho4XP (commit [7ed9a09](https://github.com/oscarpilote/Ortho4XP/commit/7ed9a092541bc3bcee810c59f07d12f885f97f17); `max_convert_slots` set to 32) downloading tile `+57-006` at zoom level 17.

### Usage

Use Ortho4XP to assemble vector data, triangulate mesh, draw masks and build DSF (set `skip_downloads` to `True` in `Ortho4XP.cfg`) for the given tileset. Once complete, use _fxpo_ to download and build orthoimage textures for the tileset.

```text
Usage: fxpo "<scenery_path>" "<tileset>"

  <scenery_path> is the path to X-Plane's Custom Scenery folder.
    Example: C:\X-Plane 12\Custom Scenery

  <tileset> is the coordinates of the tileset to download and process.
    Example: +57-006
```

The above example expects the path `C:\X-Plane 12\Custom Scenery\zOrtho4XP_+57-006` to exist.

<p align="center">
  <img src="https://github.com/bcsongor/fxpo/assets/8850110/02481f26-46a0-4d21-9b20-d5fcec857fa4" alt="fxpo screenshot">
</p>

_fxpo_ requires Windows. The code-base is relatively platform independent so with some effort it should be possible to port it to Linux and macOS.

As of now no precompiled binaries are provided (community builds are more than welcome!). Please see the next section for instructions on how to build _fxpo_.

_fxpo_ is experimental and bugs are expected.

### Development

#### Requirements 

- CMake
- vcpkg
- Visual Studio 2022
- NVIDIA Texture Tools (NVTT) 3 SDK _(requires NVIDIA Developer account)_
- NVIDIA CUDA Toolkit 11.8

#### Build

1. Generate the project files with CMake
   ```shell
   cmake --preset windows-x64-release -B cmake-build-windows-x64-release
   ```
  
2. Build _fxpo_
   ```shell
   cmake --build cmake-build-windows-x64-release --config Release
   ```
