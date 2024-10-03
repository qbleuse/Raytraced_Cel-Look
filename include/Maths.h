#ifndef __MATHS_H__
#define __MATHS_H__

#ifndef _WIN32
#define __forceinline inline
#endif

#include <stdint.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define DEGREES_TO_RADIANS M_PI/180.0f
#define RADIANS_TO_DEGREES 180.0f/M_PI

/* struct representing a mathematical 2 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec2
{
	union
	{
		float scalar[2];
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

	//vec2() = default;
	//vec2(const vec2&) = default;
	//vec2(float _x, float _y) :
	//	x{ _x },
	//	y{ _y }
	//{
	//}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return scalar[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return scalar[idx]; }

	/* math operation */

	__forceinline vec2& operator+=(const vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
	__forceinline vec2& operator-=(const vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }

};

/* typedef */

typedef vec2 float2;
typedef vec2 Vec2;

/* math operation */

__forceinline vec2 operator+(const vec2& lhs, const vec2& rhs) { return {{lhs.x + rhs.x, lhs.y + rhs.y}}; }
__forceinline vec2 operator-(const vec2& lhs, const vec2& rhs) { return {{lhs.x - rhs.x, lhs.y - rhs.y}}; }
__forceinline vec2 operator*(const vec2& vec, const float& scalar) { return { {vec.x * scalar, vec.y * scalar} }; }


__forceinline float dot(const vec2& lhs, const vec2& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y; }
__forceinline float length(const vec2& vec) { return vec.x + vec.y == 0 ? 0 : sqrtf(vec.x * vec.x + vec.y * vec.y); }
__forceinline vec2	normalize(const vec2& vec) { float l = length(vec); return l == 0 ? vec2{{0, 0}} : vec2{{vec.x / l, vec.y / l}}; }

/* struct representing a mathematical 3 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec3
{
	union
	{
		float scalar[3];
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

	//vec3() = default;
	//vec3(const vec3&) = default;
	//vec3(float _x, float _y, float _z) :
	//	x{ _x },
	//	y{ _y },
	//	z{_z}
	//{
	//}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return scalar[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return scalar[idx]; }

	/* math operation */

	__forceinline vec3& operator+=(const vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	__forceinline vec3& operator-=(const vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
};

/* typedef */

typedef vec3 float3;
typedef vec3 Vec3;

/* math operation */

__forceinline vec3 operator+(const vec3& lhs, const vec3& rhs) { return {{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z}}; }
__forceinline vec3 operator-(const vec3& lhs, const vec3& rhs) { return {{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z}}; }
__forceinline vec3 operator*(const vec3& vec, const float& scalar) { return { {vec.x * scalar, vec.y * scalar, vec.z * scalar} }; }

__forceinline float dot(const vec3& lhs, const vec3& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z; }
__forceinline float length(const vec3& vec) { return vec.x + vec.y + vec.z == 0.0f ? 0.0f : sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z); }
__forceinline vec3	normalize(const vec3& vec) { float l = length(vec); return l == 0 ? vec3{{0.0f, 0.0f, 0.0f}} : vec3{{vec.x / l, vec.y / l, vec.z / l}}; }
__forceinline vec3	cross(const vec3& lhs, const vec3& rhs) { return {{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x}}; }


/* struct representing a mathematical 4 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct vec4
{
	union
	{
		float scalar[4];
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

	//vec4() = default;
	//vec4(const vec4&) = default;
	//vec4(float _x, float _y, float _z, float _w) :
	//	x{ _x },
	//	y{ _y },
	//	z{ _z },
	//	w{ _w }
	//{
	//}

	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return scalar[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return scalar[idx]; }

	/* math operation */

	__forceinline vec4& operator+=(const vec4& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
	__forceinline vec4& operator-=(const vec4& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w += rhs.w; return *this; }

};

/* typedef */

typedef vec4 float4;
typedef vec4 Vec4;

/* math operation */

__forceinline vec4 operator+(const vec4& lhs, const vec4& rhs) { return {{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.z}}; }
__forceinline vec4 operator-(const vec4& lhs, const vec4& rhs) { return {{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.z}}; }
__forceinline vec4 operator*(const vec4& vec, const float& scalar) { return { {vec.x * scalar, vec.y * scalar, vec.z * scalar, vec.w * scalar} }; }


__forceinline float dot(const vec4& lhs, const vec4& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }
__forceinline float length(const vec4& vec) { return vec.x + vec.y + vec.z + vec.w == 0.0f ? 0.0f : sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }
__forceinline vec4	normalize(const vec4& vec) { float l = length(vec); return l == 0 ? vec4{{0.0f, 0.0f, 0.0f, 0.0f}} : vec4{{vec.x / l, vec.y / l, vec.z / l, vec.w / l}}; }
__forceinline vec4	cross(const vec4& lhs, const vec4& rhs) { return {{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.w - lhs.w * rhs.z, lhs.w * rhs.x - lhs.x * rhs.w, lhs.x * rhs.y - lhs.y * rhs.x}}; }

/* struct representing a mathematical 4 dimensional vector. it has been made to resemble the one you may encounter in glsl or hlsl. */
struct mat4
{
	union
	{
		float scalar[16];
		vec4 vector[4];
		struct
		{
			vec4 x;
			vec4 y;
			vec4 z;
			vec4 w;

		};
	};

	//mat4() = default;
	//mat4(const mat4&) = default;


	/* accessor */

	__forceinline float& operator[] (uint32_t idx) { return scalar[idx]; }
	__forceinline float  operator[] (uint32_t idx) const { return scalar[idx]; }

};

/* creates a 4 dimension scale matrix to scale uniformly on x, y, z */
__forceinline mat4 uniform_scale(float scale)
{

	return mat4{ {
		scale, 0.0f, 0.0f, 0.0f,
		0.0f, scale, 0.0f, 0.0f,
		0.0f, 0.0f, scale, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension scale matrix to scale on x, y, z from given extent*/
__forceinline mat4 scale(float extent_x, float extent_y, float extent_z)
{

	return mat4{ {
		extent_x, 0.0f, 0.0f, 0.0f,
		0.0f, extent_y, 0.0f, 0.0f,
		0.0f, 0.0f, extent_z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension translate matrix to given position in column format*/
__forceinline mat4 co_translate(const vec3& pos)
{
	return mat4{{
		1.0f, 0.0f, 0.0f, pos.x,
		0.0f, 1.0f, 0.0f, pos.y,
		0.0f, 0.0f, 1.0f, pos.z,
		0.0f, 0.0f, 0.0f, 1.0f
	}};
}

/* creates a 4 dimension translate matrix to given position in row format*/
__forceinline mat4 ro_translate(const vec3& pos)
{
	return mat4{ {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		pos.x, pos.y, pos.z, 1.0f
	} };
}

/* creates a 4 dimension matrix  with a roll rotation (on the x axis) of "angle". */
__forceinline mat4 co_roll(float angle)
{
	float cos_angle = cosf(angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(angle * DEGREES_TO_RADIANS);

	return mat4{ {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cos_angle, -sin_angle, 0.0f,
		0.0f, sin_angle, cos_angle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f		
	} };
}

/* creates a 4 dimension matrix  with a roll rotation (on the x axis) of "angle". */
__forceinline mat4 ro_roll(float angle)
{
	float cos_angle = cosf(-angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(-angle * DEGREES_TO_RADIANS);

	return mat4{ {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cos_angle, -sin_angle, 0.0f,
		0.0f, sin_angle, cos_angle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension matrix  with a pitch rotation (on the y axis) of "angle". */
__forceinline mat4 co_pitch(float angle)
{
	float cos_angle = cosf(angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(angle * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_angle, 0.0f, sin_angle, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sin_angle, 0.0f, cos_angle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension matrix  with a pitch rotation (on the y axis) of "angle". */
__forceinline mat4 ro_pitch(float angle)
{
	float cos_angle = cosf(-angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(-angle * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_angle, 0.0f, sin_angle, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sin_angle, 0.0f, cos_angle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension matrix  with a yaw rotation (on the z axis) of "angle". */
__forceinline mat4 co_yaw(float angle)
{
	float cos_angle = cosf(angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(angle * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_angle, -sin_angle, 0.0f, 0.0f,
		sin_angle, cos_angle, 0.0f, 0.0f,
		0.0f, 0.0f,1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension matrix  with a yaw rotation (on the z axis) of "angle". */
__forceinline mat4 ro_yaw(float angle)
{
	float cos_angle = cosf(-angle * DEGREES_TO_RADIANS);
	float sin_angle = sinf(-angle * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_angle, -sin_angle, 0.0f, 0.0f,
		sin_angle, cos_angle, 0.0f, 0.0f,
		0.0f, 0.0f,1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension intrinsic rotation matrix from yaw, pitch and roll angle. */
__forceinline mat4 co_intrinsic_rot(float yaw, float pitch, float roll)
{
	float cos_yaw	= cosf(yaw * DEGREES_TO_RADIANS);
	float cos_pitch = cosf(pitch * DEGREES_TO_RADIANS);
	float cos_roll	= cosf(roll * DEGREES_TO_RADIANS);


	float sin_yaw	= sinf(yaw * DEGREES_TO_RADIANS);
	float sin_pitch = sinf(pitch * DEGREES_TO_RADIANS);
	float sin_roll	= sinf(roll * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_yaw * cos_pitch, cos_yaw * sin_pitch * sin_roll - sin_yaw * cos_roll, cos_yaw * sin_pitch * cos_roll + sin_yaw * sin_roll, 0.0f,
		sin_yaw * cos_pitch, sin_yaw * sin_pitch * sin_roll + cos_yaw * cos_roll, sin_yaw * sin_pitch * cos_roll - cos_yaw * sin_roll, 0.0f,
		-sin_pitch, cos_pitch * sin_roll, cos_pitch * cos_roll, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension intrinsic rotation matrix from yaw, pitch and roll angle. */
__forceinline mat4 ro_intrinsic_rot(float yaw, float pitch, float roll)
{
	float cos_yaw = cosf(-yaw * DEGREES_TO_RADIANS);
	float cos_pitch = cosf(-pitch * DEGREES_TO_RADIANS);
	float cos_roll = cosf(-roll * DEGREES_TO_RADIANS);


	float sin_yaw = sinf(-yaw * DEGREES_TO_RADIANS);
	float sin_pitch = sinf(-pitch * DEGREES_TO_RADIANS);
	float sin_roll = sinf(-roll * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_yaw * cos_pitch, cos_yaw * sin_pitch * sin_roll - sin_yaw * cos_roll, cos_yaw * sin_pitch * cos_roll + sin_yaw * sin_roll, 0.0f,
		sin_yaw * cos_pitch, sin_yaw * sin_pitch * sin_roll + cos_yaw * cos_roll, sin_yaw * sin_pitch * cos_roll - cos_yaw * sin_roll, 0.0f,
		-sin_pitch, cos_pitch * sin_roll, cos_pitch * cos_roll, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension extrensic rotation matrix from yaw, pitch and roll angle. */
__forceinline mat4 co_extrinsic_rot(float yaw, float pitch, float roll)
{
	float cos_yaw	= cosf(yaw * DEGREES_TO_RADIANS);
	float cos_pitch = cosf(pitch * DEGREES_TO_RADIANS);
	float cos_roll	= cosf(roll * DEGREES_TO_RADIANS);


	float sin_yaw	= sinf(yaw * DEGREES_TO_RADIANS);
	float sin_pitch = sinf(pitch * DEGREES_TO_RADIANS);
	float sin_roll	= sinf(roll * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_pitch * cos_yaw, sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw, cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw, 0.0f,
		cos_pitch * sin_yaw, sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw, cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw, 0.0f,
		-sin_pitch, sin_roll * cos_pitch, cos_roll * cos_pitch, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension extrensic rotation matrix from yaw, pitch and roll angle. */
__forceinline mat4 ro_extrinsic_rot(float yaw, float pitch, float roll)
{
	float cos_yaw = cosf(-yaw * DEGREES_TO_RADIANS);
	float cos_pitch = cosf(-pitch * DEGREES_TO_RADIANS);
	float cos_roll = cosf(-roll * DEGREES_TO_RADIANS);


	float sin_yaw = sinf(-yaw * DEGREES_TO_RADIANS);
	float sin_pitch = sinf(-pitch * DEGREES_TO_RADIANS);
	float sin_roll = sinf(-roll * DEGREES_TO_RADIANS);

	return mat4{ {
		cos_pitch * cos_yaw, sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw, cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw, 0.0f,
		cos_pitch * sin_yaw, sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw, cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw, 0.0f,
		-sin_pitch, sin_roll * cos_pitch, cos_roll * cos_pitch, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	} };
}

/* creates a 4 dimension matrix for perspective projection onto a 2D plane. */
__forceinline mat4 co_perspective_proj(float width, float height, float fov, float near_plane, float far_plane)
{
	float aspect_ratio = width > height ? width / height : height / width;

	return mat4{ {
		1.0f / (aspect_ratio * tanf(fov * DEGREES_TO_RADIANS * 0.5f)), 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / (tanf(fov * DEGREES_TO_RADIANS * 0.5f)), 0.0f, 0.0f,
		0.0f, 0.0f, -(far_plane+ near_plane) / (far_plane - near_plane), (- 2.0f * near_plane * far_plane )/ (far_plane - near_plane),
		0.0f, 0.0f, -1.0f, 0.0f
	} };
}

/* creates a 4 dimension matrix for perspective projection onto a 2D plane. */
__forceinline mat4 ro_perspective_proj(float width, float height, float fov, float near_plane, float far_plane)
{
	float aspect_ratio = width > height ? width / height : height / width;

	return mat4{ {
		1.0f / (aspect_ratio * tanf(fov * DEGREES_TO_RADIANS * 0.5f)), 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / (tanf(fov * DEGREES_TO_RADIANS * 0.5f)), 0.0f, 0.0f,
		0.0f, 0.0f, -(far_plane + near_plane) / (far_plane - near_plane), -1.0f,
		0.0f, 0.0f, -(near_plane * far_plane) / (far_plane - near_plane), 0.0f
	} };
}

/* creates a 4 dimension matrix for orthographic projection onto a 2D plane. */
__forceinline mat4 co_orthographic_proj(float right, float left, float top, float bottom, float near, float far)
{
	return mat4{ {
		2.0f / (right - left), 0.0f, 0.0f, - (right + left) / (right-left),
		0.0f, 2.0f/(top - bottom), 0.0f, - (top + bottom) / (top - bottom),
		0.0f, 0.0f, - 2.0f / (far - near), - (far + near) / (far -  near),
		0.0f, 0.0f, 0.f, 1.0f
	} };
}

/* creates a 4 dimension matrix for orthographic projection onto a 2D plane. */
__forceinline mat4 ro_orthographic_proj(float right, float left, float top, float bottom, float near, float far)
{
	return mat4{ {
		2.0f / (right - left), 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
		0.0f, 0.0f, -2.0f / (far - near), 0.0f,
		 -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f
	} };
}


/* creates the transposed version of a 4 dimension matrix given in parameter. */
__forceinline mat4 transpose(const mat4& mat)
{
	return mat4{ {
		mat[0], mat[4], mat[8], mat[12],
		mat[1], mat[5], mat[9], mat[13],
		mat[2], mat[6], mat[10],mat[14],
		mat[3], mat[7], mat[11], mat[15]
	} };
}

/* multiplies two matrices with each other, and returns the result. (lhs row, rhs column) */
__forceinline mat4 mult(const mat4& lhs, const mat4& rhs)
{
	return mat4{ {
		lhs[0] * rhs[0] + lhs[1] * rhs[4] + lhs[2] * rhs[8] + lhs[3] * rhs[12], lhs[0] * rhs[1] + lhs[1] * rhs[5] + lhs[2] * rhs[9] + lhs[3] * rhs[13], lhs[0] * rhs[2] + lhs[1] * rhs[6] + lhs[2] * rhs[10] + lhs[3] * rhs[14], lhs[0] * rhs[3] + lhs[1] * rhs[7] + lhs[2] * rhs[11] + lhs[3] * rhs[15],
		lhs[4] * rhs[0] + lhs[5] * rhs[4] + lhs[6] * rhs[8] + lhs[7] * rhs[12], lhs[4] * rhs[1] + lhs[5] * rhs[5] + lhs[6] * rhs[9] + lhs[7] * rhs[13], lhs[4] * rhs[2] + lhs[5] * rhs[6] + lhs[6] * rhs[10] + lhs[7] * rhs[14], lhs[4] * rhs[3] + lhs[5] * rhs[7] + lhs[6] * rhs[11] + lhs[7] * rhs[15],
		lhs[8] * rhs[0] + lhs[9] * rhs[4] + lhs[10] * rhs[8] + lhs[11] * rhs[12], lhs[8] * rhs[1] + lhs[9] * rhs[5] + lhs[10] * rhs[9] + lhs[11] * rhs[13], lhs[8] * rhs[2] + lhs[9] * rhs[6] + lhs[10] * rhs[10] + lhs[11] * rhs[14], lhs[8] * rhs[3] + lhs[9] * rhs[7] + lhs[10] * rhs[11] + lhs[11] * rhs[15],
		lhs[12] * rhs[0] + lhs[13] * rhs[4] + lhs[14] * rhs[8] + lhs[15] * rhs[12], lhs[12] * rhs[1] + lhs[13] * rhs[5] + lhs[14] * rhs[9] + lhs[15] * rhs[13], lhs[12] * rhs[2] + lhs[13] * rhs[6] + lhs[14] * rhs[10] + lhs[15] * rhs[14], lhs[12] * rhs[3] + lhs[13] * rhs[7] + lhs[14] * rhs[11] + lhs[15] * rhs[15]
	} };
}

/* multiplies two matrices with each other, and returns the result. (lhs row, rhs column) */
__forceinline mat4 operator*(const mat4& lhs, const mat4& rhs)
{
	return mult(lhs, rhs);
}




#endif //__MATHS_H__
