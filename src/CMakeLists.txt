project(main CXX)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF) # Don't know if we actually need this
# Force MSVC static runtime before project()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "main/*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "main/*.h")
add_executable(main ${sources} ${headers})

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/_release" CACHE PATH "" FORCE)
install(
        TARGETS main
        RUNTIME DESTINATION "$<CONFIG>"
)
install(
        DIRECTORY "${CMAKE_SOURCE_DIR}/resources/" DESTINATION "$<CONFIG>/resources"
)

set_compiler_flags(main)
set_target_properties(main PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/$<CONFIG>/bin
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Dependencies

## Discovery

find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(cpptrace CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

option(TRACY_ENABLE "" OFF)
set(TRACY_TIMER_FALLBACK ON)
FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.12.2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(tracy)

include(cmake/soloud.cmake)
target_compile_definitions(soloud PRIVATE
        WITH_MINIAUDIO
)

## Config / Linking

target_compile_definitions(main PRIVATE VULKAN_HPP_NO_CONSTRUCTORS VULKAN_HPP_NO_SPACESHIP_OPERATOR VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_link_libraries(main PRIVATE Vulkan::Headers)
target_link_libraries(main PRIVATE GPUOpen::VulkanMemoryAllocator unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)
target_link_libraries(main PRIVATE glfw)
target_compile_definitions(main PRIVATE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RIGHT_HANDED GLM_ENABLE_EXPERIMENTAL)
target_link_libraries(main PRIVATE glm::glm)
target_link_libraries(main PRIVATE unofficial::shaderc::shaderc)
target_include_directories(main PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(main PRIVATE Vulkan::UtilityHeaders) # Unused-> Vulkan::SafeStruct Vulkan::LayerSettings Vulkan::CompilerConfiguration
target_link_libraries(main PRIVATE cpptrace::cpptrace)
target_compile_definitions(main PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(main PRIVATE imgui::imgui)
target_link_libraries(main PRIVATE TracyClient)
target_link_libraries(main PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(main PRIVATE fastgltf::fastgltf)
target_link_libraries(main PRIVATE soloud)
