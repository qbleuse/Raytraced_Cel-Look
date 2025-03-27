#ifndef __DEFINE_H__
#define __DEFINE_H__


/* 
 * This file will declare or define thing that most file should know and we'll be included most anywhere. 
 * Thisis to allow quick access and changes of hardcoded values.
 */
#include "Maths.h"


#ifndef _WIN32
#define __forceinline inline
#endif

/*==== SERIALIZATION ====*/

#define JSON_FILE_RW_BUFFER 2048


/*==== TRANSFORM ====*/

//Transform UI MAX VALUES
#define MAX_TRANSLATE 100.0f
#define MAX_ROT 180.0f
#define MAX_SCALE 10.0f

/*==== RAYTRACING PARAMS ====*/


//Raytracing Params Init Values
#define PIXEL_SAMPLE_NB 5
#define BACKGROUND_GRADIENT_TOP vec4{ 0.3f, 0.7f, 1.0f, 1.0f }
#define BACKGROUND_GRADIENT_BOTTOM vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define BOUNCE_DEPTH 10

//Raytracing CPU Params
#define MAX_CPU_COMPUTE_PER_FRAMES 25000


struct RaytracingParams
{
	//the color for the top of the background gradient 
	vec4		_background_gradient_top{ BACKGROUND_GRADIENT_TOP };
	//the color for the bottom of the background gradient 
	vec4		_background_gradient_bottom{ BACKGROUND_GRADIENT_BOTTOM };
	//the number of sample for a single pixel
	uint32_t	_pixel_sample_nb{ PIXEL_SAMPLE_NB };
	//the maximum ray bounce generation allowed
	uint32_t	_max_depth{ BOUNCE_DEPTH };
};

/*==== LIGHT PARAMS ====*/

//enum to get the type of light currently processed
enum class LightType
{
	DIRECTIONNAL = 0,
	SPHERE = 1,


	NB
};

//Light Init Values

#define INIT_LIGHT_DIR vec3( 1.0f, -1.0f, 1.0f)
#define INIT_LIGHT_COLOR vec3( 1.0f,1.0f,1.0f )
#define INIT_AO 0.05f

#define INIT_DIR_LIGHT {INIT_LIGHT_DIR, 0.0f, INIT_LIGHT_COLOR, LightType::DIRECTIONNAL}

//Light UI MAX MIN
#define LIGHT_DIR_EDIT_MAX 1.0f
#define LIGHT_DIR_EDIT_MIN -1.0f
#define LIGHT_POS_EDIT_MAX 100.0f
#define LIGHT_RADIUS_EDIT_MAX 100.0f

//a struct that can be used for all types of lights
struct Light
{
	union
	{
		//Directionnal Light
		struct
		{
			vec3		_dir;
			float		_padding;
		};

		//Sphere Light
		struct
		{
			vec3		_pos;
			float		_radius;
		};
	};


	vec3		_color;
	LightType	_LightType;
};


/*==== SHADING PARAMS ====*/

//the init params for the cel-shaded material
#define INIT_CEL_DIFFUSE_STEP { 0.0f, 0.01f } 
#define INIT_CEL_SPECULAR_STEP { 0.005f, 0.01f }
#define INIT_CEL_SPECULAR_GLOSS 8.0f

//Cel Parameters MAX Edit Values
#define MAX_CEL_SPECULAR_GLOSS 64.0f
#define MAX_CEL_DIFFUSE_STEP 1.0f
#define MAX_CEL_SPECULAR_STEP 1.0f


//struct represetnign the parameters needed for a cel shaded material
struct CelParams
{
	//cel Shading Diffuse Step
	vec2	_celDiffuseStep INIT_CEL_DIFFUSE_STEP;
	//cel shading spec Step
	vec2	_celSpecStep INIT_CEL_SPECULAR_STEP;
	//specular Glossiness
	float	_specGlossiness{ INIT_CEL_SPECULAR_GLOSS };
};



#endif //__DEFINE_H__