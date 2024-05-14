cmake_minimum_required (VERSION 3.8)

set (DEPS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set (IMGUI_SRC "${DEPS_SRC}/imgui")
set (DEPS_INC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set (DEPS_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/libs")

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

target_sources(VulkanLearning PUBLIC ${DEPS_SRC_FILES})