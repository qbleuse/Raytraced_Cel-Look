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
    //are we inside or outside an object
    bool front_face{true};
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
	ray launched{};
	//the final pixel we want to shade
	vec4* pixel{ nullptr };
	//the color the ray has computed so far
	vec4 color;
};

/*
* the simple virtual interface representation of a object that can be hit by a ray in 3D space
*/
struct hittable
{
	//a method to implement the collision beween the hittable object and a ray
	virtual bool hit(const ray& incomming, hit_record& record) = 0;

	virtual ray reflect(const ray& in, const hit_record& record) = 0;

	//a method to implement what color does the hit returns
	virtual vec4 shading(const hit_record& record) = 0;
};

/* the simple representation of the material of an hittable object */
struct material
{
	//a method to implement the reflected ray from a hit.
	// careful, calling twice this method with the same parameter may not have the same result
	virtual ray reflect(const ray& in, const hit_record& record) = 0;

	//a method to implement what color does the hit returns
	virtual vec4 shading(const hit_record& record) = 0;
};

struct diffuse : public material
{
	diffuse() = default;
	diffuse(const vec4& color) :
		albedo{ color }
	{
	}

	vec4 albedo{};

	//diffuse reflection, or basically, lambertian "random" reflection
	virtual ray reflect(const ray& in, const hit_record& record)
	{
		ray reflect{ record.hit_point, record.hit_normal };
		reflect.direction = normalize(reflect.direction + normalize(vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) }));

		return reflect;
	}

	virtual vec4 shading(const hit_record& record)
	{
		return albedo;
	}
};

struct metal : public material
{
	metal() = default;
	metal(const vec4& color) :
		albedo{ color }
	{
	}

	vec4 albedo{};

	//diffuse reflection, or basically, lambertian "random" reflection
	virtual ray reflect(const ray& in, const hit_record& record)
	{
		ray reflect;
		reflect.origin = record.hit_point;
		reflect.direction = normalize(in.direction - record.hit_normal * 2.0f * dot(in.direction,record.hit_normal));

		return reflect;
	}

	virtual vec4 shading(const hit_record& record)
	{
		return albedo;
	}

};


struct dieletrics : public metal
{
	dieletrics() = default;
	dieletrics(const vec4& color, float index) :
		albedo{ color },
		refract_index{index}
	{
	}

	vec4 albedo{};
	float refract_index{0.0f};

	//diffuse reflection, or basically, lambertian "random" reflection
	virtual ray reflect(const ray& in, const hit_record& record)override
	{
	    //we will use snell's law to approximate refraction properties
		//the ray will refract when the angle between the ray and the normal is less then 90 degrees
		//(in other words, when it can refract inside the sphere) and reflect otherwise

        //the refractive index between the material (changes if we are inside or outside the object)
        float index = record.front_face ? (1.0f/refract_index) : refract_index;
		//the cos angle between the ray and the normal
		float cos_ray_normal = fmin(dot(-in.direction, record.hit_normal),1.0f);
		//from trigonometry property -> cos^2 + sin^2 = 1, we get sin of angle
		float sin_ray_normal = sqrtf(1.0f - cos_ray_normal * cos_ray_normal);

		//if (refract_index * sin_ray_normal > 1.0f)
		{
		    ray refract;
			refract.origin = record.hit_point;
            vec3 cos = (in.direction + (record.hit_normal * cos_ray_normal)) * index;
            vec3 sin = -record.hit_normal * sin_ray_normal;// * -sqrtf(fabs(1.0f - dot(cos,cos)));
            refract.direction = normalize(cos + sin);
			return refract;
		}

		//return metal::reflect(in, record);
	}

	virtual vec4 shading(const hit_record& record)override
	{
		return albedo;
	}

};


struct sphere : public hittable
{
	sphere() = default;
	sphere(const vec3& _center, float _radius, material* mat = nullptr):
		center{_center},
		radius {_radius},
		_material{ mat}
	{
	}

	vec3		center{};
	float		radius{0.0f};
	material*	_material{ nullptr };

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
		if (discriminant < 0.0f)
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
        record.front_face   = dot(record.hit_normal,incomming.direction) < 0;
        record.hit_normal   = record.front_face ? record.hit_normal : -record.hit_normal;
		record.shade		= shading(record);
		record.mat			= _material;
		return true;
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)override
	{
		return _material == nullptr ? vec4{ 0.0f, 0.0f, 0.0f, 0.0f} : _material->shading(record);
	}

	//a method to implement what color does the hit returns
	__forceinline virtual ray reflect(const ray& in, const hit_record& record)override
	{
		return _material == nullptr ? ray{} : _material->reflect(in, record);
	}
};

#endif //__RAYTRACING_HELPER_H__
