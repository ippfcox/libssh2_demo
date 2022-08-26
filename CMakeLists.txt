cmake_minimum_required(VERSION 3.10)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

project(libssh2_demo VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${src})
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_FLAGS "/NODEFAULTLIB:MSVCRT")

include_directories("lib/libssh2/include")
include_directories("lib/spdlog/include")

target_link_libraries(${PROJECT_NAME} PUBLIC
    # ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/lib-vc2010-32/glfw3.lib
    # opengl32.lib
    "legacy_stdio_definitions.lib"
    "ws2_32.lib"
    "Crypt32.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/libssh2/lib/libssh2.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/openssl/lib/libcrypto.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/openssl/lib/libssl.lib"
)