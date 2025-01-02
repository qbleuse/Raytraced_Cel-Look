#ifndef __RAYTRACING_HELPER_INL__
#define __RAYTRACING_HELPER_INL__

#include "RaytraceCPUHelper.h"


/*===== MATERIALS IMPLEMENTATION =====*/

/*
* material interface implementation to represent a rough diffuse material.
* It uses a lambertian difffuse.
*/
struct diffuse : public material
{
	__forceinline diffuse() = default;
	__forceinline diffuse(const vec4& color)noexcept :
		_albedo{ color }
	{
	}

	vec4 _albedo;


	//a method to implement the reflected ray from a hit.
	//diffuse reflection, or basically, lambertian "random" reflection
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		ray reflect{ record.hit_point, record.hit_normal };
		reflect.direction = normalize(reflect.direction + normalize(vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) }));

		return reflect;
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override	{ return _albedo; }
};

/*
* material interface implementation to represent metal.
* Basically perfect mirror-like reflection.
*/
struct metal : public material
{
	__forceinline metal() = default;
	__forceinline metal(const vec4& color)noexcept :
		_albedo{ color }
	{
	}

	vec4 _albedo;


	//a method to implement the reflected ray from a hit.
	//perfect reflection so basically taking the incident ray
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		//are we in the object or outside ?
		bool front_face = dot(record.hit_normal, in.direction) < 0.0f;
		//normal is given inside-out, should be reversed if not front
		vec3 normal = front_face ? record.hit_normal : -record.hit_normal;

		//the reflected ray
		ray reflect;
		reflect.origin = record.hit_point;
		//basically projeting the incident ray on the normal and adding to the incident ray the oppposite vector of this projection
		reflect.direction = normalize(in.direction - (normal * 2.0f * dot(in.direction, normal)));

		return reflect;
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override { return _albedo; }
};

/*
* material interface implementation of dielectrics, such as glass or water.
* Those materials both refelct and refract.
*/
struct dieletrics : public metal
{
	__forceinline dieletrics() = default;
	__forceinline dieletrics(const vec4& color, float index) noexcept :
		_albedo{ color },
		_refract_index{ index }
	{
	}

	vec4	_albedo;
	float	_refract_index{ 0.0f };

	//diffuse reflection, or basically, lambertian "random" reflection
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		//we will use snell's law to approximate refraction properties
		//the ray will refract when the angle between the ray and the normal is less then 90 degrees
		//(in other words, when it can refract inside the sphere) and reflect otherwise

		//are we in the object or outside ?
		bool front_face		= dot(record.hit_normal, in.direction) < 0.0f;
		//normal is given inside-out, should be reversed if not front
		vec3 normal			= front_face ? record.hit_normal : -record.hit_normal;
		//the refractive index between the material (changes if we are inside or outside the object)
		float refract_index = front_face ? (1.0f / _refract_index) : _refract_index;


		//the cos angle between the ray and the normal
		float cos_ray_normal = fmin(dot(-in.direction, normal), 1.0f);
		//from trigonometry property -> cos^2 + sin^2 = 1, we get sin of angle
		float sin_ray_normal = sqrtf(1.0f - (cos_ray_normal * cos_ray_normal));

		//this is the result of snell's law, a hard rule for which ray should refract or not.
		bool cannot_refract = refract_index * sin_ray_normal > 1.0f;

		//as snell's law is not really enough to properly represent refraction, 
		//we'll also use Schlick's "reflectance", which is a quatitative approximation of the actual refraction law 
		float reflectance = (1.0f - refract_index) / (1.0f + refract_index);
		reflectance *= reflectance;
		reflectance = reflectance + (1.0f * reflectance) * powf((1.0f - cos_ray_normal), 5);

		//as snell's law is an approximation, it actually takes more of a probabilistic approach, thus the random.
		if (cannot_refract || reflectance > randf())
		{
			//full-on reflection
			return metal::propagate(in, record);
		}
		else
		{
			//the refracted ray
			ray refract;
			refract.origin = record.hit_point;

			//we'll decompose the refracted ray in two components : the ray on the contact plane and orthogonal to the plane
			
			//the orthogonal component to our contact plane of our refracted ray
			vec3 orth_comp		= (in.direction + (normal * cos_ray_normal)) * refract_index;
			//the tangential component to our contact plane of our refracted ray
			vec3 tangent_comp	= normal * -sqrtf(fabs(1.0f - dot(orth_comp, orth_comp)));
			//the total refracted ray
			refract.direction = normalize(tangent_comp + orth_comp);
			return refract;
		}
	}

	//a method to implement what color does the hit returns
	__forceinline virtual vec4 shading(const hit_record& record)const noexcept override { return _albedo; }

};


/*===== HITTABLE OBJECTS IMPLEMENTATION =====*/

#define HIT_EPSILON 0.01f

/*
* the representation of an ray-hittable sphere in 3D space.
* The only implemented 3D object for this demo, 
* as sphere is the simplest object to represent in terms of 3D collision in 3D space.
*/
struct sphere : public hittable
{
	__forceinline sphere() = default;
	__forceinline sphere(const vec3& _center, float _radius, material* mat = nullptr) noexcept :
		_center{ _center },
		_radius{ _radius },
		_material{ mat }
	{
	}

	//the center of our sphere in 3D space (also its position)
	vec3		_center;
	//the radius (and thus size) of our sphere
	float		_radius{ 0.0f };
	//the material of our sphere
	material*	_material{ nullptr };

	//a method to implement the collision beween the hittable object and a ray
	__forceinline virtual bool hit(const ray& incomming, hit_record& record)const override
	{
		vec3 ray_to_center = _center - incomming.origin;

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
		float c = dot(ray_to_center, ray_to_center) - (_radius * _radius);

		//a == 1 so it is not h^2 - ac but h^2 - c
		float discriminant = (h * h) - (a * c);

		//we're not in a complex plane, so sqrt(-1) is impossible
		if (discriminant <= 0.0f)
			return false;

		//finding the roots
		float sqrt_discriminant = sqrtf(discriminant);

		float root_1 = (h + sqrt_discriminant) / (a);
		float root_2 = (h - sqrt_discriminant) / (a);

		if (root_1 < HIT_EPSILON && root_2 < HIT_EPSILON)
			return false;

		//choosing our closest root
		record.distance		= fminf(root_1, root_2);
		record.distance		= record.distance < HIT_EPSILON ? fmax(root_1, root_2) : record.distance;//avoid considering a collision behind the camera
		record.hit_point	= incomming.at(record.distance);
		record.hit_normal	= (record.hit_point - _center) / _radius;//this is a normalized vector
		record.shade		= shading(record);
		record.mat			= _material;
		return true;
	}

	//a method to implement what color does the hit returns, basically depends on the material
	__forceinline virtual vec4 shading(const hit_record& record)const override
	{
		return _material == nullptr ? vec4{ 0.0f, 0.0f, 0.0f, 0.0f } : _material->shading(record);
	}

	//a method to implement the reflected ray from a hit, basically depends on the material
	__forceinline virtual ray propagate(const ray& in, const hit_record& record)const override
	{
		return _material == nullptr ? ray{} : _material->propagate(in, record);
	}
};



#endif //__RAYTRACING_HELPER_INL__