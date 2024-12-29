#include "RaytraceGPU.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

/*==== Prepare =====*/

void RaytraceGPU::PrepareVulkanRaytracingProps(GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader, VkShaderModule& IntersectShader)
{
	VkResult result = VK_SUCCESS;

	/*===== MODEL LOADING & AS BUILDING ======*/

	//load the model
	VulkanHelper::LoadObjFile(GAPI._VulkanUploader, "../../../media/teapot/teapot.obj", _RayModel._Meshes);

	//create bottom level AS from model
	VulkanHelper::CreateRaytracedGeometryFromMesh(GAPI._VulkanUploader, _RayBottomAS, _RayModel._Meshes);

	//create Top level AS from geometry with identity matrix
	VulkanHelper::UploadRaytracedGroupFromGeometry(GAPI._VulkanUploader, _RayTopAS, identity(), _RayBottomAS);

	/*===== DESCRIBE SHADER STAGE AND GROUPS =====*/

	//the shader stages we'll give to the pipeline
	MultipleVolatileMemory<VkPipelineShaderStageCreateInfo> shaderStages{ static_cast<VkPipelineShaderStageCreateInfo*>(alloca(sizeof(VkPipelineShaderStageCreateInfo) * 4)) };
	memset(*shaderStages, 0, sizeof(VkPipelineShaderStageCreateInfo) * 4);
	//the shader groups, to allow the pipeline to create the shader table
	MultipleVolatileMemory<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{ static_cast<VkRayTracingShaderGroupCreateInfoKHR*>(alloca(sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 3)) };
	memset(*shaderGroups, 0, sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 3);

	//first, we'll describe ray gen
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shaderStages[0].module = RayGenShader;
	shaderStages[0].pName = "main";

	//then, miss shader
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shaderStages[1].module = MissShader;
	shaderStages[1].pName = "main";

	//finally, closest hit shader
	shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shaderStages[2].module = HitShader;
	shaderStages[2].pName = "main";

	//intersect shader for procedural
	shaderStages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[3].stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	shaderStages[3].module = IntersectShader;
	shaderStages[3].pName = "main";

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
	shaderGroups[2].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	//shaderGroups[2].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroups[2].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	shaderGroups[2].generalShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].closestHitShader	= 2;
	shaderGroups[2].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].intersectionShader	= 3;

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
		uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sphereBufferBinding{};
		sphereBufferBinding.binding				= 3;
		sphereBufferBinding.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereBufferBinding.descriptorCount		= 1;
		sphereBufferBinding.stageFlags			= VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

		//create a binding for our uniform buffers
		VkDescriptorSetLayoutBinding sphereColourBinding{};
		sphereColourBinding.binding			= 4;
		sphereColourBinding.descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		sphereColourBinding.descriptorCount = 1;
		sphereColourBinding.stageFlags		= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 5;
		VkDescriptorSetLayoutBinding layoutsBinding[5] = {ASLayoutBinding , colourBackBufferBinding, uniformBufferBinding, sphereBufferBinding, sphereColourBinding};
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
	pipelineInfo.stageCount						= 4;
	pipelineInfo.pStages						= *shaderStages;
	pipelineInfo.groupCount						= 3;
	pipelineInfo.pGroups						= *shaderGroups;
	pipelineInfo.maxPipelineRayRecursionDepth	= 1;//for the moment we'll hardcode the value, we'll see if we'll make it editable
	pipelineInfo.layout							= _RayLayout;//describe the attachement

	VK_CALL_KHR(GAPI._VulkanDevice, vkCreateRayTracingPipelinesKHR, GAPI._VulkanDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_RayPipeline);


	/*===== SHADER BINDING TABLE CREATION =====*/
	
	//allocating space for the address of the SBT
	_RayShaderBindingAddress.Alloc(3);

	{
		//this machine's specific GPU properties to get the SBT alignement
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
		rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		//needed to get the above info
		VkPhysicalDeviceProperties2 GPUProperties{};
		GPUProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		GPUProperties.pNext = &rtProperties;
		vkGetPhysicalDeviceProperties2(GAPI._VulkanGPU, &GPUProperties);

		//the needed alignement to create our shader binding table
		uint32_t handleSize			= rtProperties.shaderGroupHandleSize;
		uint32_t handleAlignment	= rtProperties.shaderGroupHandleAlignment;
		uint32_t baseAlignment		= rtProperties.shaderGroupBaseAlignment;
		uint32_t handleSizeAligned	= VulkanHelper::AlignUp(handleSize, handleAlignment);
		uint32_t startSizeAligned	= VulkanHelper::AlignUp(handleSizeAligned, baseAlignment);

		//raygen shader's aligned stride and size
		_RayShaderBindingAddress[0].stride	= startSizeAligned;//there will only be one but still
		_RayShaderBindingAddress[0].size	= startSizeAligned;

		//miss shader's aligned stride and size
		_RayShaderBindingAddress[1].stride	= handleSizeAligned;
		_RayShaderBindingAddress[1].size	= startSizeAligned;

		//hit shader's aligned stride and size
		_RayShaderBindingAddress[2].stride	= handleSizeAligned;
		_RayShaderBindingAddress[2].size	= startSizeAligned;

		//creating a GPU buffer for the Shader Binding Table
		VulkanHelper::CreateVulkanBufferAndMemory(GAPI._VulkanUploader, 3 * startSizeAligned, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _RayShaderBindingBuffer, _RayShaderBindingMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		//get back handle from shader group created in pipeline
		MultipleVolatileMemory<uint8_t> shaderGroupHandle{ (uint8_t*)alloca(3 * handleSize) };
		VK_CALL_KHR( GAPI._VulkanDevice, vkGetRayTracingShaderGroupHandlesKHR, GAPI._VulkanDevice, _RayPipeline, 0, 3, 3 * handleSize, *shaderGroupHandle);

		//mapping memory to buffer
		uint8_t* CPUSBTBufferMap;
		vkMapMemory(GAPI._VulkanDevice, _RayShaderBindingMemory, 0, 3 * startSizeAligned, 0, (void**)(& CPUSBTBufferMap));

		//memcpy(CPUSBTBufferMap, *shaderGroupHandle, 3 * startSizeAligned);
		//copying every address to the shader table
		memcpy(CPUSBTBufferMap, *shaderGroupHandle, handleSize);
		memcpy(CPUSBTBufferMap + startSizeAligned, *shaderGroupHandle + handleSize, handleSize);
		memcpy(CPUSBTBufferMap + 2 * startSizeAligned, *shaderGroupHandle + 2 * handleSize, handleSize);

		//unmap and copy
		vkUnmapMemory(GAPI._VulkanDevice, _RayShaderBindingMemory);

		//get our GPU Buffer's address
		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = _RayShaderBindingBuffer;
		VkDeviceAddress GPUAddress = vkGetBufferDeviceAddress(GAPI._VulkanDevice, &addressInfo);

		_RayShaderBindingAddress[0].deviceAddress = GPUAddress;
		_RayShaderBindingAddress[1].deviceAddress = GPUAddress + _RayShaderBindingAddress[0].size;
		_RayShaderBindingAddress[2].deviceAddress = GPUAddress + _RayShaderBindingAddress[0].size + _RayShaderBindingAddress[1].size;
	}
}

void RaytraceGPU::PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader, VkShaderModule& IntersectShader)
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
				
			};
			layout(location = 0) rayPayloadEXT HitRecord payload;

			layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
			layout(binding = 1, rgba32f) uniform image2D image;
			layout(binding = 2) uniform Transform
			{
				mat4 model;
				mat4 view;
				mat4 proj;
			};
			layout(binding = 3) uniform spheres { vec4[] sphere; };

			void main()
			{
				const vec2 pixelCenter = gl_LaunchIDEXT.xy + vec2(0.5);

				//finding the pixel we are computing from the ray launch arguments
				const vec2 inUV = (pixelCenter) / vec2(gl_LaunchSizeEXT.xy);

				vec2 d = inUV * 2.0 - 1.0;

				vec3 finalColor = vec3(0.0);

				//for (int j = 0; j < 10; j++)
				{

					//for the moment, fixed
					vec4 origin = -view[3];
					
					//the targets are on a viewport ahead of us by 3
					vec4 target = proj * vec4(d,1.0,1.0);
					target = target/target.w;

					//creating a direction for our ray
					vec4 direction = view * vec4(normalize(target.xyz),0.0);

					//zero init
					payload.bHit		= 0;
					payload.hitColor	= vec3(0.5);
					payload.hitDistance = 0.0;
					payload.hitNormal	= vec3(0.0);

					vec3 fragColor = vec3(1.0);

					for (int i = 0; i < 10; i++)
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

					finalColor = fragColor;
				}
				
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
				
			};
			layout(location = 0) rayPayloadInEXT HitRecord payload;

			void main()
			{
				vec3 background_color_bottom = vec3(1.0);
				vec3 background_color_top = vec3(0.3, 0.7, 1.0);

				float backgroundGradient = 0.5 * (gl_WorldRayDirectionEXT.y + 1.0);

				payload.bHit = 0.0;
				payload.hitColor = background_color_bottom * (1.0 - backgroundGradient) + background_color_top * backgroundGradient;
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
				
			};
			layout(location = 0) rayPayloadInEXT HitRecord payload;

			uint InitRandomSeed(uint val0, uint val1)
			{
				uint v0 = val0, v1 = val1, s0 = 0;
			
				for (uint n = 0; n < 16; n++)
				{
					s0 += 0x9e3779b9;
					v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
					v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
				}
			
				return v0;
			}


			uint RandomInt(inout uint seed)
			{
				// LCG values from Numerical Recipes
			    return (seed = 1664525 * seed + 1013904223);
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


			layout(binding = 4) readonly buffer SphereArray { vec4[] SphereColour; };
			hitAttributeEXT vec4 attribs;

			void main()
			{
				const vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
				vec3 normal = (hitPoint - attribs.xyz) / attribs.w;

				uint seed = InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y);

				const vec3 rand = RandomInUnitSphere(seed);
				normal = normalize(normal + rand);

				payload.bHit		= 1.0;
				payload.hitColor	= SphereColour[gl_PrimitiveID].rgb;
				payload.hitDistance = gl_HitTEXT;
				payload.hitNormal	= normal;
			})";

	//define closest hit shader
	const char* intersect_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			#define HIT_EPSILON 0.01f

			hitAttributeEXT vec4 attribs;
			layout(binding = 3) readonly buffer SphereArray { vec4[] Spheres; };

			void main()
			{
				const vec4 sphere = Spheres[gl_PrimitiveID];

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

	CreateVulkanShaders(GAPI._VulkanUploader, RayGenShader, VK_SHADER_STAGE_RAYGEN_BIT_KHR, ray_gen_shader, "Raytrace GPU RayGen");
	CreateVulkanShaders(GAPI._VulkanUploader, MissShader, VK_SHADER_STAGE_MISS_BIT_KHR, miss_shader, "Raytrace GPU Miss");
	CreateVulkanShaders(GAPI._VulkanUploader, HitShader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closest_hit_shader, "Raytrace GPU Closest");
	CreateVulkanShaders(GAPI._VulkanUploader, IntersectShader, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersect_shader, "Raytrace GPU Intersect");

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


	CreateVulkanShaders(GAPI._VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex");
	CreateVulkanShaders(GAPI._VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag");
}

void RaytraceGPU::GenerateSpheres(GraphicsAPIManager& GAPI)
{
	// basically copy-paste of the CPU raytracing way of creating spheres


	MultipleScopedMemory<vec4> spheres{ 29 };
	MultipleScopedMemory<vec4> sphereColour{ 29 };
	MultipleVolatileMemory<VkAabbPositionsKHR> AABBs{ 29 };

	spheres[0] = vec4( 0.0f, 1000.0f, 0.0f, 1000.0f);//the ground
	spheres[1] = vec4( 0.0f, -5.0f, 0.0f, 5.0f);//a lambertian diffuse example
	spheres[2] = vec4( -10.0f, -5.0f, 0.0f, 5.0f);//a dieletric example (this is a glass sphere)
	spheres[3] = vec4( 10.0f, -5.0f, 0.0f, 5.0f);//a metal example

	sphereColour[0] = vec4{ 0.5f,0.5f,0.5f, 1.0f };//the ground material
	sphereColour[1] = vec4{ 0.4f, 0.2f, 0.1f, 1.0f };//a lambertian diffuse example
	sphereColour[2] = vec4{ 1.0f,1.0f,1.0f, 1.50f };//a dieletric example (this is a glass sphere)
	sphereColour[3] = vec4{ 0.7f, 0.6f, 0.5f, 1.0f };//a metal example

	//we already have the example material, so further material will start after
	uint32_t matNb = 4;
	//we already have example sphere, so further objects will be created after
	uint32_t sphereNb = 0;

	for (; sphereNb <= 3; sphereNb++)
	{
		VkAabbPositionsKHR& aabb = AABBs[sphereNb];
		aabb.maxX = spheres[sphereNb].x + spheres[sphereNb].w;
		aabb.maxY = spheres[sphereNb].y + spheres[sphereNb].w;
		aabb.maxZ = spheres[sphereNb].z + spheres[sphereNb].w;
		aabb.minX = spheres[sphereNb].x - spheres[sphereNb].w;
		aabb.minY = spheres[sphereNb].y - spheres[sphereNb].w;
		aabb.minZ = spheres[sphereNb].z - spheres[sphereNb].w;
	}


	//we create sphere at random place with random materials for the demo
	for (int32_t x = -2; x <= 2; x++)
	{
		for (int32_t z = -2; z <= 2; z++, sphereNb++)
		{
			float radius = 1.0f + randf() * 1.0f;
			vec3 sphereCenter = vec3{ (static_cast<float>(x) + randf(-1.0f,1.0f)) * 10.0f, -radius, (static_cast<float>(z) + randf(-1.0f,1.0f)) * 10.0f };

			spheres[sphereNb] = vec4(sphereCenter.x, sphereCenter.y, sphereCenter.z, radius);

			sphereColour[sphereNb] = vec4{ randf(), randf() , randf(), 0.0f };

			VkAabbPositionsKHR& aabb = AABBs[sphereNb];
			aabb.maxX = sphereCenter.x + radius;
			aabb.maxY = sphereCenter.y + radius;
			aabb.maxZ = sphereCenter.z + radius;
			aabb.minX = sphereCenter.x - radius;
			aabb.minY = sphereCenter.y - radius;
			aabb.minZ = sphereCenter.z - radius;
		}
	}

	//creating the sphere's acceleration structure from the aabb
	_RaySphereBottomAS._AccelerationStructure.Alloc(1);
	_RaySphereBottomAS._AccelerationStructureBuffer.Alloc(1);
	_RaySphereBottomAS._AccelerationStructureMemory.Alloc(1);
	VulkanHelper::CreateRaytracedProceduralFromAABB(GAPI._VulkanUploader, _RaySphereAABBBuffer, _RaySphereBottomAS, AABBs, 29);
	VulkanHelper::UploadRaytracedGroupFromGeometry(GAPI._VulkanUploader, _RaySphereTopAS, identity(), _RaySphereBottomAS);

	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, sizeof(vec4) * 29, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereBuffer, *spheres, sizeof(vec4) * 29);


	VulkanHelper::CreateStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, sizeof(vec4) * 29, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	VulkanHelper::UploadStaticBufferHandle(GAPI._VulkanUploader, _RaySphereColour, *sphereColour, sizeof(vec4) * 29);

	AABBs.Clear();
}


void RaytraceGPU::Prepare(class GraphicsAPIManager& GAPI)
{
	GenerateSpheres(GAPI);


	//preparing hardware raytracing props
	{
		//the shaders needed
		VkShaderModule RayGenShader, MissShader, HitShader, IntersectShader;

		//compile the shaders here...
		PrepareVulkanRaytracingScripts(GAPI, RayGenShader, MissShader, HitShader, IntersectShader);
		//then create the pipeline
		PrepareVulkanRaytracingProps(GAPI, RayGenShader, MissShader, HitShader, IntersectShader);
	}

	_RayBuffer.model = identity();

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
		poolInfo.poolSizeCount	= 5;
		VkDescriptorPoolSize poolSizes[5] = {ASPoolSize, imagePoolSize, bufferPoolSize, spherePoolSize, spherePoolSize};
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
	CreateUniformBufferHandle(GAPI._VulkanUploader, _RayUniformBuffer, GAPI._nb_vk_frames, sizeof(mat4) * 3);

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//Adding our Top Level AS in each descriptor set
		VkWriteDescriptorSetAccelerationStructureKHR ASInfo{};
		ASInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		ASInfo.pAccelerationStructures		= &_RaySphereTopAS._AccelerationStructure;
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
		bufferInfo.range = sizeof(mat4) * 3;

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

		VkWriteDescriptorSet descriptorWrites[5] = { ASDescriptorWrite,  imageDescriptorWrite, descriptorWrite, sphereWrite, sphereColourWrite };
		vkUpdateDescriptorSets(GAPI._VulkanDevice, 5, descriptorWrites, 0, nullptr);
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

	//_RayObjData.euler_angles.y += AppContext.delta_time;
	//
	//it will change every frame
	{
		_RayBuffer.proj = inv_perspective_proj(_CopyScissors.extent.width, _CopyScissors.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		_RayBuffer.view = transpose(AppContext.view_mat);
		_RayBuffer.view .x.w = 0.0f;
		_RayBuffer.view .y.w = 0.0f;
		_RayBuffer.view .z.w = 0.0f;
		_RayBuffer.view .w.xyz = AppContext.camera_pos;
		//_RayMatBuffer.model = scale(_RayObjData.scale.x, _RayObjData.scale.y, _RayObjData.scale.z) * ro_intrinsic_rot(_RayObjData.euler_angles.x, _RayObjData.euler_angles.y, _RayObjData.euler_angles.z) * ro_translate(_RayObjData.pos);
	}
	//
	////UI update
	//if (SceneCanShowUI(AppContext))
	//{
	//	ImGui::SliderFloat3("Object Postion", _RayObjData.pos.scalar, -100.0f, 100.0f);
	//	ImGui::SliderFloat3("Object Rotation", _RayObjData.euler_angles.scalar, -180.0f, 180.0f);
	//	ImGui::SliderFloat3("Object Scale", _RayObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");
	//}

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

	//the buffer will change every frame as the object rotates every frame
	memcpy(_RayUniformBuffer._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_RayBuffer, sizeof(mat4) * 3);

	// changing the back buffer to be able to being written by pipeline
	VulkanHelper::ImageMemoryBarrier(commandBuffer, _RayWriteImage[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayLayout, 0, 1, &_RayDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);

	VkStridedDeviceAddressRegionKHR tmp{};
	VK_CALL_KHR(GAPIHandle._VulkanDevice, vkCmdTraceRaysKHR, commandBuffer, &_RayShaderBindingAddress[0], &_RayShaderBindingAddress[1], &_RayShaderBindingAddress[2], &tmp, _CopyScissors.extent.width, _CopyScissors.extent.height, 1);

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
	ClearModel(GAPI._VulkanDevice, _RayModel);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayDescriptorPool, nullptr);
	_RayDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _RayDescriptorLayout, nullptr);


	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
}
