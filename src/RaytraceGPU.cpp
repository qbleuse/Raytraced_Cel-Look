#include "RaytraceGPU.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

#define NUMBER_OF_SPHERES 30

/*==== Prepare =====*/

void RaytraceGPU::PrepareVulkanRaytracingProps(GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	/*===== MODEL LOADING & AS BUILDING ======*/
	{
		//load the model
		VulkanHelper::LoadObjFile(GAPI._VulkanUploader, "../../../media/teapot/teapot.obj", _RayModel._Meshes);

		//create bottom level AS from model
		VulkanHelper::CreateRaytracedGeometryFromMesh(GAPI._VulkanUploader, _RayBottomAS, _RayModel._Meshes, VK_NULL_HANDLE);

		VulkanHelper::CreateSceneBufferFromMeshes(GAPI._VulkanUploader, _RaySceneBuffer, _RayModel._Meshes);

		VulkanHelper::RaytracedGeometry* bottomAS[2] = { &_RayBottomAS , &_RaySphereBottomAS };
		VulkanHelper::CreateRaytracedGroupFromGeometry(GAPI._VulkanUploader, _RayTopAS, identity(), bottomAS, 2);

	}
	/*===== DESCRIBE SHADER STAGE AND GROUPS =====*/

	//the shader stages we'll give to the pipeline
	MultipleScopedMemory<VkPipelineShaderStageCreateInfo> shaderStages;
	VulkanHelper::MakePipelineShaderFromScripts(shaderStages, _RayShaders);


	MultipleVolatileMemory<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{ static_cast<VkRayTracingShaderGroupCreateInfoKHR*>(alloca(sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 7)) };
	memset(*shaderGroups, 0, sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 7);

	//describing our raygen group, it only includes the first shader of our pipeline
	shaderGroups[0].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[0].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[0].generalShader		= 0;
	shaderGroups[0].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[0].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[0].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our miss group, it only includes the second shader of our pipeline, as we have not described multiple miss
	shaderGroups[1].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[1].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[1].generalShader		= 1;
	shaderGroups[1].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[1].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[1].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our hit group, it only includes the third shader of our pipeline, as we have not described multiple hits
	shaderGroups[3].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[3].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	shaderGroups[3].generalShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[3].closestHitShader	= 3;
	shaderGroups[3].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[3].intersectionShader	= 4;

	//describing our hit group, it only includes the third shader of our pipeline, as we have not described multiple hits
	shaderGroups[2].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[2].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroups[2].generalShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].closestHitShader	= 2;
	shaderGroups[2].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our callable diffuse group
	shaderGroups[4].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[4].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[4].generalShader		= 5;
	shaderGroups[4].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[4].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[4].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our callable metal group
	shaderGroups[5].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[5].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[5].generalShader		= 6;
	shaderGroups[5].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[5].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[5].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our callable metal group
	shaderGroups[6].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[6].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[6].generalShader		= 7;
	shaderGroups[6].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[6].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[6].intersectionShader	= VK_SHADER_UNUSED_KHR;

	/*===== PIPELINE ATTACHEMENT =====*/

	{
		VkDescriptorSetLayoutBinding layoutStaticBinding[8] =
		{
			//binding , descriptor type, descriptor count, shader stage
			{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},//acceleration structure
			{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR},//sphere buffer
			{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},//sphere colour buffer
			{ 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR},//sphere offset buffer
			{ 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},//scene buffer offset
			{ 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},//scene buffer indices
			{ 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},//scene buffer uvs
			{ 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}//scene buffer normals
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, layoutStaticBinding, 8);

		VkDescriptorSetLayoutBinding layoutDynamicBinding[2] =
		{
			//binding , descriptor type, descriptor count, shader stage
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},//framebuffer
			{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},//uniform global buffer
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _RayPipelineDynamicDescriptor, layoutDynamicBinding, 2);
	}

	{
		//our pipeline layout is the same as the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkDescriptorSetLayout layouts[2] = { _RayPipelineStaticDescriptor._DescriptorLayout,_RayPipelineDynamicDescriptor._DescriptorLayout };
		pipelineLayoutInfo.pSetLayouts		= layouts;
		pipelineLayoutInfo.setLayoutCount	= 2;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_RayLayout));
	}

	//fill the static descriptors
	{
		/*===== LINKING RESOURCES ======*/

		//allocating a single set that will be used throughout
		VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, 1);
		
		//Adding our Top Level AS in each descriptor set
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RayTopAS._AccelerationStructure, 0);

		//bind spheres buffers to each descriptor
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySphereBuffer._StaticGPUBuffer, 0, sizeof(vec4) * NUMBER_OF_SPHERES, 3);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySphereColour._StaticGPUBuffer, 0, sizeof(vec4) * NUMBER_OF_SPHERES, 4);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySphereOffsets._StaticGPUBuffer, 0, sizeof(uint32_t) * 3, 5);//3 offset spheres are separated depending on their material

		//bind scene buffers to each descriptor
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySceneBuffer._OffsetBuffer._StaticGPUBuffer, 0, _RaySceneBuffer._OffsetBufferSize, 6);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySceneBuffer._IndexBuffer._StaticGPUBuffer, 0, _RaySceneBuffer._IndexBufferSize, 7);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySceneBuffer._UVsBuffer._StaticGPUBuffer, 0, _RaySceneBuffer._UVsBufferSize, 8);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineStaticDescriptor, _RaySceneBuffer._NormalBuffer._StaticGPUBuffer, 0, _RaySceneBuffer._NormalBufferSize, 9);
		
	}

	/*===== PIPELINE CREATION =====*/

	//our raytracing pipeline
	VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
	pipelineInfo.sType							= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.stageCount						= _RayShaders.Nb();
	pipelineInfo.pStages						= *shaderStages;
	pipelineInfo.groupCount						= 7;
	pipelineInfo.pGroups						= *shaderGroups;
	pipelineInfo.maxPipelineRayRecursionDepth	= 1;//for the moment we'll hardcode the value, we'll see if we'll make it editable
	pipelineInfo.layout							= _RayLayout;//describe the attachement

	VK_CALL_KHR(GAPI._VulkanDevice, vkCreateRayTracingPipelinesKHR, GAPI._VulkanDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_RayPipeline);


	VulkanHelper::GetShaderBindingTable(GAPI._VulkanUploader, _RayPipeline, _RayShaderBindingTable, 1, 2, 3);
}

void RaytraceGPU::PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI)
{
	//define ray gen shader
	const char* ray_gen_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			struct HitRecord 
			{
				float	bHit;//a float that acts as a boolean to know if it hit or missed, also used for padding
				vec3	hitColor;// the color of the hit object
				float	hitDistance;// the distance from the ray's origin the hit was recorded
				vec3	hitNormal;// the normal got from contact with the object
				uint	RandomSeed;
				
			};
			layout(location = 0) rayPayloadEXT HitRecord payload;

			uint RandomInt(inout uint seed)
			{
				//using PCG hash for random number seed generation (found from this https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/)
				seed = seed * 747796405u + 2891336453u;
				uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
				return (word >> 22u) ^ word;

				// LCG values from Numerical Recipes
			    //return (seed = 1664525 * seed + 1013904223);
			}

			float RandomFloat(inout uint seed)
			{
				//// Float version using bitmask from Numerical Recipes
				//const uint one = 0x3f800000;
				//const uint msk = 0x007fffff;
				//return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
			
				// Faster version from NVIDIA examples; quality good enough for our use case.
				return (float(RandomInt(seed) & 0x00FFFFFF) / float(0x01000000));
			}

			layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
			layout(set = 1, binding = 0, rgba32f) uniform image2D image;
			layout(set = 1, binding = 1) uniform UniformBuffer
			{
				mat4 view;
				mat4 proj;
				vec4 background_color_top;
				vec4 background_color_bottom;
				uint nb_samples;
				uint depth;
				uint nb_frame;
				float random;
			};

			void main()
			{
				vec3 finalColor = vec3(0.0);

				uint seed = 1;
				payload.RandomSeed = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.y);
				payload.RandomSeed = RandomInt(payload.RandomSeed);

				for (int j = 0; j < nb_samples; j++)
				{
					const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(RandomFloat(seed) - 0.5,RandomFloat(seed) - 0.5);

					//finding the pixel we are computing from the ray launch arguments
					const vec2 inUV = (pixelCenter) / vec2(gl_LaunchSizeEXT.xy);

					vec2 d = inUV * 2.0 - 1.0;

					vec4 origin = -view[3];
					//the targets are on a viewport ahead of us by 3
					vec4 target = proj * vec4(d,1.0,1.0);
					target = target/target.w;

					//creating a direction for our ray
					vec4 direction = view * vec4(normalize(target.xyz),0.0);

					//zero init
					payload.bHit		= 0;
					payload.hitColor	= vec3(0.0);
					payload.hitDistance = 0.0;
					payload.hitNormal	= vec3(0.0);

					vec3 fragColor = vec3(1.0);

					for (int i = 0; i < depth; i++)
					{
						//tracing rays
						traceRayEXT(
							topLevelAS,//Acceleration structure
							gl_RayFlagsOpaqueEXT,//opaque flags (definelty don't want it forever)
							0xff,//cull mask
							0,0,0,//shader binding table offset, stride, and miss index (as this function can be called from all hit shader)
							origin.xyz,//our camera's origin
							0.001,//our collision epsilon
							direction.xyz,//the direction of our ray
							10000.0,//the max length of our ray (this could be changed if we added energy conservation rules, I think, but in our case , it is more or less infinity)
							0
						);

						fragColor *= vec3(payload.hitColor);

						if (payload.hitDistance < 0.0 || payload.bHit == 0.0)
							break;


						origin		+= direction * payload.hitDistance;
						direction	= vec4(payload.hitNormal,0.0);
					}

					finalColor += fragColor;
				}

				finalColor /= nb_samples;
				
				//our recursive call finished, let's write into the image
				imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(finalColor,0.0));

			})";

	//define miss shader
	const char* miss_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			struct HitRecord 
			{
				float	bHit;//a float that acts as a boolean to know if it hit or missed, also used for padding
				vec3	hitColor;// the color of the hit object
				float	hitDistance;// the distance from the ray's origin the hit was recorded
				vec3	hitNormal;// the normal got from contact with the object
				uint	RandomSeed;
				
			};
			layout(location = 0) rayPayloadInEXT HitRecord payload;

			layout(set = 1, binding = 1) uniform UniformBuffer
			{
				mat4 view;
				mat4 proj;
				vec4 background_color_top;
				vec4 background_color_bottom;
				uint nb_samples;
				uint depth;
				uint nb_frame;
				uint random;
			};

			void main()
			{
				float backgroundGradient = 0.5 * (gl_WorldRayDirectionEXT.y + 1.0);

				payload.bHit = 0.0;
				payload.hitColor = background_color_bottom.rgb * (1.0 - backgroundGradient) + background_color_top.rgb * backgroundGradient;
				payload.hitDistance = 0.0;
				payload.hitNormal = vec3(0.0);
			})";

	//define closest hit shader
	const char* closest_hit_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable


			struct HitRecord 
			{
				float	bHit;//a float that acts as a boolean to know if it hit or missed, also used for padding
				vec3	hitColor;// the color of the hit object
				float	hitDistance;// the distance from the ray's origin the hit was recorded
				vec3	hitNormal;// the normal got from contact with the object
				uint	RandomSeed;
				
			};
			layout(location = 0) rayPayloadInEXT HitRecord payload;
			struct RayDispatch
			{
				vec3	direction;// the original directio of the hit 
				vec3	normal;// the normal got from contact with the object
				float	matInfo;
				uint	RandomSeed;
			};
			layout(location = 1) callableDataEXT RayDispatch callablePayload;

			layout(binding = 4) readonly buffer SphereArray { vec4[] SphereColour; };
			layout(binding = 5) readonly buffer SphereOffset{ uint[3] SphereOffsets; };
			hitAttributeEXT vec4 attribs;

			void main()
			{
				const vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
				vec3 normal = (hitPoint - attribs.xyz) / attribs.w;

				payload.bHit		= 1.0;
				vec4 colour			= SphereColour[gl_PrimitiveID + SphereOffsets[gl_InstanceCustomIndexEXT]];
				payload.hitColor	= colour.rgb;
				payload.hitDistance = gl_HitTEXT;

				callablePayload.direction	= gl_WorldRayDirectionEXT;
				callablePayload.normal		= normal;
				callablePayload.matInfo		= colour.w;
				callablePayload.RandomSeed	= payload.RandomSeed;
				executeCallableEXT(gl_InstanceCustomIndexEXT, 1);
				payload.hitNormal	= callablePayload.normal;
				payload.RandomSeed	= callablePayload.RandomSeed;



			})";

	//define traingle closest hit shader
	const char* trinagle_closest_hit_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable


			struct HitRecord 
			{
				float	bHit;//a float that acts as a boolean to know if it hit or missed, also used for padding
				vec3	hitColor;// the color of the hit object
				float	hitDistance;// the distance from the ray's origin the hit was recorded
				vec3	hitNormal;// the normal got from contact with the object
				uint	RandomSeed;
				
			};
			layout(location = 0) rayPayloadInEXT HitRecord payload;
			struct RayDispatch
			{
				vec3	direction;// the original directio of the hit 
				vec3	normal;// the normal got from contact with the object
				float	matInfo;
				uint	RandomSeed;
				
			};
			layout(location = 1) callableDataEXT RayDispatch callablePayload;

			layout(binding = 6) readonly buffer OffsetBuffer { uint[] Offsets; };
			layout(binding = 7) readonly buffer IndexBuffer { uint[] Indices; };
			layout(binding = 8) readonly buffer UVBuffer { vec2[] UVs; };
			layout(binding = 9) readonly buffer NormalBuffer { float[] Normals; };

			hitAttributeEXT vec2 barycentric;

			void main()
			{
				const vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

				const uint indexOffset = Offsets[gl_InstanceID * 3 + 0];
				const uint normalOffset = Offsets[gl_InstanceID * 3 + 2];


				const uint n1_offset = (Indices[indexOffset + gl_PrimitiveID * 3 + 0] + normalOffset) * 3;
				const uint n2_offset = (Indices[indexOffset + gl_PrimitiveID * 3 + 1] + normalOffset) * 3;
				const uint n3_offset = (Indices[indexOffset + gl_PrimitiveID * 3 + 2] + normalOffset) * 3;


				const vec3 N1 = vec3(Normals[n1_offset + 0], Normals[n1_offset + 1], Normals[n1_offset + 2]);
				const vec3 N2 = vec3(Normals[n2_offset + 0], Normals[n2_offset + 1], Normals[n2_offset + 2]);
				const vec3 N3 = vec3(Normals[n3_offset + 0], Normals[n3_offset + 1], Normals[n3_offset + 2]);

				const vec3 coordinates = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);

				vec3 normal = gl_ObjectToWorldEXT * vec4(normalize(N1 * coordinates.x + N2 * coordinates.y + N3 * coordinates.z),0.0);

				payload.bHit		= 1.0;
				payload.hitColor	= vec3(1.0);
				payload.hitDistance = gl_HitTEXT;

				callablePayload.direction	= gl_WorldRayDirectionEXT;
				callablePayload.normal		= normal;
				callablePayload.matInfo		= 0.0;
				callablePayload.RandomSeed	= payload.RandomSeed;
				executeCallableEXT(gl_InstanceCustomIndexEXT, 1);
				payload.hitNormal	= callablePayload.normal;
				payload.RandomSeed	= callablePayload.RandomSeed;

			})";

	//define intersect shader
	const char* intersect_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			#define HIT_EPSILON 0.01f

			hitAttributeEXT vec4 attribs;
			layout(binding = 3) readonly buffer SphereArray { vec4[] Spheres; };
			layout(binding = 5) readonly buffer SphereOffset{ uint[3] SphereOffsets; };

			void main()
			{
				const vec4 sphere = Spheres[gl_PrimitiveID + SphereOffsets[gl_InstanceCustomIndexEXT]];

				const vec3 ray_to_center	= sphere.xyz - gl_WorldRayOriginEXT;
				const vec3 direction		= gl_WorldRayDirectionEXT;
				const float radius			= sphere.w;
		
				//information on how the formula works can be found here : https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
				//this is a basic quadratic equation.
				//for the used formula here, it is the simplified one from here : https://raytracing.github.io/books/RayTracingInOneWeekend.html#surfacenormalsandmultipleobjects/simplifyingtheray-sphereintersectioncode
		
				//in my case, because ray direction are normalized, a == 1, so I'll move it out
				float a = dot(direction, direction);
		
				//h in geometrical terms is the distance from the ray's origin to the sphere's center
				//projected onto the ray's direction.
				float h = dot(direction, ray_to_center);
		
				//this is basically the squared distance between the ray's origin and the sphere's center
				//substracted to the sphere's squared radius.
				float c = dot(ray_to_center, ray_to_center) - (radius * radius);
		
				//a == 1 so it is not h^2 - ac but h^2 - c
				float discriminant = (h * h) - (a * c);
		
				//we're not in a complex plane, so sqrt(-1) is impossible
				if (discriminant <= 0.0f)
					return;
		
				//finding the roots
				float sqrt_discriminant = sqrt(discriminant);
		
				float root_1 = (h + sqrt_discriminant) / (a);
				float root_2 = (h - sqrt_discriminant) / (a);
		
				if (root_1 < HIT_EPSILON && root_2 < HIT_EPSILON)
					return;
		
				//choosing our closest root
				float distance		= min(root_1,root_2);
				distance			= distance < HIT_EPSILON ? max(root_1, root_2) : distance;//avoid considering a collision behind the camera
				attribs = sphere;
				reportIntersectionEXT(distance,0);

			})";

	//define diffuse shader
	const char* diffuse_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			uint RandomInt(inout uint seed)
			{
				//using PCG hash for random number seed generation (found from this https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/)
				seed = seed * 747796405u + 2891336453u;
				uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
				return (word >> 22u) ^ word;

				// LCG values from Numerical Recipes
			    //return (seed = 1664525 * seed + 1013904223);
			}

			float RandomFloat(inout uint seed)
			{
				//// Float version using bitmask from Numerical Recipes
				//const uint one = 0x3f800000;
				//const uint msk = 0x007fffff;
				//return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
			
				// Faster version from NVIDIA examples; quality good enough for our use case.
				return (float(RandomInt(seed) & 0x00FFFFFF) / float(0x01000000));
			}
			
			
			
			vec3 RandomInUnitSphere(inout uint seed)
			{
				for (;;)
				{
					const vec3 p = 2 * vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1;
					if (dot(p, p) < 1)
					{
						return p;
					}
				}
			}

			struct RayDispatch
			{
				vec3	direction;// the original directio of the hit 
				vec3	normal;// the normal got from contact with the object
				float	matInfo;
				uint	RandomSeed;
				
			};
			layout(location = 1) callableDataInEXT RayDispatch callablePayload;

			void main()
			{
				const vec3 rand = RandomInUnitSphere(callablePayload.RandomSeed);
				callablePayload.normal = normalize(callablePayload.normal + rand);

			})";

	//define metal shader
	const char* metal_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			struct RayDispatch
			{
				vec3	direction;// the original directio of the hit 
				vec3	normal;// the normal got from contact with the object
				float	matInfo;
				uint	RandomSeed;
				
			};
			layout(location = 1) callableDataInEXT RayDispatch callablePayload;

			void main()
			{
				vec3 direction = callablePayload.direction;

				//are we in the object or outside ?
				bool front_face = dot(callablePayload.normal, direction) < 0.0f;
				//normal is given inside-out, should be reversed if not front
				vec3 normal = front_face ? callablePayload.normal : -callablePayload.normal;

				callablePayload.normal = normalize(direction - (normal * 2.0f * dot(direction, normal)));

			})";

	//define dielectrics shader
	const char* dieletctrics_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			uint RandomInt(inout uint seed)
			{
				//using PCG hash for random number seed generation (found from this https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/)
				seed = seed * 747796405u + 2891336453u;
				uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
				return (word >> 22u) ^ word;

				// LCG values from Numerical Recipes
			    //return (seed = 1664525 * seed + 1013904223);
			}

			float RandomFloat(inout uint seed)
			{
				//// Float version using bitmask from Numerical Recipes
				//const uint one = 0x3f800000;
				//const uint msk = 0x007fffff;
				//return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
			
				// Faster version from NVIDIA examples; quality good enough for our use case.
				return (float(RandomInt(seed) & 0x00FFFFFF) / float(0x01000000));
			}

			struct RayDispatch
			{
				vec3	direction;// the original directio of the hit 
				vec3	normal;// the normal got from contact with the object
				float	matInfo;
				uint	RandomSeed;
				
			};
			layout(location = 1) callableDataInEXT RayDispatch callablePayload;

			void main()
			{
				vec3 direction = callablePayload.direction;
				vec3 normal = callablePayload.normal;

				//are we in the object or outside ?
				bool front_face		= dot(normal, direction) < 0.0f;
				//normal is given inside-out, should be reversed if not front
				normal			= front_face ? normal : -normal;
				//the refractive index between the material (changes if we are inside or outside the object)
				float refract_index = front_face ? (1.0f / callablePayload.matInfo) : callablePayload.matInfo;


				//the cos angle between the ray and the normal
				float cos_ray_normal = min(dot(-direction, normal), 1.0f);
				//from trigonometry property -> cos^2 + sin^2 = 1, we get sin of angle
				float sin_ray_normal = sqrt(1.0 - (cos_ray_normal * cos_ray_normal));

				//this is the result of snell's law, a hard rule for which ray should refract or not.
				bool cannot_refract = refract_index * sin_ray_normal > 1.0f;

				//as snell's law is not really enough to properly represent refraction, 
				//we'll also use Schlick's "reflectance", which is a quatitative approximation of the actual refraction law 
				float reflectance = (1.0f - refract_index) / (1.0f + refract_index);
				reflectance *= reflectance;
				reflectance = reflectance + (1.0f * reflectance) * pow((1.0f - cos_ray_normal), 5);

				//as snell's law is an approximation, it actually takes more of a probabilistic approach, thus the random.
				if (cannot_refract || reflectance > RandomFloat(callablePayload.RandomSeed) )
				{
					callablePayload.normal = normalize(direction - (normal * 2.0f * dot(direction, normal)));
				}
				else
				{	
					//the orthogonal component to our contact plane of our refracted ray
					vec3 orth_comp			= (direction + (normal * cos_ray_normal)) * refract_index;
					//the tangential component to our contact plane of our refracted ray
					vec3 tangent_comp		= normal * -sqrt(abs(1.0f - dot(orth_comp, orth_comp)));
					//the total refracted ray
					callablePayload.normal  = normalize(tangent_comp + orth_comp);
				}


			})";

	{
		VulkanHelper::ShaderScripts Script;
		
		//add raygen shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_RAYGEN_BIT_KHR, ray_gen_shader, "Raytrace GPU RayGen"))
			_RayShaders.Add(Script);

		//add miss shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_MISS_BIT_KHR, miss_shader, "Raytrace GPU Miss"))
			_RayShaders.Add(Script);

		//add hit shaders
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, trinagle_closest_hit_shader, "Raytrace GPU Triangle Closest"))
			_RayShaders.Add(Script);
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closest_hit_shader, "Raytrace GPU Sphere Closest"))
			_RayShaders.Add(Script);
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersect_shader, "Raytrace GPU Intersect"))
			_RayShaders.Add(Script);

		//add callable shaders
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_CALLABLE_BIT_KHR, diffuse_shader, "Raytrace GPU Diffuse"))
			_RayShaders.Add(Script);
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_CALLABLE_BIT_KHR, metal_shader, "Raytrace GPU Metal"))
			_RayShaders.Add(Script);
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_CALLABLE_BIT_KHR, dieletctrics_shader, "Raytrace GPU Dieletctrics"))
			_RayShaders.Add(Script);
	}
}


void RaytraceGPU::PrepareVulkanProps(GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	{
		/*===== FRAMEBUFFER OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription attachements[1] =
		{
			//flags, format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, inialLayout, finalLayout
			{0, GAPI._VulkanSurfaceFormat.format,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}
		};

		VulkanHelper::CreatePipelineOutput(GAPI._VulkanUploader, _CopyPipelineOutput, attachements, 1, {});
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_CopySampler);

		//the sampler and image to sample the CPU texture
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding			= 0;
		samplerLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount	= 1;
		samplerLayoutBinding.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &_CopySampler;

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _CopyDescriptors, &samplerLayoutBinding, 1);

	}

	{
		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_CopyDescriptors._DescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_CopyLayout));
	}

	CreateFullscreenCopyPipeline(GAPI, _CopyPipeline, _CopyLayout, _CopyPipelineOutput._OutputRenderPass, _CopyShaders);
}

void RaytraceGPU::CreateFullscreenCopyPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VkRenderPass& RenderPass, const List<VulkanHelper::ShaderScripts>& Shaders)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//the shader stages we'll give to the pipeline
	MultipleScopedMemory<VkPipelineShaderStageCreateInfo> shaderStages;
	VulkanHelper::MakePipelineShaderFromScripts(shaderStages, Shaders);

	/*===== INTPUT ASSEMBLY ======*/

	//to do a full screen, you actually do not need any vertex
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount	= 0;
	vertexInputInfo.pVertexBindingDescriptions		= nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions	= nullptr; // Optional

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	/*===== VIEWPORT/SCISSORS  =====*/

	//as window can be resized, I set viewport and scissors as dynamic so it is always the size of the output window
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 2;
	VkDynamicState viewportState[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStateInfo.pDynamicStates = viewportState;

	//setting our starting viewport
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount		= 1;
	viewportStateInfo.pViewports		= &_CopyPipelineOutput._OutputViewport;
	viewportStateInfo.scissorCount		= 1;
	viewportStateInfo.pScissors			= &_CopyPipelineOutput._OutputScissor;


	/*==== RASTERIZER =====*/

	//Here, our rasterizer actually does the conversion from the vertex created inthe vertex shader and pixel coordinates\
	//it is the same as always, and frankly does not do much 
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_NONE;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth		= 1.0f;

	/*===== MSAA =====*/

	//MSAA would be way overkill for what wee are doing, so just disabling it
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	/*===== FRAMEBUFFER BLEND ======*/

	//we'll write the texture as-is
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	//nothing crazy in colour blending, as we just want to show our CPU texture
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= nullptr;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.pStages				= *shaderStages;
	pipelineInfo.stageCount				= Shaders.Nb();
	pipelineInfo.layout					= PipelineLayout;
	pipelineInfo.renderPass				= RenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &Pipeline));
}

void RaytraceGPU::PrepareVulkanScripts(GraphicsAPIManager& GAPI)
{
	//define vertex shader
	const char* vertex_shader =
		R"(#version 450
			layout(location = 0) out vec2 uv;

			void main()
			{
				uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);				
				gl_Position =  vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);

			})";

	const char* fragment_shader =
		R"(#version 450
		layout(location = 0) in vec2 uv;
		layout(location = 0) out vec4 outColor;

		layout(set = 0, binding = 0) uniform sampler2D fullscreenTex;


		void main()
		{
			outColor = vec4(texture(fullscreenTex,uv).rgb,1.0);
		}
		)";


	{
		VulkanHelper::ShaderScripts Script;

		//add vertex shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex"))
			_CopyShaders.Add(Script);

		//add fragment shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag"))
			_CopyShaders.Add(Script);
	}
}

void RaytraceGPU::GenerateSpheres(GraphicsAPIManager& GAPI)
{
	// basically copy-paste of the CPU raytracing way of creating spheres

	uint32_t nbOfRandomSpheres = NUMBER_OF_SPHERES;
	uint32_t randomSphereIndex = nbOfRandomSpheres - 4;
	uint32_t nbOfSpherePerMat[3] = { 2, 1, 1 };//The 4 spheres are the ground and the three example one

	/*randomy generating the number of sphere each material should have*/
	for (uint32_t i = 1; i < 3; i++)
	{
		uint32_t originalNb = nbOfSpherePerMat[i];
		nbOfSpherePerMat[i] += static_cast<uint32_t>(randf(1.0f, randomSphereIndex));
		randomSphereIndex -= nbOfSpherePerMat[i];
	}
	nbOfSpherePerMat[0] += randomSphereIndex;

	/* the arrays for spheres and "color" */
	MultipleScopedMemory<vec4> spheres{ NUMBER_OF_SPHERES };
	MultipleScopedMemory<vec4> sphereColour{ NUMBER_OF_SPHERES };
	MultipleVolatileMemory<VkAabbPositionsKHR> AABBs{ NUMBER_OF_SPHERES };

	//init of the example sphere. the array will be split in 3, diffuse first, metal second, dieletrics third. 
	spheres[0] = vec4( 0.0f, -1000.0f, 0.0f, 1000.0f);//the ground
	spheres[1] = vec4( 0.0f, 5.0f, 0.0f, 5.0f);//a lambertian diffuse example
	spheres[nbOfSpherePerMat[0]] = vec4(10.0f, 5.0f, 0.0f, 5.0f);//a metal example
	spheres[nbOfSpherePerMat[0] + nbOfSpherePerMat[1]] = vec4(-10.0f, 5.0f, 0.0f, 5.0f);//a dieletric example (this is a glass sphere)

	sphereColour[0] = vec4{ 0.5f,0.5f,0.5f, 1.0f };//the ground material
	sphereColour[1] = vec4{ 0.4f, 0.2f, 0.1f, 1.0f };//a lambertian diffuse example
	sphereColour[nbOfSpherePerMat[0]] = vec4{ 0.7f, 0.6f, 0.5f, 1.0f };//a metal example
	sphereColour[nbOfSpherePerMat[0] + nbOfSpherePerMat[1]] = vec4{ 1.0f,1.0f,1.0f, 1.50f };//a dieletric example (this is a glass sphere)

	{
		uint32_t groupLength = 0;
		randomSphereIndex = 2;
		//we create sphere at random place with random materials for the demo
		for (uint32_t i = 0; i < 3; i++)
		{
			groupLength += nbOfSpherePerMat[i];

			for (; randomSphereIndex < groupLength; randomSphereIndex++)
			{
				float radius = 1.0f + randf() * 1.0f;
				vec3 sphereCenter = vec3{ (randf(-1.0f,1.0f)) * 20.0f, radius, (randf(-1.0f,1.0f)) * 20.0f };

				spheres[randomSphereIndex] = vec4(sphereCenter.x, sphereCenter.y, sphereCenter.z, radius);

				//very simple material. for most, albedo and glass coefficient for dieletrics
				sphereColour[randomSphereIndex] = vec4{ randf(), randf() , randf(), i == 2 ? 1.50F : 0.0f };
			}

			randomSphereIndex++;
		}
	}

	//create the AABB for procedural construction
	for (uint32_t i = 0; i < NUMBER_OF_SPHERES; i++)
	{
		const vec4& iSphere = spheres[i];

		VkAabbPositionsKHR& aabb = AABBs[i];
		aabb.maxX = iSphere.x + iSphere.w;
		aabb.maxY = iSphere.y + iSphere.w;
		aabb.maxZ = iSphere.z + iSphere.w;
		aabb.minX = iSphere.x - iSphere.w;
		aabb.minY = iSphere.y - iSphere.w;
		aabb.minZ = iSphere.z - iSphere.w;
	}

	//creating the sphere's acceleration structure from the aabb
	_RaySphereBottomAS._AccelerationStructure.Alloc(3);
	_RaySphereBottomAS._AccelerationStructureBuffer.Alloc(3);
	_RaySphereBottomAS._AccelerationStructureMemory.Alloc(3);
	_RaySphereBottomAS._CustomInstanceIndex.Alloc(3);
	_RaySphereBottomAS._ShaderOffset.Alloc(3);
	_RaySphereAABBBuffer.Alloc(3);
	VkAabbPositionsKHR* AABBPtr = *AABBs;
	for (uint32_t i = 0; i < 3; i++)
	{
		VulkanHelper::CreateRaytracedProceduralFromAABB(GAPI._VulkanUploader, _RaySphereAABBBuffer[i], _RaySphereBottomAS, AABBPtr, nbOfSpherePerMat[i], i, i, 1);
		AABBPtr += nbOfSpherePerMat[i];
	}

	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, sizeof(vec4) * NUMBER_OF_SPHERES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, *spheres, sizeof(vec4) * NUMBER_OF_SPHERES);


	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, sizeof(vec4) * NUMBER_OF_SPHERES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, *sphereColour, sizeof(vec4) * NUMBER_OF_SPHERES);

	uint32_t offsets[4] = {0, nbOfSpherePerMat[0], nbOfSpherePerMat[0] + nbOfSpherePerMat[1], nbOfSpherePerMat[0] + nbOfSpherePerMat[1] + nbOfSpherePerMat[2]};
	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereOffsets, sizeof(uint32_t)*3, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereOffsets, &offsets, sizeof(uint32_t) * 3);

	AABBs.Clear();
}


void RaytraceGPU::Prepare(class GraphicsAPIManager& GAPI)
{
	GenerateSpheres(GAPI);


	//preparing hardware raytracing props
	{
		//compile the shaders here...
		PrepareVulkanRaytracingScripts(GAPI);
		//then create the pipeline
		PrepareVulkanRaytracingProps(GAPI);
	}

	//a "zero init" of the transform values
	_RayObjData.scale = vec3{ 1.0f, 1.0f, 1.0f };
	_RayObjData.pos = vec3{ 0.0f, 0.0f, 0.0f };

	//preparing full screen copy pipeline
	{
		//the shaders needed
		//compile the shaders here...
		PrepareVulkanScripts(GAPI);
		//then create the pipeline
		PrepareVulkanProps(GAPI);
	}

	enabled = true;
}

/*===== Resize =====*/

void RaytraceGPU::ResizeVulkanRaytracingResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _RayPipelineDynamicDescriptor);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);

	/*===== DESCRIPTORS ======*/

	//realloate descriptor sets with the new number of frames
	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _RayPipelineDynamicDescriptor, GAPI._nb_vk_frames);


	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _RayUniformBuffer, GAPI._nb_vk_frames, sizeof(UniformBuffer));

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//bind the back buffer to each descriptor
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineDynamicDescriptor, _RayFramebuffer._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, 0, i);

		//bind uniform buffer to each descriptor
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _RayPipelineDynamicDescriptor, _RayUniformBuffer._GPUBuffer[i], 0, sizeof(UniformBuffer), 1, i);
	}
}

void RaytraceGPU::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{

	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	ClearFrameBuffer(GAPI._VulkanDevice, _RayFramebuffer);
	VulkanHelper::ReleaseFrameBuffer(GAPI._VulkanDevice, _CopyPipelineOutput);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _CopyDescriptors);

	/*===== CPU SIDE IMAGE BUFFERS ======*/

	//reallocate the fullscreen image buffers with the new number of available images
	VulkanHelper::AllocateFrameBuffer(_CopyPipelineOutput, GAPI._vk_width, GAPI._vk_height, GAPI._nb_vk_frames);

	/*===== DESCRIPTORS ======*/

	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _CopyDescriptors, GAPI._nb_vk_frames);

	/*===== GPU SIDE IMAGE BUFFERS ======*/

	CreateFrameBuffer(GAPI._VulkanUploader, _RayFramebuffer, _CopyPipelineOutput, 0, VK_FORMAT_R16G16B16A16_SFLOAT);

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		VulkanHelper::SetFrameBuffer(GAPI._VulkanUploader, _CopyPipelineOutput, &GAPI._VulkanBackColourBuffers[i], i);

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _CopyDescriptors, _RayFramebuffer._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, i);
	}
}



void RaytraceGPU::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
	ResizeVulkanRaytracingResource(GAPI, old_width, old_height, old_nb_frames);

}

/*===== Act =====*/

void RaytraceGPU::Act(struct AppWideContext& AppContext)
{
	//it will change every frame
	{
		_RayBuffer._proj = inv_perspective_proj(_CopyPipelineOutput._OutputScissor.extent.width, _CopyPipelineOutput._OutputScissor.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		_RayBuffer._view = transpose(AppContext.view_mat);
		_RayBuffer._view .x.w = 0.0f;
		_RayBuffer._view .y.w = 0.0f;
		_RayBuffer._view .z.w = 0.0f;
		_RayBuffer._view .w.xyz = AppContext.camera_pos;
	}
	

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		 changedFlag |= ImGui::SliderFloat3("Object Postion", _RayObjData.pos.scalar, -100.0f, 100.0f);
		 changedFlag |= ImGui::SliderFloat3("Object Rotation", _RayObjData.euler_angles.scalar, -180.0f, 180.0f);
		 changedFlag |= ImGui::SliderFloat3("Object Scale", _RayObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");

		 changedFlag |= ImGui::SliderInt("SampleNb", (int*)&_RayBuffer._nb_samples, 1, 100);
		 changedFlag |= ImGui::SliderInt("Max Bounce Depth", (int*)&_RayBuffer._depth, 1, 20);

		 changedFlag |= ImGui::ColorPicker4("Background Gradient Top", _RayBuffer._background_color_top.scalar);
		 changedFlag |= ImGui::ColorPicker4("Background Gradient Bottom", _RayBuffer._background_color_bottom.scalar);

	}

	_RayObjData.euler_angles.y += 20.0f * AppContext.delta_time;
	_RayBuffer._random = randf();
	_RayBuffer._nb_frame = ImGui::GetFrameCount();
}

/*===== Show =====*/

void RaytraceGPU::Show(GAPIHandle& GAPIHandle)
{
	//to record errors
	VkResult result = VK_SUCCESS;

	//current frames resources
	VkCommandBuffer commandBuffer	= GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore		= GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore		= GAPIHandle.GetCurrentHasPresentedSemaphore();

	{
		//first, reset previous records
		VK_CALL_PRINT(vkResetCommandBuffer(commandBuffer, 0));

		//then open for record
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &info));

	}


	//if (changedFlag)
	{
		mat4 transform = scale(_RayObjData.scale.x, _RayObjData.scale.y, _RayObjData.scale.z) * intrinsic_rot(_RayObjData.euler_angles.x, _RayObjData.euler_angles.y, _RayObjData.euler_angles.z) * translate(_RayObjData.pos);

		changedFlag = false;

		VulkanHelper::Uploader tmpUploader;
		VulkanHelper::StartUploader(GAPIHandle, tmpUploader);
		VulkanHelper::UpdateTransform(GAPIHandle._VulkanDevice, _RayTopAS, transform, 0, _RayBottomAS._AccelerationStructure.Nb());
		VulkanHelper::UpdateRaytracedGroup(tmpUploader, _RayTopAS);
		VulkanHelper::SubmitUploader(tmpUploader);
	}

	//the buffer will change every frame as the object rotates every frame
	memcpy(_RayUniformBuffer._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_RayBuffer, sizeof(UniformBuffer));

	// changing the back buffer to be able to being written by pipeline
	VulkanHelper::ImageMemoryBarrier(commandBuffer, _RayFramebuffer._Images[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	VkDescriptorSet currentSet[2] = { _RayPipelineStaticDescriptor._DescriptorSets[0], _RayPipelineDynamicDescriptor._DescriptorSets[GAPIHandle._vk_current_frame] };
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayLayout, 0, 2, currentSet, 0, nullptr);

	VK_CALL_KHR(GAPIHandle._VulkanDevice, vkCmdTraceRaysKHR, commandBuffer, &_RayShaderBindingTable._RayGenRegion, &_RayShaderBindingTable._MissRegion, &_RayShaderBindingTable._HitRegion, &_RayShaderBindingTable._CallableRegion, _CopyPipelineOutput._OutputScissor.extent.width, _CopyPipelineOutput._OutputScissor.extent.height, 1);

	//allowing copy from write iamge to framebuffer
	VulkanHelper::ImageMemoryBarrier(commandBuffer, _RayFramebuffer._Images[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass		= _CopyPipelineOutput._OutputRenderPass;
		info.framebuffer	= _CopyPipelineOutput._OuputFrameBuffer[GAPIHandle._vk_frame_index];
		info.renderArea		= _CopyPipelineOutput._OutputScissor;
		info.clearValueCount = 1;

		//clear our output (grey for backbuffer)
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyLayout, 0, 1, &_CopyDescriptors._DescriptorSets[GAPIHandle._vk_current_frame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &_CopyPipelineOutput._OutputViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_CopyPipelineOutput._OutputScissor);

	// ... then draw !
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
	{
		//the pipeline stage at which the GPU should waait for the semaphore to signal itself
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR

		//we submit our commands, while setting the necesssary fences (semaphores on GPU), 
		//to schedule work properly
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &waitSemaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &commandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &signalSemaphore;

		//closing our command record ...
		VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));
		//... and submit it straight away
		VK_CALL_PRINT(vkQueueSubmit(GAPIHandle._VulkanQueues[0], 1, &info, nullptr));
	}
}

/*===== Close =====*/

void RaytraceGPU::Close(class GraphicsAPIManager& GAPI)
{
	//the memory allocated by the Vulkan Helper is volatile : it must be explecitly freed !

	//Clear geometry data
    if (_RayModel._Meshes != nullptr)
        ClearMesh(GAPI._VulkanDevice, _RayModel._Meshes[0]);
	_RayModel._Meshes.Clear();
	VulkanHelper::ClearSceneBuffer(GAPI._VulkanDevice, _RaySceneBuffer);

	//clear Acceleration Structures
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RayBottomAS);
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RaySphereBottomAS);
	VulkanHelper::ClearRaytracedGroup(GAPI._VulkanDevice, _RayTopAS);

	//release descriptors
	
	ClearPipelineDescriptor(GAPI._VulkanDevice, _CopyDescriptors);
	ClearPipelineDescriptor(GAPI._VulkanDevice, _RayPipelineStaticDescriptor);
	ClearPipelineDescriptor(GAPI._VulkanDevice, _RayPipelineDynamicDescriptor);
	vkDestroySampler(GAPI._VulkanDevice, _CopySampler,nullptr);


	//clear buffers
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereColour);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereOffsets);
    if (_RaySphereAABBBuffer != nullptr)
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereAABBBuffer[i]);
        }
		_RaySphereAABBBuffer.Clear();
    }

	//clear shader resources
	ClearShaderBindingTable(GAPI._VulkanDevice, _RayShaderBindingTable);
	CLEAR_LIST(_RayShaders, _RayShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);
	CLEAR_LIST(_CopyShaders, _CopyShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);


	//clear the fullscreen images buffer
	VulkanHelper::ClearPipelineOutput(GAPI._VulkanDevice, _CopyPipelineOutput);
	ClearFrameBuffer(GAPI._VulkanDevice, _RayFramebuffer);

	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _CopyLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _CopyPipeline, nullptr);
}
