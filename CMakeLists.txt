# CMakeList.txt : CMake project for VulkanLearning, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.24)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("VulkanLearning")

#define project directory
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
set(INC_DIR "${CMAKE_SOURCE_DIR}/include")

#add include directory
include_directories("${INC_DIR}/")

# Add source to this project's executable.
add_executable (VulkanLearning "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanLearning PROPERTY CXX_STANDARD 20)
endif()

# Add sub projects.
add_subdirectory(${SRC_DIR})
add_subdirectory(${DEPS_DIR})

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

# Add libraries
target_link_libraries(VulkanLearning PUBLIC "Vulkan::Vulkan")
target_link_libraries(VulkanLearning PUBLIC "Vulkan::shaderc_combined")


# Copy Dll
add_custom_command(TARGET VulkanLearning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E  copy
        "$<TARGET_RUNTIME_DLLS:VulkanLearning>"
        "$<TARGET_FILE_DIR:VulkanLearning>"
		COMMAND_EXPAND_LISTS)