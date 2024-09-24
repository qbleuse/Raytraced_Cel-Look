#ifndef __MATHS_H__
#define __MATHS_H__

#include <stdint.h>
#include <math.h>

/* struct representing a mathematical 2 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec2
{
	union
	{
		float array[2];
		struct
		{
			float x;
			float y;

		};
		struct
		{
			float r;
			float g;
		};

	};

	vec2() = default;
	vec2(float _x, float _y) :
		x{ _x },
		y{ _y }
	{
	}
	//vec2(struct ImVec2 imvec) :
	//	x{ imvec.x },
	//	y{ imvec.y }
	//{
	//}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return array[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return array[idx]; }

	/* math operation */

	__forceinline vec2& operator+=(const vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
	__forceinline vec2& operator-=(const vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
	
};

/* typedef */

typedef vec2 float2;
typedef vec2 Vec2;

/* math operation */

__forceinline vec2 operator+(const vec2& lhs, const vec2& rhs) { return vec2(lhs.x + rhs.x, lhs.y + rhs.y); }
__forceinline vec2 operator-(const vec2& lhs, const vec2& rhs) { return vec2(lhs.x - rhs.x, lhs.y - rhs.y); }

__forceinline float dot(const vec2& lhs, const vec2& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y; }
__forceinline float length(const vec2& vec) { return vec.x + vec.y == 0 ? 0 : sqrtf(vec.x * vec.x + vec.y * vec.y); }
__forceinline vec2	normalize(const vec2& vec) { float l = length(vec); return l == 0 ? vec2(0, 0) : vec2(vec.x / l, vec.y / l); }

/* struct representing a mathematical 3 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec3
{
	union
	{
		float array[3];
		struct
		{
			float x;
			float y;
			float z;

		};
		struct
		{
			float r;
			float g;
			float b;
		};
		vec2 xy;
		vec2 rg;
	};

	vec3() = default;
	vec3(float _x, float _y, float _z) :
		x{ _x },
		y{ _y },
		z{_z}
	{
	}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return array[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return array[idx]; }

	/* math operation */

	__forceinline vec3& operator+=(const vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	__forceinline vec3& operator-=(const vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
};

/* typedef */

typedef vec3 float3;
typedef vec3 Vec3;

/* math operation */

__forceinline vec3 operator+(const vec3& lhs, const vec3& rhs) { return vec3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z); }
__forceinline vec3 operator-(const vec3& lhs, const vec3& rhs) { return vec3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z); }

__forceinline float dot(const vec3& lhs, const vec3& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z; }
__forceinline float length(const vec3& vec) { return vec.x + vec.y + vec.z == 0.0f ? 0.0f : sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z); }
__forceinline vec3	normalize(const vec3& vec) { float l = length(vec); return l == 0 ? vec3(0.0f, 0.0f, 0.0f) : vec3(vec.x / l, vec.y / l, vec.z / l); }
__forceinline vec3	cross(const vec3& lhs, const vec3& rhs) { return vec3(lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x); }


/* struct representing a mathematical 4 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec4
{
	union
	{
		float array[4];
		struct
		{
			float x;
			float y;
			float z;
			float w;

		};
		struct
		{
			float r;
			float g;
			float b;
			float a;
		};
		struct
		{
			vec2 xy;
			vec2 zw;
		};
		struct 
		{
			vec2 rg;
			vec2 ba;
		};
		vec3 xyz;
		vec3 rgb;
	};

	vec4() = default;
	vec4(float _x, float _y, float _z, float _w) :
		x{ _x },
		y{ _y },
		z{ _z },
		w{ _w }
	{
	}

	//vec4(struct ImVec4 imvec) :
	//	x{ imvec.x },
	//	y{ imvec.y },
	//	z{ imvec.z },
	//	w {imvec.w}
	//{
	//}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return array[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return array[idx]; }

	/* math operation */

	__forceinline vec4& operator+=(const vec4& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
	__forceinline vec4& operator-=(const vec4& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w += rhs.w; return *this; }

};

/* typedef */

typedef vec4 float4;
typedef vec4 Vec4;

/* math operation */

__forceinline vec4 operator+(const vec4& lhs, const vec4& rhs) { return vec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.z); }
__forceinline vec4 operator-(const vec4& lhs, const vec4& rhs) { return vec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.z); }

__forceinline float dot(const vec4& lhs, const vec4& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }
__forceinline float length(const vec4& vec) { return vec.x + vec.y + vec.z + vec.w == 0.0f ? 0.0f : sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }
__forceinline vec4	normalize(const vec4& vec) { float l = length(vec); return l == 0 ? vec4(0.0f, 0.0f, 0.0f, 0.0f) : vec4(vec.x / l, vec.y / l, vec.z / l, vec.w / l); }
__forceinline vec4	cross(const vec4& lhs, const vec4& rhs) { return vec4(lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.w - lhs.w * rhs.z, lhs.w * rhs.x - lhs.x * rhs.w, lhs.x * rhs.y - lhs.y * rhs.x); }

/* struct representing a mathematical 4 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct mat4
{
	union
	{
		vec4 array[4];
		struct
		{
			vec4 x;
			vec4 y;
			vec4 z;
			vec4 w;

		};
	};

	mat4() = default;

};


#endif //__MATHS_H__