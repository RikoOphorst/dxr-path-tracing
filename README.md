# DirectX Raytracing: A Path Tracer

A DXR path tracer with OptiX denoising. 5 months worth of research, trial & error as part of a project to learn and understand DirectX Raytracing & raytracing concepts.

- Progressive Monte Carlo pathtracing
- Native DirectX Raytracing
- DXR Fallback Layer
- OptiX deep-learning denoiser
- Anti-aliasing with various sampling patterns
- Reflection
- Refraction
- Material picking & editing
- ImGui

<div style="text-align: center;"><img src="https://i.imgur.com/nj25pfX.png" alt="Cornell Box Sample Image" width="320" height="180"><img src="https://i.imgur.com/n2DLDKY.png" alt="Cornell Box Sample Image" width="320" height="180"></div>

## Performance
**Specifications:**
- NVIDIA GeForce RTX 2080 Founders Edition (non-overclocked)
- Intel Core i6-6600K at 4.1 GHz
- 8 GB RAM at 2133 MHz
- 256 GB NVMe M.2 Samsung SSD
- **All tests ran at 720p (1280 x 720)**

### Test case: Cornell Box (Release mode)
| Bounces | Frame Time |
|:---:|:---:|
| 0 | ~3.6ms |
| 1 | ~4.0ms |
| 2 | ~4.3ms |
| 3 | ~4.9ms |
| 4 | ~5.6ms |
| 5 | ~6.3ms |
| 10 | ~10ms |
| 15 | ~12.9ms |

### Test case: Crytek Sponza (Release mode)
| Bounces | Frame Time |
|:---:|:---:|
| 0 | ~5.1ms |
| 1 | ~6.3ms |
| 2 | ~8.4ms |
| 3 | ~10.0ms |
| 4 | ~11.4ms |
| 5 | ~13.6ms |
| 10 | ~22.7ms |
| 15 | ~31.8ms |

## Building the project
1. Clone the project
2. [Download the project's dependencies from here!](http://dependencies.rikoophorst.com/dxr-path-tracing/dxr-path-tracing.zip)
3. Run CMake on the project.
4. Configure for Visual Studio 2017 x64.

**Prerequisites to compile & run**
- Must be on Windows 10 October 2018 update (RS5 | v1809) or newer
- Must have Windows 10 SDK version 10.0.17763.0 or newer (prefer using that exact version!)
- To run native DXR: NVIDIA Turing or Volta GPU or newer.
- To run fallback: Second generation Maxwell GPU or newer.
- Visual Studio 2017 v15.8.6+.
