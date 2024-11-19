#ifndef __RAYTRACING_HELPER_H__
#define __RAYTRACING_HELPER_H__

#include "Utilities.h"
#include "Maths.h"

/*
* the simple representation of a ray in 3D space
*/
struct ray
{
	//the origin of the ray
	vec3 origin;
	//the normalized direction of the ray
	vec3 direction;

	//gives out a point in 3D space, at a distance t from the ray's origin
	__forceinline vec3 at(float t)const noexcept
	{
		return origin + direction * t;
	}
};

/*
* a simple struct to get info back from the collision between a ray and a hittable object
*/
struct hit_record
{
	//the first contact point between the ray and the hittable object
	vec3 hit_point{};
	//the contact normal between the ray and the hittable object
	vec3 hit_normal{};
	//the distance between the ray origin and the contact point along the ray's direction
	float distance{0.0f};
	//the shade color for this hit
	vec4 shade{};
	//the material of the hit object
	struct material* mat{ nullptr };
};


/*
* a simple struct to represent a way for the system to ask for the compute of a ray with the scene
*/
struct ray_compute
{
	//said launched ray that we need to compute if it hits anything
	ray launched;
	//the final pixel we want to shade
	vec4* pixel{ nullptr };
	//the color the ray has computed so far
	vec4 color;
	//the number of time the same pixel has been computed
	uint32_t depth{0};
};

/*
* the simple virtual interface representation of a object that can be hit by a ray in 3D space
*/
struct hittable
{
	//a method to implement the collision beween the hittable object and a ray
	__forceinline virtual bool hit(const ray& incomming, hit_record& record)const = 0;


	//a method to implement the reflected ray from a hit.
	// careful, calling twice this method with the same parameter may not have the same result
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const = 0;

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)const = 0;
};

/* the simple representation of the material of an hittable object */
struct material
{
	//a method to implement the reflected ray from a hit.
	// careful, calling twice this method with the same parameter may not have the same result
	virtual __forceinline ray propagate(const ray& in, const hit_record& record)const = 0;

	//a method to implement what color does the hit returns
	virtual __forceinline vec4 shading(const hit_record& record)const = 0;
};

#endif //__RAYTRACING_HELPER_H__
