# Raytraced Cel-Look

This repository is the source code used for my research thesis at the Hachioji Campus of the Tokyo University Of Technology.
You will find almost everything I've done from the day I've started working on it to this day.

## Table Of Contents

- [Raytraced Cel-Look](#raytraced-cel-look)
	- [Table Of Contents](#table-of-contents)
	- [Abstract/Introduction](#abstract-/-introduction)
	- [Features & Roadmap](#features-&-roadmap)
	- [Repository Structure & Explanation](#repository-structure-&-explanation)
	- [Building](#building)
		- [Windows](#windows)
		- [MacOS](#macoS)
		- [Linux](#linux)
	- [Running](#running)
	- [Thoughts](#toughts)
	- [Reference & Resources](#reference-&-resources)
	- [License](#license)

 ## Abstract/Introduction

 Hereafter is the formal description of my research thesis (I have yet to publish it anywhere so it cannot be called an abstract but it is the idea).

 > Year after year the number of game titles integrating real-time raytracing as a rendering engine increases, with last year's "Indiana Jones and the Great Circle" being the first game to have raytracing as the only rendering engine.
 However, in this landscape, most games use real-time raytracing to aim for realistic rendering, while a lot of games prefer sytlized rendering, and animated movies and their rendering engines such as Pixar's RenderMan or Dreamworks' MoonRay,  have developed new stylized rendering techniques with unique artistic expression.
 This research as for purpose an attempt at the implementation of a real-time stylized raytracing renderer for games or other real-time applications, and the study of the merits and demerits of raytracing for stylized rendering, using Cel-Look as a study case.

 So this is the convoluted way of saying, that I wanted to learn real-time raytracing, and cel-shading, and I was intrigued to mix both.
 
 As I said above, and discovered while doing my research, stylized rendering in raytracing is not new, and is already done by most rendering engine of animated studios, such as Pixar's RenderMan, Dreamworks' MoonRay, and Adobe's Arnold.
 But in this instance, the research focuses on an anime-like cel-look, which is not explicitly done by any of these engines (though toon shading exists, and I think it could be replicated easily).

 The main use-case in mind is for a game, or virtual avatar live concert, as it may be a way to improve graphics quality or find new artistic expression.
 As such, real-time performance is most important and actively considered in this study.

## Features & Roadmap

For real-time performance, the project is aiming to be a hybrid rasterizer using Vulkan.
But, it is also a sandbox where I learnt the tools and the theory behind my subject.
You will find test scenes and features that are irrelevant to my subject.
Here is a list of all the scenes : 

- Rasterized triangle (to test basic Graphics API features)
- Rasterized gltf object (to test depth buffer, Input Layout and other features needed for model drawing)
- CPU Raytracer based on Peter Shirley's [Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html) (to understand the basic principles of Raytracing)
- GPU Raytracing based on Peter Shirley's [Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html) (using the Vulkan Raytracing Extension)
- Deffered renderer with basic Cel-Shading (to test deffered rendering in Vulkan)
- Hybrid rasterizer for real-time raytracing performance

Arguably, only the last two have something to do with my research.

Here is a list of available features :

- [x] Cross Platform window management (using glfw)
- [x] Basic scene manager
- [x] Detachable and movable UI window with control over parameters of each scene
- [x] Unified camera settings and free camera control in all 3D scenes

To compare rasterized cel-shading, and raytraced cel-shading, we would need to:

- [ ] implement a proper cel-shader (a basic version of the popular [NiloToonURP](https://github.com/ColinLeung-NiloCat/UnityURPToonLitShaderExample))
- [ ] implement a proper hybrid raytracer with all its advantages
- [ ] study the "cel-look", and integrate this look into the hybrid raytracer
- [ ] compare the styles

This is the basic roadmap.

 ## Repository Structure & Explanation

 The repository is structured as such:

- Root
	- [deps](deps/) : the folder including all dependencies of this project
		- [bin](deps/bin/) : the folder including depedencies' dynamic libraries
		- [include](deps/include/) : the folder including depedencies' include
		- [libs](deps/libs/) : the folder including depedencies' static libraries
		- [src](deps/src/) : the folder including depedencies' sources that needs to be compiled
	- [include](include/) : the folder for all include files of this project
	- [media](media/) : the media used for the project (such as models and textures)
	- [src](src/) : the folder for all source files of this project

All dependencies and resources are in the [deps](deps/) and [media](media/) folders, with things I have not done and do not own with each having their own license.
Files I have made for this project are in the [src](src/) and [include](include/) folders, and are subject to this repository's license.

Here is a list of the dependencies and resources used by this project
- deps
	- [GLFW](https://www.glfw.org/) for multiplatform window interfacing
	- [Dear ImGUI](https://github.com/ocornut/imgui) for simple UI
	- [Rapid Json](https://rapidjson.org/) for json parsing used in gltf loading
	- [stb_image](https://github.com/nothings/stb/tree/master) for jpeg texture loading
	- [tiny_gltf](https://github.com/syoyo/tinygltf) for gltf loading
	- [tiny_obj](https://github.com/tinyobjloader/tinyobjloader) for obj loading
- media
	- [the COLLADA Duck](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Duck) licensed by Sony under [SCEA Shared Source License, Version 1.0](https://spdx.org/licenses/SCEA.html)
	- [the Utah Teapot](https://graphics.cs.utah.edu/teapot/) (supposedly unlicensed)

 As performance is a priority, we need to use a graphics API that can use accelerated raytracing cores, that leaves us with :
  - DirectX 12 (and DirectX Raytracing)
  - Vulkan (and the Vulkan Raytracing extension)
  - Metal

In the interest of multi-platform compatibility, I chose Vulkan, and thus use the Vulkan SDK.
I took great inspiration of [GPSnoopy's Raytracing in Vulkan](https://github.com/GPSnoopy/RayTracingInVulkan) repository, particularly for glsl shader's raytracing syntax.
You may find all [references](#reference-&-resources) in a section lower, but this one being very prominent, I prefered to cite it here.


## Building
		
As the project was developed on Vulkan, it is supposed to compile and run on most platform, however I did not test on every platform.
Also as this projects uses Vulkan, you need to install the [Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools) to make it work.

The project uses CMake to build, to make it easier to compile on most platforms, and it should work on most.
However, it was only compiled using clang as a compiler, so be careful to use clang as a C/CXX compiler.

### Windows

While I developed mostly on Windows, I did not really used clang tools to build the project, but mostly Visual Studio 2022's CMake integration to compile as it was easier (thus the CMake Presets.json in the root folder of the repository).

If you wish to use Visual Studio as a generator and and msvc as a compiler, you may by simply calling :

```bash
cmake.exe -G "[Your Desired Visual Studio Version]" -B out/ -S ."
```
I have tested it and it works.

If you want to compile with Ninja and clang like I have done, I recommend the CMake integration and clang compiler of Visual Studio, as everything will work just by opening visual studio at the root of the repository.

However, if you want to do it with your own installed tools, you may try, but I was not successful in making it work.
The visual studio extension uses Ninja to generate the output directory and CMake Cache.
The generation command I get is close to:

```bash
cmake.exe -G "Ninja" -B out/ -S . -DCMAKE_C_COMPILER="cl.exe" -DCMAKE_CXX_COMPILER="cl.exe"
```

But you should not really rely on this command, as visual studio adds a lot to the environment to make the command work.
Do not forget to add VK_SDK_PATH in your environment variable as the installed directory of your [Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools) .

After the project is generated you can use :

```bash
cmake.exe --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

or 

```bash
cmake.exe --build out/ -DCMAKE_BUILD_TYPE="Release"
```

To build it in either Debug or Release config.

But I highly advise to just use Visual Studio's Cmake and clang Integration to compile as the project was not made with command line build in mind.

### MacOS

Mac build is actually quite straightforward.

You need to install CMake (I used Homebrew for this), but as Apple uses AppleClang as the default compile, it is not difficult to make it work.

if you want to use the default MakeFile process just use : 

```bash
cmake -B out/ -S ."
```

but I prefer to use XCode to build the project (for this you need to have XCode installed): 

```bash
cmake -G="XCode" -B out/ -S ."
```

I find XCode more comfortable for the debugging features that it offers : mostly the gdb and the Metal Frame Debugger ; so you may not need it.

Then like for on Windows : 

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

or 

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Release"
```

to build from command line, or use XCode "Run" if you used XCode as a generator.

### Linux

Linux is the only OS I did not test, but I have hope it should not be that different from compiling on Mac.

If you are to compile on a Linux distribution, I think Makefile as a generator will work so as for Mac : 

```bash
cmake -B out/ -S ."
```
Should be enough, but there is two things to keep in mind :
 - You need VulkanSDK
 - you need to compile with clang 
 - you may need to add missing dynamic and static libraries

I am pretty certain that the project will not compile on gcc, so it would be better use clang if possible.

Build after generation, as per usual is :

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

or 

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Release"
```

Depending on the desired configuration.

## Running

For every platform the one thing that needs to be taken into account is where is the media folder from the produced executable as path is based on the windows config for now.

Depending on your CMake building method, the executable will be produced somewhere else.
the CMakeList copies the dynamic libraries needed next to the executable, so it can be run by just calling

```bash
./[path/to/excutable]/RaytracedCel[.exe]
```

## Thoughts

Here are thoughts I had while doing the project, related or unrelated to the research.

## Reference & Resources

Research Papers :
	- [Toon Shading Using Distributed Raytracing](https://www.cs.rpi.edu/~cutler/classes/advancedgraphics/S17/final_projects/amy_toshi.pdf) ; pretty much what I aim to do, though it is more straightforward toon shading than Cel-Look.
	- [“Non-photorealistic ray tracing with paint and toon shading” by Moon, Reddy and Tychonievich](https://history.siggraph.org/learning/non-photorealistic-ray-tracing-with-paint-and-toon-shading-by-moon-reddy-and-tychonievich/) ; an other instance of talented people already doing what I thought, but still not the Cel-Look I thought.
	- [Global Illumination-Aware Stylised Shading](https://diglib.eg.org/bitstream/handle/10.1111/cgf14397/v40i7pp011-020.pdf) ; talented people with an incredibly potent raytracer doing stylized shading ; this in real-time would be awesome.
	- [Hybrid Rendering For Real Time Raytracing](https://link.springer.com/chapter/10.1007/978-1-4842-4427-2_25) ; the inspiration to try to attain reasonable performance.
	- [Ray-Tracing NPR-style Feature Lines](https://www.sci.utah.edu/~roni/for-roni/NPR-lines/NPR-lines.NPAR09.pdf) ; cel-look usually needs outline. with an hybrid raytracer it seems useless to do it through the raytracer, but still can be useful.

Resources to learn and implement raytracing :
	- [Difference between Path-Tracing and Raytracing](https://www.techspot.com/article/2485-path-tracing-vs-ray-tracing/)
	- [Importance Sampling For Global Illumination](https://diglib.eg.org/server/api/core/bitstreams/0205c190-16aa-4f85-a26f-c7b3220683b9/content)
	- [ReSTIR Method Paper](https://cdn.pharr.org/ReSTIR.pdf)
	- [ReSTIR GI Method Paper](https://d1qx31qr3h6wln.cloudfront.net/publications/ReSTIR%20GI.pdf)
	- [GPSnoopy's Raytracing in Vulkan](https://github.com/GPSnoopy/RayTracingInVulkan) ; cannot be more grateful for this repository to have all the answers I needed in an understandable and readable codebase.
	- [Raytracing Denoiser](https://alain.xyz/blog/ray-tracing-denoising)
	- [GPU PseudoRNG](https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/)
	- [Sascha Willems's Vulkan Samples](https://github.com/SaschaWillems/Vulkan/tree/master)
	- [The Vulkan Raytracing Tutorial](https://github.com/alelenv/vk_raytracing_tutorial) ; still helpful while I would rather use the Khronos extensions than the NVidia one for compatibility.
	- [the all so helful vulkan raytracing extension text note](https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_ray_tracing.txt) ; for glsl raytracing extension's syntax.
	
Resources on stylized shading or cel-look : 
	- [Locally Controllable Stylised Shading](https://www-ui.is.s.u-tokyo.ac.jp/~takeo/papers/todo_siggraph2007_shading.pdf)
	- [cel-look pipeline and challenges to take advantage of hand drawn animation](https://a-film-production-technique-seminar.com/fppat/materials/ppi_phones_possibility_celook_pipeline_challenges/index.html)

Example of what I am looking for :
	- Difference between the [final cut of Kyoto Ani](https://sakugabooru.com/post/show/259302) and the [Genga](https://sakugabooru.com/post/show/259594) in the latest "Hibike Euphonium" season.


Example of use of raytracing in cel-look titles
	- [The Idolmaster: Starlit Season Ray Tracing ON vs OFF 4K RTX 3090](https://youtu.be/Xzn98J44VTU) ; only reflections on the ground...
	- [Real Toon Shader in DXR | Unity HDRP](https://www.youtube.com/watch?v=7_VEjFL8O1w) ; proof that toon shader is transferable (and transfered), into real-time raytracing engines.

## License

Everything that is in the [src](src/) and [include](include/) folder are made by me which is licensed under the MIT License, see [LICENSE](LICENSE) for more information.

For everything else, I do not own, and each resource is under their own license.
You may find more in the [Repository Structure & Explanation](#repository-structure-&-explanation) chapter.