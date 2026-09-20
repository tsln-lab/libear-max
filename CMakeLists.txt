# libear-max: Max externals wrapping libear (ITU-R BS.2127 / EBU ADM Renderer)
#
# Package layout follows the Min-DevKit conventions:
#   source/min-api        Min C++ API for Max (submodule, brings max-sdk-base)
#   source/libear         EBU libear (submodule)
#   source/projects/*     one folder per external
#   externals/            build output (git-ignored)
#   tests/                unit test binaries (git-ignored)

cmake_minimum_required(VERSION 3.19)
project(libear-max LANGUAGES C CXX)

enable_testing()

if (APPLE)
    if (${CMAKE_GENERATOR} MATCHES "Xcode")
        if (${XCODE_VERSION} VERSION_LESS 10)
            message(FATAL_ERROR "Xcode 10 or higher is required.")
        elseif (${XCODE_VERSION} VERSION_GREATER_EQUAL 12)
            set(C74_BUILD_FAT YES)
        endif ()
    endif ()

    if (NOT CMAKE_OSX_ARCHITECTURES)
        if (C74_BUILD_FAT)
            set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architecture" FORCE)
        else ()
            set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR} CACHE STRING "macOS architecture" FORCE)
        endif ()
        message(STATUS "CMAKE_OSX_ARCHITECTURES set to ${CMAKE_OSX_ARCHITECTURES}")
    endif ()
endif ()

if (MSVC)
    # Max externals are linked against the static MSVC runtime (see
    # max-sdk-base/script/max-pretarget.cmake). libear is added from this
    # directory scope, so give it the same runtime to avoid LNK2038 mismatches.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_CXX_FLAGS_DEBUG          "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG")
    set(CMAKE_C_FLAGS_DEBUG            "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_C_FLAGS_MINSIZEREL       "/MT /O1 /Ob1 /D NDEBUG")
    set(CMAKE_C_FLAGS_RELEASE          "/MT /O2 /Ob2 /D NDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "/MT /Zi /O2 /Ob1 /D NDEBUG")
endif ()

# Misc setup and subroutines from the Min API (package-info.json, SUBDIRLIST, ...)
include(${CMAKE_CURRENT_SOURCE_DIR}/source/min-api/script/min-package.cmake)

#############################################################
# libear (static library, linked into every external)
#############################################################

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(EAR_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(EAR_EXAMPLES OFF CACHE BOOL "" FORCE)
set(EAR_PACKAGE_AND_INSTALL OFF CACHE BOOL "" FORCE)
set(EAR_HIDE_INTERNAL_SYMBOLS ON CACHE BOOL "" FORCE)

if (APPLE)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _libear_max_arch_count)
    if (_libear_max_arch_count GREATER 1)
        # libear's per-ISA SIMD object libraries pass x86-only flags that do not
        # apply to a universal (x86_64 + arm64) compile; use the portable path.
        set(EAR_SIMD OFF CACHE BOOL "" FORCE)
    endif ()
endif ()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/libear EXCLUDE_FROM_ALL)

#############################################################
# Externals: one project per folder in source/projects
#############################################################

SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source/projects)
foreach (project_dir ${PROJECT_DIRS})
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir}/CMakeLists.txt")
        message(STATUS "Generating: ${project_dir}")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir})
    endif ()
endforeach ()
