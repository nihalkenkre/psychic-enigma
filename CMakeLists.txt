cmake_minimum_required(VERSION 3.10)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

project(psychic-enigma LANGUAGES C CXX)

file(GLOB PROJECT_SRCS src/*)
list(FILTER PROJECT_SRCS EXCLUDE REGEX "imgui.ini")
file(GLOB IMGUI_SRCS ../imgui/*.cpp ../imgui/*.h ../imgui/backends/imgui_impl_sdl3.* ../imgui/backends/imgui_impl_vulkan.*)
file(GLOB IMGUI_FILE_DLG_SRCS ../ImGuiFileDialog/ImGuiFileDialog*.h ../ImGuiFileDialog/ImGuiFileDialog*.cpp)
file(GLOB VOLK_SRCS ../volk/volk.c)

source_group("ImGui" FILES ${IMGUI_SRCS})
source_group("ImGuiFileDialog" FILES ${IMGUI_FILE_DLG_SRCS})

add_executable(${PROJECT_NAME} ${PROJECT_SRCS} ${IMGUI_SRCS} ${IMGUI_FILE_DLG_SRCS} ${VOLK_SRCS})

add_compile_definitions(SDL_MAIN_USE_CALLBACKS _HAS_EXCEPTION=0 _CRT_SECURE_NO_WARNINGS VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
   target_compile_options(${PROJECT_NAME} PRIVATE "/EHs-" "/EHc-" "/GR-")
   add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
   target_compile_options(${PROJECT_NAME} PRIVATE "-fno-exceptions" "-fno-rtti")
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE ../SDL/include ../imgui ../imgui/backends ../ImGuiFileDialog ../glm ../cglm/include ../cgltf ../stb src/ ../Vulkan-Headers/build/install/include ../volk ../vma/include)

target_link_directories(${PROJECT_NAME} PRIVATE $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/../SDL/build/Debug,> $<IF:$<CONFIG:MinSizeRel>,${CMAKE_SOURCE_DIR}/../SDL/build/MinSizeRel,>)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3-static winmm imm32 setupapi version)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND python ${CMAKE_SOURCE_DIR}/build_slang.py ${CMAKE_SOURCE_DIR} $<CONFIG> $<TARGET_FILE_DIR:${PROJECT_NAME}>)