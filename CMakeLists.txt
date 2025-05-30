﻿# CMakeList.txt : CMake project for VulkanLearning, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.24)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project ("RaytracedCel")

#we at least need C++ 11 (with imgui dependency)
#set_property(TARGET VulkanLearning PROPERTY CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)

#define project directory
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
set(INC_DIR "${CMAKE_SOURCE_DIR}/include")

#add include directory
include_directories("${INC_DIR}/")

# Add source to this project's executable.
add_executable (RaytracedCel "main.cpp")

#if (CMAKE_VERSION VERSION_GREATER 3.12)
#    set_property(TARGET RaytracedCel PROPERTY CXX_STANDARD 20)
#endif()

# Add sub projects.
add_subdirectory(${SRC_DIR})
add_subdirectory(${DEPS_DIR})

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

# Add libraries
target_link_libraries(RaytracedCel PUBLIC "Vulkan::Vulkan")
target_link_libraries(RaytracedCel PUBLIC "Vulkan::shaderc_combined")


# Copy Dll
if (WIN32)
    add_custom_command(TARGET RaytracedCel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E  copy
            "$<TARGET_RUNTIME_DLLS:RaytracedCel>"
            "$<TARGET_FILE_DIR:RaytracedCel>"
    COMMAND_EXPAND_LISTS)
endif()
