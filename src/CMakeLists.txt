cmake_minimum_required (VERSION 3.8)

# set all source files
set (SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/GraphicsAPIManager.cpp" 
	"${CMAKE_CURRENT_SOURCE_DIR}/RasterTriangle.cpp")

# add to executable
target_sources(VulkanLearning PUBLIC ${SRC_FILES})