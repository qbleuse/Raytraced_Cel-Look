#ifndef __SERIALIZATION_HELPER_H__
#define __SERIALIZATION_HELPER_H__

#include "rapidjson/document.h"
#include "Define.h"


#define SAFE_LOAD_VALUE(var)\
	if (!object.HasMember(name))\
		return;\
	const rapidjson::Value& var = object[name]; \


namespace SerializationHelper
{

	/*==== Vectors ====*/

	//serialize maths vector
	template<typename T, typename Allocator>
	__forceinline void SerializeVector(const char* name, rapidjson::Value& object, const T& vector, Allocator& allocator)
	{
		rapidjson::Value VectorValue(rapidjson::kArrayType);

		//first reserve the needed size for the vector
		VectorValue.Reserve(sizeof(T), allocator);

		//compute the number of float to write
		uint8_t nb = sizeof(T) / sizeof(float);
		for (uint8_t i = 0; i < nb; i++)
			VectorValue.PushBack(vector.scalar[i], allocator);
		object.AddMember(rapidjson::StringRef(name), VectorValue, allocator);
	}

	//Load maths vector from json
	template<typename T>
	__forceinline void LoadVector(const char* name, const rapidjson::Value& object, T& vector)
	{
		//get the vector
		SAFE_LOAD_VALUE(Vector);

		//get our array in the vector
		if (!Vector.IsArray())
			return;

		//go in the array and copy
		rapidjson::Value::ConstArray& Scalar = Vector.GetArray();
		for (uint8_t i = 0; i < Scalar.Size(); i++)
			vector.scalar[i] = Scalar[i].GetFloat();
	}

	/*==== Transform ====*/

	//serialize transform struct
	template<typename Allocator>
	__forceinline void SerializeTransform(const char* name, rapidjson::Value& object, const Transform& transform, Allocator& allocator)
	{
		//create a new "transform" object
		rapidjson::Value TransformValue(rapidjson::kObjectType);

		//serialize each part of the transform
		SerializeVector("Pos", TransformValue, transform.pos, allocator);
		SerializeVector("Rot", TransformValue, transform.rot, allocator);
		SerializeVector("Scale", TransformValue, transform.scale, allocator);


		//add the transform object to the whole object
		object.AddMember(rapidjson::StringRef(name), TransformValue, allocator);
	}

	//Load transform from json
	__forceinline void LoadTransform(const char* name, const rapidjson::Value& object, Transform& transform)
	{
		//get the transform
		SAFE_LOAD_VALUE(TransformValue);

		//does it have the transform objecct?
		if (!TransformValue.IsObject())
			return;

		//get back every part of the transform
		LoadVector("Pos", TransformValue, transform.pos);
		LoadVector("Rot", TransformValue, transform.rot);
		LoadVector("Scale", TransformValue, transform.scale);
	}


	/*==== Raytracing Params ====*/

	//serialize raytracing params struct
	template<typename Allocator>
	__forceinline void SerializRaytracingParams(const char* name, rapidjson::Value& object, const RaytracingParams& raytracingParams, Allocator& allocator)
	{
		//create a new object
		rapidjson::Value RaytracingParamsValue(rapidjson::kObjectType);

		//add ray specific params
		RaytracingParamsValue.AddMember("SampleNb", raytracingParams._pixel_sample_nb, allocator);
		RaytracingParamsValue.AddMember("MaxDepth", raytracingParams._max_depth, allocator);

		//serialize background gradient colours
		SerializeVector("Top Gradient", RaytracingParamsValue, raytracingParams._background_gradient_top, allocator);
		SerializeVector("Bottom Gradient", RaytracingParamsValue, raytracingParams._background_gradient_bottom, allocator);

		//add the raytracing params object to the whole object
		object.AddMember(rapidjson::StringRef(name), RaytracingParamsValue, allocator);
	}

	//Load raytracing params from json
	__forceinline void LoadRaytracingParams(const char* name, const rapidjson::Value& object, RaytracingParams& raytracingParams)
	{
		//get the raytracing params
		SAFE_LOAD_VALUE(RaytracingParamsValue);

		//does it have the transform objecct?
		if (!RaytracingParamsValue.IsObject())
			return;

		if (RaytracingParamsValue.HasMember("SampleNb"))
			raytracingParams._pixel_sample_nb = RaytracingParamsValue["SampleNb"].GetUint();

		if (RaytracingParamsValue.HasMember("MaxDepth"))
			raytracingParams._max_depth = RaytracingParamsValue["MaxDepth"].GetUint();

		//get back gradient
		LoadVector("Top Gradient", RaytracingParamsValue, raytracingParams._background_gradient_top);
		LoadVector("Bottom Gradient", RaytracingParamsValue, raytracingParams._background_gradient_bottom);
	}

	/*==== CelParams ====*/

	//serialize cel-shading params struct
	template<typename Allocator>
	__forceinline void SerializeCelParams(const char* name, rapidjson::Value& object, const CelParams& celParams, Allocator& allocator)
	{
		//create a new object
		rapidjson::Value CelParamsValue(rapidjson::kObjectType);

		//serialize Cel shading steps
		SerializeVector("Diffuse Steps", CelParamsValue, celParams._celDiffuseStep, allocator);
		SerializeVector("Specular Steps", CelParamsValue, celParams._celSpecStep, allocator);

		//setialize specular glosssiness power based on blinn phong
		CelParamsValue.AddMember("Glossiness", celParams._specGlossiness, allocator);

		//add the transform object to the whole object
		object.AddMember(rapidjson::StringRef(name), CelParamsValue, allocator);
	}

	//Load cel-shading params from json
	__forceinline void LoadCelParams(const char* name, const rapidjson::Value& object, CelParams& celParams)
	{
		//get the raytracing params
		SAFE_LOAD_VALUE(CelParamsValue);

		//does it have the transform objecct?
		if (!CelParamsValue.IsObject())
			return;

		//get cel steps
		LoadVector("Diffuse Steps", CelParamsValue, celParams._celDiffuseStep);
		LoadVector("Specular Steps", CelParamsValue, celParams._celSpecStep);
	
		//get glossiness power
		if (CelParamsValue.HasMember("Glossiness"))
			celParams._specGlossiness = CelParamsValue["Glossiness"].GetFloat();
	}

	/*==== Light ====*/

	//serialize light struct
	template<typename Allocator>
	__forceinline void SerializeLight(const char* name, rapidjson::Value& object, const Light& light, Allocator& allocator)
	{
		//create a new object
		rapidjson::Value LightValue(rapidjson::kObjectType);


		//serialize the light type ofg this light
		LightValue.AddMember("Light Type", (int)light._LightType, allocator);

		//Note : makes it easier to read it in the file, but genually adds unnecessary work in code
		switch (light._LightType)
		{
		case LightType::DIRECTIONNAL:
			SerializeVector("Light Dir", LightValue, light._dir, allocator);
			break;
		case LightType::SPHERE:
			SerializeVector("Light Pos", LightValue, light._pos, allocator);
			LightValue.AddMember("Light Radius", light._radius, allocator);
			break;
		default:
			break;
		}

		SerializeVector("Light Colour", LightValue, light._color, allocator);

		//add the transform object to the whole object
		object.AddMember(rapidjson::StringRef(name), LightValue, allocator);
	}

	//Load cel-shading params from json
	__forceinline void LoadLight(const char* name, const rapidjson::Value& object, Light& light)
	{
		//get the raytracing params
		SAFE_LOAD_VALUE(LightValue);

		//does it have the transform objecct?
		if (!LightValue.IsObject())
			return;

		//get the type of the light to know how wer should read it
		if (LightValue.HasMember("Light Type"))
			light._LightType = (LightType)LightValue["Light Type"].GetInt();

		//Note : makes it easier to read it in the file, but genually adds unnecessary work in code
		switch (light._LightType)
		{
		case LightType::DIRECTIONNAL:
			LoadVector("Light Dir", LightValue, light._dir);
			break;
		case LightType::SPHERE:
			LoadVector("Light Pos", LightValue, light._pos);
			if (LightValue.HasMember("Light Radius"))
				light._radius = LightValue["Light Radius"].GetFloat();
			break;
		default:
			break;
		}

		LoadVector("Light Colour", LightValue, light._color);
	}

}




#endif //__SERIALIZATION_HELPER_H__