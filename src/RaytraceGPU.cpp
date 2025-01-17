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
		VulkanHelper::CreateRaytracedGeometryFromMesh(GAPI._VulkanUploader, _RayBottomAS, _RayModel._Meshes, VK_NULL_HANDLE, nullptr, 0, 0);

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
		//creating our acceleration structure binder
		VkDescriptorSetLayoutBinding ASLayoutBinding{};
		ASLayoutBinding.binding			= 0;
		ASLayoutBinding.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASLayoutBinding.descriptorCount = 1;
		ASLayoutBinding.stageFlags		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		//create a binding for our colour buffers
		VkDescriptorSetLayoutBinding colourBackBufferBinding{};
		colourBackBufferBinding.binding			= 1;
		colourBackBufferBinding.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		colourBackBufferBinding.descriptorCount	= 1;
		colourBackBufferBinding.stageFlags		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding uniformBufferBinding{};
		uniformBufferBinding.binding		= 2;
		uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBufferBinding.descriptorCount = 1;
		uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sphereBufferBinding{};
		sphereBufferBinding.binding				= 3;
		sphereBufferBinding.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereBufferBinding.descriptorCount		= 1;
		sphereBufferBinding.stageFlags			= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sphereColourBinding{};
		sphereColourBinding.binding			= 4;
		sphereColourBinding.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereColourBinding.descriptorCount = 1;
		sphereColourBinding.stageFlags		= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sphereOffsetBinding{};
		sphereOffsetBinding.binding = 5;
		sphereOffsetBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereOffsetBinding.descriptorCount = 1;
		sphereOffsetBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding offsetBinding{};
		offsetBinding.binding = 6;
		offsetBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		offsetBinding.descriptorCount = 1;
		offsetBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sceneIndexBinding{};
		sceneIndexBinding.binding = 7;
		sceneIndexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sceneIndexBinding.descriptorCount = 1;
		sceneIndexBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sceneUVBinding{};
		sceneUVBinding.binding = 8;
		sceneUVBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sceneUVBinding.descriptorCount = 1;
		sceneUVBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sceneNormalBinding{};
		sceneNormalBinding.binding = 9;
		sceneNormalBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sceneNormalBinding.descriptorCount = 1;
		sceneNormalBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 10;
		VkDescriptorSetLayoutBinding layoutsBinding[10] = {ASLayoutBinding , colourBackBufferBinding, uniformBufferBinding, sphereBufferBinding, sphereColourBinding, sphereOffsetBinding,
		offsetBinding, sceneIndexBinding, sceneUVBinding, sceneNormalBinding};
		layoutInfo.pBindings = layoutsBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_RayDescriptorLayout));
	}

	{
		//our pipeline layout is the same as the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_RayDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_RayLayout));
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
			layout(binding = 1, rgba32f) uniform image2D image;
			layout(binding = 2) uniform UniformBuffer
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

			layout(binding = 2) uniform UniformBuffer
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


void RaytraceGPU::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//describe vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = VertexShader;
	vertShaderStageInfo.pName = "main";

	//describe fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = FragmentShader;
	fragShaderStageInfo.pName = "main";

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
	inputAssembly.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology				= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports	= &_CopyViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &_CopyScissors;


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
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

	{
		/*===== FRAMEBUFFER OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription frameColourBufferAttachment{};
		frameColourBufferAttachment.format			= GAPI._VulkanSurfaceFormat.format;
		frameColourBufferAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		frameColourBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear
		frameColourBufferAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;//we want to write into the framebuffer
		frameColourBufferAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		frameColourBufferAttachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//there is no post process this is immediate writing into frame buffer

		/*===== SUBPASS DESCRIPTION ======*/

		//describing the output in the subpass
		VkAttachmentReference frameColourBufferAttachmentRef{};
		frameColourBufferAttachmentRef.attachment	= 0;
		frameColourBufferAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//describing the subpass of our main renderpass
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount	= 1;
		subpass.pColorAttachments		= &frameColourBufferAttachmentRef;

		//describing our renderpass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount	= 1;
		renderPassInfo.pAttachments		= &frameColourBufferAttachment;
		renderPassInfo.subpassCount		= 1;
		renderPassInfo.pSubpasses		= &subpass;
		VkSubpassDependency dependency{};
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderPassInfo, nullptr, &_CopyRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_CopySampler);


		//the sampler and image to sample the CPU texture
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &_CopySampler;


		//the descriptor layout that include the immutable sampler and the local GPU image
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings	= &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_CopyDescriptorLayout));

	}

	{
		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = &_CopyDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_CopyLayout));
	}

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//putting our two shader stages
	VkPipelineShaderStageCreateInfo stages[2] = { vertShaderStageInfo, fragShaderStageInfo };
	pipelineInfo.pStages = stages;
	pipelineInfo.stageCount = 2;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= nullptr;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.layout					= _CopyLayout;
	pipelineInfo.renderPass				= _CopyRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_CopyPipeline));

	vkDestroyShaderModule(GAPI._VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI._VulkanDevice, VertexShader, nullptr);
}

void RaytraceGPU::PrepareVulkanScripts(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
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


	CompileVulkanShaders(GAPI._VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex");
	CompileVulkanShaders(GAPI._VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag");
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
	spheres[0] = vec4( 0.0f, 1000.0f, 0.0f, 1000.0f);//the ground
	spheres[1] = vec4( 0.0f, -5.0f, 0.0f, 5.0f);//a lambertian diffuse example
	spheres[nbOfSpherePerMat[0]] = vec4(10.0f, -5.0f, 0.0f, 5.0f);//a metal example
	spheres[nbOfSpherePerMat[0] + nbOfSpherePerMat[1]] = vec4(-10.0f, -5.0f, 0.0f, 5.0f);//a dieletric example (this is a glass sphere)

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
				vec3 sphereCenter = vec3{ (randf(-1.0f,1.0f)) * 20.0f, -radius, (randf(-1.0f,1.0f)) * 20.0f };

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

	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, sizeof(vec4) * 29, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, *spheres, sizeof(vec4) * 29);


	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, sizeof(vec4) * 29, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, *sphereColour, sizeof(vec4) * 29);

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
		VkShaderModule VertexShader, FragmentShader;

		//compile the shaders here...
		PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
		//then create the pipeline
		PrepareVulkanProps(GAPI, VertexShader, FragmentShader);
	}
}

/*===== Resize =====*/

void RaytraceGPU::ResizeVulkanRaytracingResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayDescriptorPool, nullptr);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);

	/*===== DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize ASPoolSize{};
		ASPoolSize.type			= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASPoolSize.descriptorCount = 1;

		VkDescriptorPoolSize imagePoolSize{};
		imagePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imagePoolSize.descriptorCount = 1;

		VkDescriptorPoolSize bufferPoolSize{};
		bufferPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferPoolSize.descriptorCount = 1;

		VkDescriptorPoolSize spherePoolSize{};
		spherePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		spherePoolSize.descriptorCount = 1;

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 10;
		VkDescriptorPoolSize poolSizes[10] = {ASPoolSize, imagePoolSize, bufferPoolSize, spherePoolSize, spherePoolSize, spherePoolSize , spherePoolSize, spherePoolSize , spherePoolSize , spherePoolSize };
		poolInfo.pPoolSizes		= poolSizes;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_RayDescriptorPool))

		//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _RayDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_RayDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_RayDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_RayDescriptorSet));
	}

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _RayUniformBuffer, GAPI._nb_vk_frames, sizeof(UniformBuffer));

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//Adding our Top Level AS in each descriptor set
		VkWriteDescriptorSetAccelerationStructureKHR ASInfo{};
		ASInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		ASInfo.pAccelerationStructures		= &_RayTopAS._AccelerationStructure;
		ASInfo.accelerationStructureCount	= 1;
		
		VkWriteDescriptorSet ASDescriptorWrite{};
		ASDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ASDescriptorWrite.dstSet			= _RayDescriptorSet[i];
		ASDescriptorWrite.dstBinding		= 0;
		ASDescriptorWrite.dstArrayElement	= 0;
		ASDescriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASDescriptorWrite.descriptorCount	= 1;
		ASDescriptorWrite.pNext				= &ASInfo;

		//bind the back buffer to each descriptor
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageView = _RayWriteImageView[i];
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _RayDescriptorSet[i];
		imageDescriptorWrite.dstBinding			= 1;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageInfo;

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = _RayUniformBuffer._GPUBuffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBuffer);

		//then link it to the descriptor
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= _RayDescriptorSet[i];
		descriptorWrite.dstBinding		= 2;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo		= &bufferInfo;

		VkDescriptorBufferInfo sphereInfo{};
		sphereInfo.buffer	= _RaySphereBuffer._StaticGPUBuffer;
		sphereInfo.offset	= 0;
		sphereInfo.range	= sizeof(vec4) * 29;

		//then link it to the descriptor
		VkWriteDescriptorSet sphereWrite{};
		sphereWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sphereWrite.dstSet				= _RayDescriptorSet[i];
		sphereWrite.dstBinding			= 3;
		sphereWrite.dstArrayElement		= 0;
		sphereWrite.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereWrite.descriptorCount		= 1;
		sphereWrite.pBufferInfo			= &sphereInfo;

		VkDescriptorBufferInfo sphereColourInfo{};
		sphereColourInfo.buffer = _RaySphereColour._StaticGPUBuffer;
		sphereColourInfo.offset = 0;
		sphereColourInfo.range = sizeof(vec4) * 29;

		//then link it to the descriptor
		VkWriteDescriptorSet sphereColourWrite{};
		sphereColourWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sphereColourWrite.dstSet = _RayDescriptorSet[i];
		sphereColourWrite.dstBinding = 4;
		sphereColourWrite.dstArrayElement = 0;
		sphereColourWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereColourWrite.descriptorCount = 1;
		sphereColourWrite.pBufferInfo = &sphereColourInfo;

		VkDescriptorBufferInfo sphereOffsetInfo{};
		sphereOffsetInfo.buffer = _RaySphereOffsets._StaticGPUBuffer;
		sphereOffsetInfo.offset = 0;
		sphereOffsetInfo.range = sizeof(uint32_t) * 3;

		//then link it to the descriptor
		VkWriteDescriptorSet sphereOffsetWrite{};
		sphereOffsetWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sphereOffsetWrite.dstSet		= _RayDescriptorSet[i];
		sphereOffsetWrite.dstBinding	= 5;
		sphereOffsetWrite.dstArrayElement = 0;
		sphereOffsetWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereOffsetWrite.descriptorCount = 1;
		sphereOffsetWrite.pBufferInfo = &sphereOffsetInfo;

		VkDescriptorBufferInfo offsetInfo{};
		offsetInfo.buffer	= _RaySceneBuffer._OffsetBuffer._StaticGPUBuffer;
		offsetInfo.offset	= 0;
		offsetInfo.range	= _RaySceneBuffer._OffsetBufferSize;

		//then link it to the descriptor
		VkWriteDescriptorSet offsetWrite{};
		offsetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		offsetWrite.dstSet = _RayDescriptorSet[i];
		offsetWrite.dstBinding = 6;
		offsetWrite.dstArrayElement = 0;
		offsetWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		offsetWrite.descriptorCount = 1;
		offsetWrite.pBufferInfo = &offsetInfo;

		VkDescriptorBufferInfo indexInfo{};
		indexInfo.buffer = _RaySceneBuffer._IndexBuffer._StaticGPUBuffer;
		indexInfo.offset = 0;
		indexInfo.range = _RaySceneBuffer._IndexBufferSize;

		//then link it to the descriptor
		VkWriteDescriptorSet indexWrite{};
		indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		indexWrite.dstSet = _RayDescriptorSet[i];
		indexWrite.dstBinding = 7;
		indexWrite.dstArrayElement = 0;
		indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		indexWrite.descriptorCount = 1;
		indexWrite.pBufferInfo = &indexInfo;

		VkDescriptorBufferInfo uvInfo{};
		uvInfo.buffer = _RaySceneBuffer._UVsBuffer._StaticGPUBuffer;
		uvInfo.offset = 0;
		uvInfo.range = _RaySceneBuffer._UVsBufferSize;

		//then link it to the descriptor
		VkWriteDescriptorSet uvWrite{};
		uvWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uvWrite.dstSet = _RayDescriptorSet[i];
		uvWrite.dstBinding = 8;
		uvWrite.dstArrayElement = 0;
		uvWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		uvWrite.descriptorCount = 1;
		uvWrite.pBufferInfo = &uvInfo;

		VkDescriptorBufferInfo normalInfo{};
		normalInfo.buffer = _RaySceneBuffer._NormalBuffer._StaticGPUBuffer;
		normalInfo.offset = 0;
		normalInfo.range = _RaySceneBuffer._NormalBufferSize;

		//then link it to the descriptor
		VkWriteDescriptorSet normalWrite{};
		normalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalWrite.dstSet = _RayDescriptorSet[i];
		normalWrite.dstBinding = 9;
		normalWrite.dstArrayElement = 0;
		normalWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		normalWrite.descriptorCount = 1;
		normalWrite.pBufferInfo = &normalInfo;


		VkWriteDescriptorSet descriptorWrites[10] = { ASDescriptorWrite,  imageDescriptorWrite, descriptorWrite, sphereWrite, sphereColourWrite, sphereOffsetWrite, offsetWrite, indexWrite, uvWrite, normalWrite };
		vkUpdateDescriptorSets(GAPI._VulkanDevice, 10, descriptorWrites, 0, nullptr);
	}
}

void RaytraceGPU::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{

	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(_RayWriteImageView, old_nb_frames, vkDestroyImageView, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_RayWriteImage, old_nb_frames, vkDestroyImage, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_CopyOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);

	

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI._VulkanDevice, _RayImageMemory, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _CopyDescriptorPool, nullptr);


	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport and scissors
	_CopyViewport.width = (float)GAPI._vk_width;
	_CopyViewport.height = (float)GAPI._vk_height;
	_CopyViewport.maxDepth = 1.0f;
	_CopyViewport.minDepth = 0.0f;
	_CopyViewport.x = 0.0f;
	_CopyViewport.y = 0.0f;
	_CopyScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== CPU SIDE IMAGE BUFFERS ======*/

	//reallocate the fullscreen image buffers with the new number of available images
	_CopyOutput.Alloc(GAPI._nb_vk_frames);
	_RayWriteImageView.Alloc(GAPI._nb_vk_frames);
	_RayWriteImage.Alloc(GAPI._nb_vk_frames);


	/*===== DESCRIPTORS ======*/

	//recreate the descriptor pool
	{
		//there is only sampler and image in the descriptor pool
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolUniformSize.descriptorCount = GAPI._nb_vk_frames;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolUniformSize;
		poolInfo.maxSets = GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_CopyDescriptorPool))
	}

	//allocate the memory for descriptor sets
	{
		//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _CopyDescriptorPool;
		allocInfo.descriptorSetCount = GAPI._nb_vk_frames;

		//make a copy of our loayout for the number of frames needed
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_CopyDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//Allocate descriptor sets on CPU and GPU side
		_CopyDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_CopyDescriptorSet));
	}

	/*===== GPU SIDE IMAGE BUFFERS ======*/

	// the size of the fullscreen image's buffer : a fullscreen rgba texture
	VkDeviceSize imageOffset = 0;

	//allocate the memory for buffer and image
	{

		// creating the image object with data given in parameters
		VkImageCreateInfo imageInfo{};
		imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;//we copy a fullscreen image
		imageInfo.format		= VK_FORMAT_R32G32B32A32_SFLOAT;//easier to manipulate (may be unsuppoorted by some GPUs)
		imageInfo.extent.width	= _CopyScissors.extent.width;
		imageInfo.extent.height = _CopyScissors.extent.height;
		imageInfo.extent.depth	= 1;
		imageInfo.mipLevels		= 1u;
		imageInfo.arrayLayers	= 1u;
		imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each _Scene will do what they need on their own
		imageInfo.usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
		imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
		imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
		imageInfo.flags			= 0; // Optional

		VK_CALL_PRINT(vkCreateImage(GAPI._VulkanDevice, &imageInfo, nullptr, &_RayWriteImage[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(GAPI._VulkanDevice, _RayWriteImage[0], &memRequirements);
		imageOffset = memRequirements.size;

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= memRequirements.size * GAPI._nb_vk_frames;//we want multiple frames
		allocInfo.memoryTypeIndex	= VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, GAPI._VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(GAPI._VulkanDevice, &allocInfo, nullptr, &_RayImageMemory));

		vkDestroyImage(GAPI._VulkanDevice, _RayWriteImage[0], nullptr);
	}

	/*===== FRAMEBUFFERS ======*/

	//describing the framebuffer to the swapchain
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= _CopyRenderPass;
	framebufferInfo.width		= GAPI._vk_width;
	framebufferInfo.height		= GAPI._vk_height;
	framebufferInfo.layers		= 1;

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI._VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_CopyOutput[i]))

		//create the image that will be used to write from texture to screen frame buffer
		VulkanHelper::CreateImage(GAPI._VulkanUploader, _RayWriteImage[i], _RayImageMemory, _CopyScissors.extent.width, _CopyScissors.extent.height, 1, 
			VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageOffset * i, false);

		// changing the image to allow read from shader
		VkImageMemoryBarrier barrier{};
		barrier.sType						= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout					= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout					= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to shader read dest (will be change to storage afterwards)
		barrier.srcQueueFamilyIndex			= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex			= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image						= _RayWriteImage[i];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;// making the image accessible for ...
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(GAPI._VulkanUploader._CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.format						= VK_FORMAT_R32G32B32A32_SFLOAT;
		viewCreateInfo.image						= _RayWriteImage[i];
		viewCreateInfo.subresourceRange.layerCount	= 1;
		viewCreateInfo.subresourceRange.levelCount	= 1;
		viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;

		VK_CALL_PRINT(vkCreateImageView(GAPI._VulkanDevice, &viewCreateInfo, nullptr, &_RayWriteImageView[i]));

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VkDescriptorImageInfo imageViewInfo{};
		imageViewInfo.imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageViewInfo.imageView		= _RayWriteImageView[i];

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _CopyDescriptorSet[i];
		imageDescriptorWrite.dstBinding			= 0;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageViewInfo;

		vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &imageDescriptorWrite, 0, nullptr);
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
		_RayBuffer._proj = inv_perspective_proj(_CopyScissors.extent.width, _CopyScissors.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
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

		 changedFlag |= ImGui::SliderInt("SampleNb", (int*)&_RayBuffer._nb_samples, 1, 50);
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
	VulkanHelper::ImageMemoryBarrier(commandBuffer, _RayWriteImage[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayLayout, 0, 1, &_RayDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);

	VK_CALL_KHR(GAPIHandle._VulkanDevice, vkCmdTraceRaysKHR, commandBuffer, &_RayShaderBindingTable._RayGenRegion, &_RayShaderBindingTable._MissRegion, &_RayShaderBindingTable._HitRegion, &_RayShaderBindingTable._CallableRegion, _CopyScissors.extent.width, _CopyScissors.extent.height, 1);

	//allowing copy from write iamge to framebuffer
	VulkanHelper::ImageMemoryBarrier(commandBuffer, _RayWriteImage[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass		= _CopyRenderPass;
		info.framebuffer	= _CopyOutput[GAPIHandle._vk_frame_index];
		info.renderArea		= _CopyScissors;
		info.clearValueCount = 1;

		//clear our output (grey for backbuffer)
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyLayout, 0, 1, &_CopyDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &_CopyViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_CopyScissors);

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
	ClearMesh(GAPI._VulkanDevice, _RayModel._Meshes[0]);
	VulkanHelper::ClearSceneBuffer(GAPI._VulkanDevice, _RaySceneBuffer);

	//clear Acceleration Structures
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RayBottomAS);
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RaySphereBottomAS);
	VulkanHelper::ClearRaytracedGroup(GAPI._VulkanDevice, _RayTopAS);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _CopyDescriptorPool, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayDescriptorPool, nullptr);
	_RayDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _RayDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _CopyDescriptorLayout, nullptr);
	_CopyDescriptorSet.Clear();
	vkDestroySampler(GAPI._VulkanDevice, _CopySampler,nullptr);


	//clear buffers
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereColour);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereOffsets);
	for (uint32_t i = 0; i < 3; i++)
	{
		ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereAABBBuffer[i]);

	}

	//clear shader resources
	ClearShaderBindingTable(GAPI._VulkanDevice, _RayShaderBindingTable);
	CLEAR_LIST(_RayShaders, _RayShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);

	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(_RayWriteImageView, GAPI._nb_vk_frames, vkDestroyImageView, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_RayWriteImage, GAPI._nb_vk_frames, vkDestroyImage, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_CopyOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI._VulkanDevice, _RayImageMemory, nullptr);

	//release pipeline objects
	vkDestroyRenderPass(GAPI._VulkanDevice, _CopyRenderPass, nullptr);
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _CopyLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _CopyPipeline, nullptr);
}
