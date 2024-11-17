#ifndef __RAYTRACING_HELPER_INL__
#define __RAYTRACING_HELPER_INL__

#include "RaytraceCPUHelper.h"


/*===== MATERIALS IMPLEMENTATION =====*/

struct diffuse : public material
{
	__forceinline diffuse() = default;
	__forceinline diffuse(const vec4& color)noexcept :
		albedo{ color }
	{
	}

	vec4 albedo{};

	//diffuse reflection, or basically, lambertian "random" reflection
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		ray reflect{ record.hit_point, record.hit_normal };
		reflect.direction = normalize(reflect.direction + normalize(vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) }));

		return reflect;
	}

	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override
	{
		return albedo;
	}
};

struct metal : public material
{
	__forceinline metal() = default;
	__forceinline metal(const vec4& color)noexcept :
		albedo{ color }
	{
	}

	vec4 albedo{};

	//diffuse reflection, or basically, lambertian "random" reflection
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		bool front_face = dot(record.hit_normal, in.direction) < 0.0f;
		vec3 normal = front_face ? record.hit_normal : -record.hit_normal;

		ray reflect;
		reflect.origin = record.hit_point;
		reflect.direction = normalize(in.direction - (normal * 2.0f * dot(in.direction, normal)));

		return reflect;
	}

	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override
	{
		return albedo;
	}

};


struct dieletrics : public metal
{
	__forceinline dieletrics() = default;
	__forceinline dieletrics(const vec4& color, float index) noexcept :
		albedo{ color },
		refract_index{ index }
	{
	}

	vec4 albedo{};
	float refract_index{ 0.0f };

	//diffuse reflection, or basically, lambertian "random" reflection
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		//we will use snell's law to approximate refraction properties
		//the ray will refract when the angle between the ray and the normal is less then 90 degrees
		//(in other words, when it can refract inside the sphere) and reflect otherwise

		bool front_face = dot(record.hit_normal, in.direction) < 0.0f;
		vec3 normal = front_face ? record.hit_normal : -record.hit_normal;
		vec3 uv = normalize(in.direction);
		//the refractive index between the material (changes if we are inside or outside the object)
		float index = front_face ? (1.0f / refract_index) : refract_index;


		//the cos angle between the ray and the normal
		float cos_ray_normal = fmin(dot(-uv, normal), 1.0f);
		//from trigonometry property -> cos^2 + sin^2 = 1, we get sin of angle
		float sin_ray_normal = sqrtf(1.0f - (cos_ray_normal * cos_ray_normal));

		bool cannot_refract = index * sin_ray_normal > 1.0f;

		float reflectance = (1.0f - index) / (1.0f + index);
		reflectance *= reflectance;
		reflectance = reflectance + (1.0f * reflectance) * powf((1.0f - cos_ray_normal), 5);

		if (cannot_refract || reflectance > randf())
		{
			ray reflect = metal::propagate(in, record);
			return reflect;
		}
		else
		{
			ray refract;
			refract.origin = record.hit_point;
			vec3 cos = (uv + (normal * cos_ray_normal)) * index;
			vec3 sin = normal * -sqrtf(fabs(1.0f - dot(cos, cos)));
			refract.direction = normalize(cos + sin);
			return refract;
		}
	}

	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override
	{
		return albedo;
	}

};


/*===== HITTABLE OBJECTS IMPLEMENTATION =====*/

struct sphere : public hittable
{
	__forceinline sphere() = default;
	__forceinline sphere(const vec3& _center, float _radius, material* mat = nullptr) noexcept :
		center{ _center },
		radius{ _radius },
		_material{ mat }
	{
	}

	vec3		center{};
	float		radius{ 0.0f };
	material* _material{ nullptr };

	//a method to implement the collision beween the hittable object and a ray
	__forceinline virtual bool hit(const ray& incomming, hit_record& record)const override
	{
		vec3 ray_to_center = center - incomming.origin;

		//information on how the formula works can be found here : https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
		//this is a basic quadratic equation.
		//for the used formula here, it is the simplified one from here : https://raytracing.github.io/books/RayTracingInOneWeekend.html#surfacenormalsandmultipleobjects/simplifyingtheray-sphereintersectioncode

		//in my case, because ray direction are normalized, a == 1, so I'll move it out
		float a = dot(incomming.direction, incomming.direction);

		//h in geometrical terms is the distance from the ray's origin to the sphere's center
		//projected onto the ray's direction.
		float h = dot(incomming.direction, ray_to_center);

		//this is basically the squared distance between the ray's origin and the sphere's center
		//substracted to the sphere's squared radius.
		float c = dot(ray_to_center, ray_to_center) - (radius * radius);

		//a == 1 so it is not h^2 - ac but h^2 - c
		float discriminant = (h * h) - (a * c);

		//we're not in a complex plane, so sqrt(-1) is impossible
		if (discriminant <= 0.0f)
			return false;

		//finding the roots
		float sqrt_discriminant = sqrtf(discriminant);

		float root_1 = (h + sqrt_discriminant) / (a);
		float root_2 = (h - sqrt_discriminant) / (a);

		if (root_1 < 0.1f && root_2 < 0.1f)
			return false;

		//choosing our closest root
		record.distance = fminf(root_1, root_2);
		record.distance = record.distance < 0.01f ? fmax(root_1, root_2) : record.distance;
		record.hit_point = incomming.at(record.distance);
		record.hit_normal = (record.hit_point - center) / radius;//this is a normalized vector
		record.shade = shading(record);
		record.mat = _material;
		return true;
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)const override
	{
		return _material == nullptr ? vec4{ 0.0f, 0.0f, 0.0f, 0.0f } : _material->shading(record);
	}

	//a method to implement what color does the hit returns
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		return _material == nullptr ? ray{} : _material->propagate(in, record);
	}
};



#endif //__RAYTRACING_HELPER_INL__