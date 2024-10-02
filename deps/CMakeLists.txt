cmake_minimum_required (VERSION 3.21)

set (DEPS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set (IMGUI_SRC "${DEPS_SRC}/imgui")
set (DEPS_INC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set (DEPS_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set (DEPS_BIN "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set (DEPS_SRC_FILES
    "${IMGUI_SRC}/imgui.cpp"
    "${IMGUI_SRC}/imgui_demo.cpp"
    "${IMGUI_SRC}/imgui_draw.cpp"
    "${IMGUI_SRC}/imgui_impl_vulkan.cpp"
    "${IMGUI_SRC}/imgui_impl_glfw.cpp"
    "${IMGUI_SRC}/imgui_tables.cpp"
    "${IMGUI_SRC}/imgui_widgets.cpp")

target_include_directories(VulkanLearning PUBLIC "${DEPS_INC}/")
target_include_directories(VulkanLearning PUBLIC "${DEPS_INC}/imgui/")
target_include_directories(VulkanLearning PUBLIC "${DEPS_INC}/GLFW/")

#define lib
add_library(GLFW SHARED IMPORTED)

if (WIN32)
    set_target_properties(GLFW PROPERTIES IMPORTED_LOCATION "${DEPS_BIN}/GLFW/glfw3.dll" )
    set_target_properties(GLFW PROPERTIES IMPORTED_IMPLIB "${DEPS_LIBS}/GLFW/glfw3dll.lib" )
elseif(APPLE)
    set_target_properties(GLFW PROPERTIES IMPORTED_LOCATION "${DEPS_BIN}/GLFW/libglfw.3.dylib" )
    set(CMAKE_BUILD_RPATH "${DEPS_BIN}/GLFW/libglfw.3.dylib")
    #set_target_properties(GLFW PROPERTIES IMPORTED_IMPLIB "${DEPS_LIBS}/GLFW/libglfw3.a" )
endif()

# Add libraries
target_link_libraries(VulkanLearning PUBLIC "GLFW")


target_sources(VulkanLearning PUBLIC ${DEPS_SRC_FILES})
