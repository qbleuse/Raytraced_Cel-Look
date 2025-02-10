#ifndef __CORNELL_BOX__
#define __CORNELL_BOX__

#include "Utilities.h"
#include "Maths.h"

namespace CornellBox
{
	__forceinline void CreateMesh(const float scale, VolatileLoopArray<vec3>& pos, VolatileLoopArray<vec2>& uv, VolatileLoopArray<vec3>& normal, VolatileLoopArray<vec4>& vertexColor, VolatileLoopArray<uint32_t>& indices)
	{
		//based on GPSnoopy's RaytracingInVulkan's CornellBox file : https://github.com/GPSnoopy/RayTracingInVulkan/blob/master/src/Assets/CornellBox.cpp

		//cornell box is an open box with a back, right, left top and bottom panel and a light panel. this makes 6 quads, 6*4 vertices = 24
		//for uv's, we'll "cheat" and make it a 1D texture uv with the "1D texture" in the vertexColors
		pos.Alloc(24);
		uv.Alloc(24);
		normal.Alloc(24);

		//there is only 3 colors, red, green, and white so we'll put it first
		vertexColor.Alloc(3);
		vertexColor[0] = vec4(0.12f, 0.45f, 0.15f, 0.0f);//green
		vertexColor[1] = vec4(0.65f, 0.05f, 0.05f, 0.0f);//red
		vertexColor[2] = vec4(0.73f, 0.73f, 0.73f, 0.0f);//white

		//for each quad, you need 6 indices, this makes 6 quads * 6 indices = 36
		indices.Alloc(36);

		uint32_t i = 0;
		uint32_t indexOffset = 0;

		const float s = scale;

		//make the box
		{
			//l for left
			const vec3 l0(0.0f, 0.0f, 0.0f);
			const vec3 l1(0.0f, 0.0f, -s);
			const vec3 l2(0.0f, s, -s);
			const vec3 l3(0.0f, s, 0.0f);

			//r for right
			const vec3 r0(s, 0.0f, 0.0f);
			const vec3 r1(s, 0.0f, -s);
			const vec3 r2(s, s, -s);
			const vec3 r3(s, s, 0.0f);

			// Left green panel

			{
				pos[0] = l0;
				pos[1] = l1;
				pos[2] = l2;
				pos[3] = l3;
				vec2 tmpUv = vec2(0.0f, 0.0f);
				vec3 tmpNormal = vec3(1.0f, 0.0f, 0.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}
				indices[0] = 0;
				indices[1] = 1;
				indices[2] = 2;
				indices[3] = 0;
				indices[4] = 2;
				indices[5] = 3;
				indexOffset += 6;
			}

			// Right red panel
			{
				pos[i + 0] = r0;
				pos[i + 1] = r1;
				pos[i + 2] = r2;
				pos[i + 3] = r3;
				vec2 tmpUv = vec2(0.5f, 0.0f);
				vec3 tmpNormal = vec3(-1.0f, 0.0f, 0.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}

				indices[indexOffset+0]	= indexOffset + 2;
				indices[indexOffset + 1] = indexOffset + 1;
				indices[indexOffset + 2] = indexOffset + 0;
				indices[indexOffset + 3] = indexOffset + 3;
				indices[indexOffset + 4] = indexOffset + 2;
				indices[indexOffset + 5] = indexOffset +0;
				indexOffset += 6;
			}

			// Back white panel
			{
				pos[i + 0] = l1;
				pos[i + 1] = r1;
				pos[i + 2] = r2;
				pos[i + 3] = l2;
				vec2 tmpUv = vec2(1.0f, 0.0f);
				vec3 tmpNormal = vec3(0.0f, 0.0f, 1.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}
				indices[indexOffset + 0] = indexOffset + 0;
				indices[indexOffset + 1] = indexOffset + 1;
				indices[indexOffset + 2] = indexOffset + 2;
				indices[indexOffset + 3] = indexOffset + 0;
				indices[indexOffset + 4] = indexOffset + 2;
				indices[indexOffset + 5] = indexOffset + 3;
				indexOffset += 6;
			}

			// Bottom white panel
			{
				pos[i + 0] = l0;
				pos[i + 1] = r0;
				pos[i + 2] = r1;
				pos[i + 3] = l1;
				vec2 tmpUv = vec2(1.0f, 0.0f);
				vec3 tmpNormal = vec3(0.0f, 1.0f, 0.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}
				indices[indexOffset + 0] = indexOffset + 0;
				indices[indexOffset + 1] = indexOffset + 1;
				indices[indexOffset + 2] = indexOffset + 2;
				indices[indexOffset + 3] = indexOffset + 0;
				indices[indexOffset + 4] = indexOffset + 2;
				indices[indexOffset + 5] = indexOffset + 3;
				indexOffset += 6;
			}

			// Top white panel
			{
				pos[i + 0] = l2;
				pos[i + 1] = r2;
				pos[i + 2] = r3;
				pos[i + 3] = l3;
				vec2 tmpUv = vec2(1.0f, 0.0f);
				vec3 tmpNormal = vec3(0.0f, -1.0f, 0.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}
				indices[indexOffset + 0] = indexOffset + 0;
				indices[indexOffset + 1] = indexOffset + 1;
				indices[indexOffset + 2] = indexOffset + 2;
				indices[indexOffset + 3] = indexOffset + 0;
				indices[indexOffset + 4] = indexOffset + 2;
				indices[indexOffset + 5] = indexOffset + 3;
				indexOffset += 6;
			}
		}

		//make the light
		{

			const float x0 = s * (213.0f / 555.0f);
			const float x1 = s * (343.0f / 555.0f);
			const float z0 = s * (-555.0f + 332.0f) / 555.0f;
			const float z1 = s * (-555.0f + 227.0f) / 555.0f;
			const float y1 = s * 0.998f;

			// light panel
			{
				pos[i + 0] = vec3(x0, y1, z1);
				pos[i + 1] = vec3(x1, y1, z1);
				pos[i + 2] = vec3(x1, y1, z0);
				pos[i + 3] = vec3(x0, y1, z0);
				vec2 tmpUv = vec2(1.0f, 0.0f);
				vec3 tmpNormal = vec3(0.0f, -1.0f, 0.0f);
				for (char c = 0; c < 4; c++, i++)
				{
					uv[i] = tmpUv;
					normal[i] = tmpNormal;
				}
				indices[indexOffset + 0] = indexOffset + 0;
				indices[indexOffset + 1] = indexOffset + 1;
				indices[indexOffset + 2] = indexOffset + 2;
				indices[indexOffset + 3] = indexOffset + 0;
				indices[indexOffset + 4] = indexOffset + 2;
				indices[indexOffset + 5] = indexOffset + 3;
			}
		}
	}
}

#endif //__CORNELL_BOX__