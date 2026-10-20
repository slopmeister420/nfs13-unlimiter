cmake_minimum_required(VERSION 3.20)
project(NFS13Unlimiter CXX)

# libmem is distributed as -static-md, so the whole thing must be /MD
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "NFS13.exe is x86 - configure with -A Win32")
endif()

set(LIBMEM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/libmem-i686)

add_library(NFS13Unlimiter SHARED src/main.cpp)
target_include_directories(NFS13Unlimiter PRIVATE ${LIBMEM_DIR}/include)
target_compile_definitions(NFS13Unlimiter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(NFS13Unlimiter PRIVATE /O2 /W3 /EHa)
target_link_libraries(NFS13Unlimiter PRIVATE
    $<$<CONFIG:Debug>:${LIBMEM_DIR}/lib/debug/libmem.lib>
    $<$<NOT:$<CONFIG:Debug>>:${LIBMEM_DIR}/lib/release/libmem.lib>
    user32 psapi shell32 ntdll ws2_32 userenv bcrypt advapi32
)
set_target_properties(NFS13Unlimiter PROPERTIES OUTPUT_NAME "NFS13Unlimiter" SUFFIX ".asi")
