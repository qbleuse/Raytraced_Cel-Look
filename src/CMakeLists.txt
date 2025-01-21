cmake_minimum_required (VERSION 3.8)

# set all source files
set (SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/GraphicsAPIManager.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/RasterTriangle.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/VulkanHelper.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/RasterObject.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/ImGuiHelper.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/RaytraceCPU.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/RaytraceGPU.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/DefferedRendering.cpp")

# add to executable
target_sources(VulkanLearning PUBLIC ${SRC_FILES})