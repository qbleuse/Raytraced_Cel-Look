# Raytraced Cel-Look

This repository is the source code used for my research thesis at the Hachioji Campus of the Tokyo University Of Technology.
You will find almost everything I've done from the day I've started working on it to this day.

## Table Of Contents

- [Raytraced Cel-Look](#raytraced-cel-look)
	- [Table Of Contents](#table-of-contents)
	- [Abstract/Introduction](#abstract-/-introduction)
	- [Repository Structure & Explanation](#repository-structure-&-explanation)
	- [Features & Roadmap](#features-&-roadmap)
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

 > Though having a weak start, with difficulty to attain accepatable framerate, year after year the number of game titles integrating
 real-time raytracing is increasing with last year's "Indiana Jones and the Great Circle" being the first game to have raytracing as the only rendering engine.
 However, in this landscape, most games use real-time raytracing to aim for realistic rendering, while a lot of games prefer sytlized rendering,
 and animated movies and their rendering engines such as Pixar's RenderMan or Dreamworks MoonRay,  have developed new stylized rendering techniques with unique artistic expression.
 This research as for purpose an attempt at the implementation of a real-time stylized raytracing renderer for games or other real-time applications,
 and the study of the merits and demerits of raytracing for stylized rendering, using Cel-Look as a study case.


 So this is the pompous way of saying, that I wanted to learn to do real-time raytracing, and cel-shading, and I was intrigued to mix both.
 As I said above, stylized rendering in raytracing is not new, and is already done by most rendering engine of animated studios, such as Pixar's RenderMan, Dreamworks' MoonRay, and Adobe's Arnold.
 But in this instance, the research focuses on an anime-like cel-look, which is not explicitly done by any of these engines (though toon shading exists, and I think it could be replicated easily).

 The main use-case in mind is for a game, or virtual avatar live concert, as it may be a way to improve graphics quality or find new artistic expression.
 As such, framerate is most important, and performance is actively considered in this study.

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

All dependencies and resources are in [deps](deps/) and [media](media/) folder, with thing I have not done and do not own with each having their own license
Files I have made for this project are in  [src](src/) and [include](include/) folder, and are subject to this repository's license.

Here is a list of the dependencies and resources used by this project
- deps
	- [GLFW](https://www.glfw.org/) for multiplatform window interfacing
	- [Dear ImGUI](https://github.com/ocornut/imgui) for simple UI
	- [Rapid Json](https://rapidjson.org/) for json parser for gltf loading
	- [stb_image](https://github.com/nothings/stb/tree/master) for jpeg texture loading
	- [tiny_gltf](https://github.com/syoyo/tinygltf) for gltf loading
	- [tiny_obj](https://github.com/tinyobjloader/tinyobjloader) for obj loading
- media
	- [the COLLADA Duck](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Duck) licensed by Sony under [SCEA Shared Source License, Version 1.0](https://spdx.org/licenses/SCEA.html)
	- [the Utah Teapot](https://graphics.cs.utah.edu/teapot/) (supposedly unlicensed)


 As the performance is a priority, a graphics API that can use accelerated raytracing cores, that leaves us with :
  - DirectX XII (and DirectX Raytracing)
  - Vulkan (and teh Vulkan Raytracing extension)
  - Metal

In the interest of multi-platform compatibility, I chose Vulkan, and thus use the Vulkan SDK.
I took great inspiration for [GPSnoopy's Raytracing in Vulkan](https://github.com/GPSnoopy/RayTracingInVulkan) repository, particularly for glsl shader's raytracing syntax.
You may find all [references](#reference-&-resources) in a section lower, but this one being very proeminent, I prefered to cite it here.

## Features & Roadmap

For real-time performance, the project is aiming to be a hybrid renderer using Vulkan.
But before this, it is also the project where I learnt the tools and the theory behind my subject.
You will find test scenes and features that are irrelevant to my subject.
You can look at it if you are interested.

Here is a list of available features and of features that are yet to be done, expressed by checkboxes.
checkboxes marked are features that are available.
Those that are not are for features that need or that would be nice to have.

- [x] Cross Platform window management (using glfw)
- [x] Detachable and movable UI window
- [x] Basic Scene Manager
- [x] Unified Camera settings and controls in all scenes
	- [x] A scene with a rasterized triangle (to test basic Graphics API features)
		- [x] movable triangle vertices by clicking or through UI (to test uniform buffers)
		- [x] basic vertices color editing through the scene's UI (to test uniform buffers)
	- [x] A scene with a rasterized gltf object (to test depth buffer, Input Layout and other features needed for model drawing)
		- [x] The possibility to move the object in the scene through the UI (to test uniform buffers and my own matrices)
		- [x] The possibility to move the camera in the scene (to test uniform buffers and my own matrices)
	- [x] a scene implementing a CPU Raytracer based on Peter Shirley's [Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html) (to understand the basic principles of Raytracing)
		- [x] a UI allowing to change ray rebound depth, nb of sample for depth, and other parameters
		- [x] an accelerated generation of the frame using a basic theadpool
		- [x] a "camera mode", using the same controls as any other scene that allows to move into the CPU raytraced scene by disabling multisampling and rebound
		- [ ] compute optimization and memory optimization for the scene (right now using over 2Go for a 1600x800 pixel image)
		- [ ] SIMD computation
	- [x] a scene implementing GPU Raytracing based on Peter Shirley's [Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html) (using the Vulkan Raytracing Extension)
		- [x] a UI allowing to change ray rebound depth, nb of sample for depth, and other parameters
		- [x] a movable object in the scene (Utah Teapot) that can be moved in the scene's UI
	- [x] a scene implementing a deffered renderer with basic Cel-Shading (to test deffered rendering in Vulkan)
		- [x] The possibility to move the object in the scene through the UI
		- [x] The possibility to move the camera in the scene 
		- [x] the possibility to see each framebuffer in the deffered rendering process for debug purposes
		- [x] a basic cel-shader based on smoothstep and blinnphong
		- [ ] a basic version of the popular [NiloToonURP](https://github.com/ColinLeung-NiloCat/UnityURPToonLitShaderExample) to get a better comparison of a real used toon shader
	- [x] a scene implementing a basic hybrid renderer for real-time raytracing performance
		- [x] The possibility to move the object in the scene through the UI
		- [x] The possibility to move the camera in the scene 
		- [x] a UI allowing to change ray rebound depth, nb of sample for depth, and other parameters
		- [x] basic raytracing based on Peter Shirley's [Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
		- [ ] Shadow rays and light integration in Raytracing
		- [ ] basic Cel-Shading in Raytracing through compositing
		
As you may have seen, the project is mostly a sandbox	for me to learn the research subjects for now, as there is a lot to learn.
But the aim is, I think, pretty clear from the available features and what is planned above.
The idea is :

Being able to compare graphical quality, artistic expression, and performance between rasterized cel-shading and the raytraced cel-shading I am developing, to balance out the merits and demerits.

For this a simple roadmap would be :
- [ ] implementing a proper cel-shader
- [ ] implementing a proper hybrid raytracer with all its advantage
- [ ] studying the "cel-look", and integrating this look into the hybrid raytracer
- [ ] comparing the styles

## Building
		
As the project was developed on Vulkan, it is supposed to compile and run on most platform, however I could not test on every platform.

The project uses CMake to build, to make it easier to compile on most platforms, and it should work on most.
However, it was only compiled using clang as a compiler, so be careful to use clang as a CXX compiler.

Let us see the steps for every platform.

### Windows

While I developed mostly on Windows, I did not really used clang tools to build the project, but mostly Visual Studio 2022's CMake integration to compile as It was easier (thus the CMake Presets.json in the root folder of the repository).

If you wish to use Visual Studio as a generator and and msvc as a compiler, you may by simply calling :

```bash
cmake.exe -G "[Your Desired Visual Studio Version]" -B out/ -S ."
```
I have tested it and it works.

If you want to commpile with Ninja and clang, I very much advise the CMake integration and clang compiler of Visual Studio, as everything will work just by opening visual studio at the root of the repository.

However, if you want to do it with your own installed tools, you may try, but I was not successful in making it work.
The visual studio extension uses Ninja to generate the output directory and CMake Cache.
The generation command I get is close to:

```bash
cmake.exe -G "Ninja" -B out/ -S . -DCMAKE_C_COMPILER="cl.exe" -DCMAKE_CXX_COMPILER="cl.exe"
```

But you should not really rely on this command, as visual studio adds a lot to the environment to make the command work.
Also as this projects uses Vulkan, you need to install the [Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools) and have the VK_SDK_PATH to the installed directory into your environment variable.

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
should be enough, but there is two things to keep in mind :
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

## Reference & Resources

## License

Everything that is in the [src](src/) and [include](include/) folder are made by me which is licensed under the MIT License, see [LICENSE](LICENSE) for more information.

For everything else, I do not own, and each resource is under their own license.
You may find more in the [Repository Structure & Explanation](#repository-structure-&-explanation) chapter.