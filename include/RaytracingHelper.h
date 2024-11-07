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
	__forceinline vec3 at(float t)const
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
};


/*
* a simple struct to represent a way for the system to ask for the compute of a ray with the scene
*/
struct ray_compute
{
	//said launched ray that we need to compute if it hits anything
	ray launched{};
	//the final pixel we want to shade
	vec4* pixel{ nullptr };
};

/*
* the simple virtual interface representation of a object that can be hit by a ray in 3D space
*/
struct hittable
{
	//a method to implement the collision beween the hittable object and a ray
	virtual bool hit(const ray& incomming, hit_record& record) = 0;

	//a method to implement what color does the hit returns
	virtual vec4 shading(const hit_record& record) = 0;
};

struct sphere : public hittable
{
	sphere() = default;
	sphere(const vec3& _center, float _radius):
		center{_center},
		radius {_radius}
	{
	}

	vec3 center{};
	float radius{0.0f};

	//a method to implement the collision beween the hittable object and a ray
	__forceinline virtual bool hit(const ray& incomming, hit_record& record)override
	{
		vec3 ray_to_center = center - incomming.origin;

		//information on how the formula works can be found here : https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
		//this is a basic quadratic equation.
		//for the used formula here, it is the simplified one from here : https://raytracing.github.io/books/RayTracingInOneWeekend.html#surfacenormalsandmultipleobjects/simplifyingtheray-sphereintersectioncode

		//in my case, because ray direction are normalized, a == 1, so I'll move it out

		//h in geometrical terms is the distance from the ray's origin to the sphere's center
		//projected onto the ray's direction.
		float h = dot(incomming.direction, ray_to_center);

		//this is basically the squared distance between the ray's origin and the sphere's center
		//substracted to the sphere's squared radius.
		float c = dot(ray_to_center, ray_to_center) - radius * radius;

		//a == 1 so it is not h^2 - ac but h^2 - c
		float discriminant = h * h - c;

		//we're not in a complex plane, so sqrt(-1) is impossible
		if (discriminant < 0)
			return false;
		
		//finding the roots
		float sqrt_discriminant = sqrtf(discriminant);

		float root_1 = h + sqrt_discriminant;
		float root_2 = h - sqrt_discriminant;

		if (root_1 < 0.01f && root_2 < 0.01f)
			return false;

		//choosing our closest root
		record.distance		= fminf(root_1, root_2);
		record.hit_point	= incomming.at(record.distance);
		record.hit_normal	= (record.hit_point - center) / radius;//this is a normalized vector
		record.shade		= shading(record);
		return true;
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)override
	{
		return vec4{ 0.7f, 0.8f, 0.5f, 1.0f };
	}
};

#endif //__RAYTRACING_HELPER_H__