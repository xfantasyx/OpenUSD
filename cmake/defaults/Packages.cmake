#
# Copyright 2016 Pixar
#
# Licensed under the terms set forth in the LICENSE.txt file available at
# https://openusd.org/license.
#

# Save the current value of BUILD_SHARED_LIBS and restore it at
# the end of this file, since some of the Find* modules invoked
# below may wind up stomping over this value.
set(build_shared_libs "${BUILD_SHARED_LIBS}")

# Core USD Package Requirements 
# ----------------------------------------------

# Threads.  Save the libraries needed in PXR_THREAD_LIBS;  we may modify
# them later.  We need the threads package because some platforms require
# it when using C++ functions from #include <thread>.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
set(PXR_THREAD_LIBS "${CMAKE_THREAD_LIBS_INIT}")

if((PXR_ENABLE_PYTHON_SUPPORT AND PXR_USE_BOOST_PYTHON) OR PXR_ENABLE_OPENVDB_SUPPORT)
    # Find Boost package before getting any boost specific components as we need to
    # disable boost-provided cmake config, based on the boost version found.
    find_package(Boost REQUIRED)

    # Boost provided cmake files (introduced in boost version 1.70) result in 
    # inconsistent build failures on different platforms, when trying to find boost 
    # component dependencies like python, etc. Refer some related
    # discussions:
    # https://github.com/boostorg/python/issues/262#issuecomment-483069294
    # https://github.com/boostorg/boost_install/issues/12#issuecomment-508683006
    #
    # Hence to avoid issues with Boost provided cmake config, Boost_NO_BOOST_CMAKE
    # is enabled by default for boost version 1.70 and above. If a user explicitly 
    # set Boost_NO_BOOST_CMAKE to Off, following will be a no-op.
    option(Boost_NO_BOOST_CMAKE "Disable boost-provided cmake config" ON)
    if (Boost_NO_BOOST_CMAKE)
      message(STATUS "Disabling boost-provided cmake config")
    endif()
endif()

if(PXR_ENABLE_PYTHON_SUPPORT)
    # 1--Python.
    macro(setup_python_package package)
        find_package(${package} COMPONENTS Interpreter Development REQUIRED)

        # Set up versionless variables so that downstream libraries don't
        # have to worry about which Python version is being used.
        set(PYTHON_EXECUTABLE "${${package}_EXECUTABLE}")
        set(PYTHON_INCLUDE_DIRS "${${package}_INCLUDE_DIRS}")
        set(PYTHON_VERSION_MAJOR "${${package}_VERSION_MAJOR}")
        set(PYTHON_VERSION_MINOR "${${package}_VERSION_MINOR}")

        # Convert paths to CMake path format on Windows to avoid string parsing
        # issues when we pass PYTHON_EXECUTABLE or PYTHON_INCLUDE_DIRS to
        # pxr_library or other functions.
        if(WIN32)
            file(TO_CMAKE_PATH ${PYTHON_EXECUTABLE} PYTHON_EXECUTABLE)
            file(TO_CMAKE_PATH ${PYTHON_INCLUDE_DIRS} PYTHON_INCLUDE_DIRS)
        endif()

        # PXR_PY_UNDEFINED_DYNAMIC_LOOKUP might be explicitly set when 
        # packaging wheels, or when cross compiling to a Python environment 
        # that is not the current interpreter environment.
        # If it was not explicitly set to ON or OFF, then determine whether 
        # Python was statically linked to its runtime library by fetching the
        # sysconfig variable LDLIBRARY, and set the variable accordingly.
        # If the variable does not exist, PXR_PY_UNDEFINED_DYNAMIC_LOOKUP will
        # default to OFF. On Windows, LDLIBRARY does not exist, as the default
        # will always be OFF.
        if((NOT WIN32) AND (NOT DEFINED PXR_PY_UNDEFINED_DYNAMIC_LOOKUP))
            execute_process(COMMAND ${PYTHON_EXECUTABLE} "-c" "import sysconfig;print(sysconfig.get_config_var('LDLIBRARY'))"
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE PXR_PYTHON_LINKED_LIBRARY
            )
            get_filename_component(PXR_PYTHON_LINKED_LIBRARY_EXT ${PXR_PYTHON_LINKED_LIBRARY} LAST_EXT)
            if(PXR_PYTHON_LINKED_LIBRARY_EXT STREQUAL ".a")
                set(PXR_PY_UNDEFINED_DYNAMIC_LOOKUP ON)
                message(STATUS 
                        "PXR_PY_UNDEFINED_DYNAMIC_LOOKUP wasn't specified, forced ON because Python statically links ${PXR_PYTHON_LINKED_LIBRARY}")
            endif()
        endif()

        # This option indicates that we don't want to explicitly link to the
        # python libraries. See BUILDING.md for details.
        if(PXR_PY_UNDEFINED_DYNAMIC_LOOKUP AND NOT WIN32)
            set(PYTHON_LIBRARIES "")
        else()
            set(PYTHON_LIBRARIES PYTHON::PYTHON)
        endif()
    endmacro()

    # USD builds only work with Python3
    setup_python_package(Python3)

    if(PXR_USE_BOOST_PYTHON)
        if(WIN32 AND PXR_USE_DEBUG_PYTHON)
            set(Boost_USE_DEBUG_PYTHON ON)
        endif()

        # Manually specify VS2022, 2019, and 2017 as USD's supported compiler versions
        if(WIN32)
            set(Boost_COMPILER "-vc143;-vc142;-vc141")
        endif()

        # As of boost 1.67 the boost_python component name includes the
        # associated Python version (e.g. python27, python36). 
        # XXX: After boost 1.73, boost provided config files should be able to 
        # work without specifying a python version!
        # https://github.com/boostorg/boost_install/blob/master/BoostConfig.cmake

        # Find the component under the versioned name and then set the generic
        # Boost_PYTHON_LIBRARY variable so that we don't have to duplicate this
        # logic in each library's CMakeLists.txt.
        set(python_version_nodot "${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}")
        find_package(Boost
            COMPONENTS
            python${python_version_nodot}
            REQUIRED
        )
        set(Boost_PYTHON_LIBRARY "${Boost_PYTHON${python_version_nodot}_LIBRARY}")
    endif()

    # --Jinja2
    find_package(Jinja2)
else()
    # -- Python
    # A Python interpreter is still required for certain build options.
    if (PXR_BUILD_DOCUMENTATION OR PXR_BUILD_TESTS
        OR PXR_VALIDATE_GENERATED_CODE)

        # We only need to check for Python3 components
        find_package(Python3 COMPONENTS Interpreter)
        set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    endif()
endif()


# --TBB
find_package(TBB REQUIRED COMPONENTS tbb)
add_definitions(${TBB_DEFINITIONS})


if(NOT TBB_tbb_LIBRARY)
    set(TBB_tbb_LIBRARY TBB::tbb)
endif()

# --math
if(WIN32)
    # Math functions are linked automatically by including math.h on Windows.
    set(M_LIB "")
else()
    set(M_LIB m)
endif()

if (NOT PXR_MALLOC_LIBRARY)
    if (NOT WIN32)
        message(STATUS "Using default system allocator because PXR_MALLOC_LIBRARY is unspecified")
    endif()
endif()

# Developer Options Package Requirements
# ----------------------------------------------
if (PXR_BUILD_DOCUMENTATION)
    find_program(DOXYGEN_EXECUTABLE
        NAMES doxygen
    )
    if (EXISTS ${DOXYGEN_EXECUTABLE})                                        
        message(STATUS "Found doxygen: ${DOXYGEN_EXECUTABLE}") 
    else()
        message(FATAL_ERROR 
                "doxygen not found, required for PXR_BUILD_DOCUMENTATION")
    endif()

    if (PXR_BUILD_HTML_DOCUMENTATION)
        find_program(DOT_EXECUTABLE
            NAMES dot
        )
        if (EXISTS ${DOT_EXECUTABLE})
            message(STATUS "Found dot: ${DOT_EXECUTABLE}") 
        else()
            message(FATAL_ERROR
                    "dot not found, required for PXR_BUILD_DOCUMENTATION")
        endif()
    endif()
endif()

# Imaging Components Package Requirements
# ----------------------------------------------

if (PXR_BUILD_IMAGING)
    # --OpenImageIO
    if (PXR_BUILD_OPENIMAGEIO_PLUGIN)
        set(REQUIRES_Imath TRUE)
        find_package(OpenImageIO REQUIRED)
        add_definitions(-DPXR_OIIO_PLUGIN_ENABLED)
        if (OIIO_idiff_BINARY)
            set(IMAGE_DIFF_TOOL ${OIIO_idiff_BINARY} CACHE STRING "Uses idiff for image diffing")
        endif()
        if(NOT OIIO_LIBRARIES)
                set(OIIO_LIBRARIES OpenImageIO::OpenImageIO)
        endif()
    endif()
    # --OpenColorIO
    if (PXR_BUILD_OPENCOLORIO_PLUGIN)
        find_package(OpenColorIO REQUIRED)
        add_definitions(-DPXR_OCIO_PLUGIN_ENABLED)

        if(NOT OCIO_LIBRARIES)
            set(OCIO_LIBRARIES OpenColorIO::OpenColorIO)
        endif()
    endif()
    # --OpenGL
    if (PXR_ENABLE_GL_SUPPORT AND NOT PXR_APPLE_EMBEDDED)
        # Prefer legacy GL library over GLVND libraries if both
        # are installed.
        if (POLICY CMP0072)
            cmake_policy(SET CMP0072 OLD)
        endif()

        if(ANDROID)
            # get_target_property(INCLUDE_DIRS EGL INTERFACE_INCLUDE_DIRECTORIES)
            # message(FATAL_ERROR "Interface include directories: ${INCLUDE_DIRS}")

            # find_package (OpenGL REQUIRED)
  

            SET(OPENGL_FOUND TRUE)
            SET(OPENGL_GLU_FOUND TRUE)


            # FIND_PATH( EGL_INCLUDE_DIR
            #     EGL/egl.h
            #     "${ANDROID_STANDALONE_TOOLCHAIN}/usr/include"
            # )
            # FIND_LIBRARY( EGL_LIBRARIES
            #     NAMES
            #         EGL
            #     PATHS
            #         "${ANDROID_STANDALONE_TOOLCHAIN}/usr/lib"
            # )

            FIND_PATH( GLESv3_INCLUDE_DIR
                GLES3/gl3.h
                "${ANDROID_STANDALONE_TOOLCHAIN}/usr/include"
            )

            

            if(NOT TARGET OpenGL::GL)
                add_library(OpenGL::GL INTERFACE IMPORTED)

                set_property(TARGET OpenGL::GL APPEND PROPERTY INTERFACE_LINK_LIBRARIES EGL GLESv3)

                set_target_properties(OpenGL::GL PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${GLESv3_INCLUDE_DIR}"
                # INTERFACE_LINK_LIBRARIES "${OPENGL_LIBRARIES}"
                )
            endif()

        else()
            find_package(OpenGL REQUIRED)
        endif()
        add_definitions(-DPXR_GL_SUPPORT_ENABLED)
    endif()
    # --Metal
    if (PXR_ENABLE_METAL_SUPPORT)
        add_definitions(-DPXR_METAL_SUPPORT_ENABLED)
    endif()
    if (PXR_ENABLE_VULKAN_SUPPORT)
        if(ANDROID)

            find_package(Vulkan REQUIRED)
            list(APPEND VULKAN_LIBS Vulkan::Vulkan)

            set(VULKAN_EXT_PATH "${PROJECT_SOURCE_DIR}/android_support")

            if(NOT TARGET shaderc_combined)
                FIND_PATH( SHADERC_INCLUDE_DIR
                    shaderc/shaderc.h
                    "${ANDROID_NDK}/sources/third_party/shaderc/include"
                )
                FIND_LIBRARY( SHADERC_LIBRARIES
                    NAMES
                        shaderc
                    PATHS
                        "${ANDROID_NDK}/sources/third_party/shaderc/libs/c++_static/${ANDROID_ABI}"
                )

                add_library(shaderc_combined INTERFACE IMPORTED)
                set_target_properties(shaderc_combined PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${SHADERC_INCLUDE_DIR};${VULKAN_EXT_PATH}"
                INTERFACE_LINK_LIBRARIES "${SHADERC_LIBRARIES}"
                )

                # add_library(vulkan_shaderc_lib STATIC IMPORTED)
                # set_target_properties(vulkan_shaderc_lib PROPERTIES
                #     INTERFACE_INCLUDE_DIRECTORIES "${SHADERC_INCLUDE_DIR};${VULKAN_EXT_PATH}"
                #     IMPORTED_LOCATION_RELEASE "${SHADERC_LIBRARIES}"
                #     IMPORTED_LOCATION_DEBUG "${SHADERC_LIBRARIES}"
                #     IMPORTED_LOCATION_RELWITHDEBINFO "${SHADERC_LIBRARIES}"
                #     )
            endif()

            list(APPEND VULKAN_LIBS shaderc_combined)
            add_definitions(-DPXR_VULKAN_SUPPORT_ENABLED)
            
        else()
            message(STATUS "Enabling experimental feature Vulkan support")
            if (EXISTS $ENV{VULKAN_SDK})
                # Prioritize the VULKAN_SDK includes and packages before any system
                # installed headers. This is to prevent linking against older SDKs
                # that may be installed by the OS.
                # XXX This is fixed in cmake 3.18+
                include_directories(BEFORE SYSTEM $ENV{VULKAN_SDK} $ENV{VULKAN_SDK}/include $ENV{VULKAN_SDK}/lib $ENV{VULKAN_SDK}/source)
                set(ENV{PATH} "$ENV{VULKAN_SDK}:$ENV{VULKAN_SDK}/include:$ENV{VULKAN_SDK}/lib:$ENV{VULKAN_SDK}/source:$ENV{PATH}")
                find_package(Vulkan REQUIRED)
                list(APPEND VULKAN_LIBS Vulkan::Vulkan)

                # Find the extra vulkan libraries we need
                # set(EXTRA_VULKAN_LIBS shaderc_combined)
                # if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
                #     set(EXTRA_VULKAN_LIBS shaderc_combinedd)
                # endif()
                # foreach(EXTRA_LIBRARY ${EXTRA_VULKAN_LIBS})
                #     find_library("${EXTRA_LIBRARY}_PATH" NAMES "${EXTRA_LIBRARY}" PATHS $ENV{VULKAN_SDK}/lib)
                #     list(APPEND VULKAN_LIBS "${${EXTRA_LIBRARY}_PATH}")
                # endforeach()

                if(NOT TARGET shaderc_combined)
                    find_library(shaderc_combined_release_lib_path NAMES "shaderc_combined" PATHS $ENV{VULKAN_SDK}/lib)
                    find_library(shaderc_combined_debug_lib_path NAMES "shaderc_combinedd" PATHS $ENV{VULKAN_SDK}/lib)

                    add_library(shaderc_combined STATIC IMPORTED)
                    set_target_properties(shaderc_combined PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${shaderc_combined_release_lib_path}"
                    IMPORTED_LOCATION_DEBUG "${shaderc_combined_debug_lib_path}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${shaderc_combined_release_lib_path}"
                    )
                endif()

                # get_target_property(test_vulkan_shader_dbg_lib shaderc_combined IMPORTED_IMPLIB_DEBUG)

                list(APPEND VULKAN_LIBS shaderc_combined)

                # message(FATAL_ERROR "VULKAN_LIBS=${VULKAN_LIBS}")

                # Find the OS specific libs we need
                if (UNIX AND NOT APPLE AND NOT ANDROID)
                    find_package(X11 REQUIRED)
                    list(APPEND VULKAN_LIBS ${X11_LIBRARIES})
                elseif (WIN32)
                    # No extra libs required
                endif()

                add_definitions(-DPXR_VULKAN_SUPPORT_ENABLED)
                # add_definitions(-DVMA_VULKAN_VERSION=1001000)
                
            else()
                message(FATAL_ERROR "VULKAN_SDK not valid")
            endif()
        endif()
        
    endif()
    # --Opensubdiv
    set(OPENSUBDIV_USE_GPU ${PXR_BUILD_GPU_SUPPORT})
    
    if (EMSCRIPTEN )
        set(OPENSUBDIV_USE_GPU OFF)
    endif()
    
    find_package(OpenSubdiv 3 REQUIRED)
    set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_OSDCPU_LIBRARY})

    if(OPENSUBDIV_USE_GPU)
        list(APPEND OPENSUBDIV_LIBRARIES ${OPENSUBDIV_OSDGPU_LIBRARY})
    endif()
    
    # --Ptex
    if (PXR_ENABLE_PTEX_SUPPORT)
        find_package(PTex REQUIRED)
        add_definitions(-DPXR_PTEX_SUPPORT_ENABLED)
        if(NOT PTEX_LIBRARY)
            if(TARGET Ptex::Ptex_dynamic)
                set(PTEX_LIBRARY Ptex::Ptex_dynamic)
            else()
                set(PTEX_LIBRARY Ptex::Ptex_static)
            endif()
            
        endif()
    endif()
    # --OpenVDB
    if (PXR_ENABLE_OPENVDB_SUPPORT)
        set(REQUIRES_Imath TRUE)
        find_package(OpenVDB REQUIRED)
        add_definitions(-DPXR_OPENVDB_SUPPORT_ENABLED)

        if(NOT OPENVDB_LIBRARY)
            if(TARGET openvdb_shared)
                set(OPENVDB_LIBRARY openvdb_shared)
            else()
                set(OPENVDB_LIBRARY openvdb_static)
            endif()
        endif()
    endif()
    # --X11
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(X11)
    endif()
    # --Embree
    if (PXR_BUILD_EMBREE_PLUGIN)
        find_package(Embree REQUIRED)
    endif()
endif()

if (PXR_BUILD_USDVIEW)
    # --PySide
    find_package(PySide REQUIRED)
    # --PyOpenGL
    find_package(PyOpenGL REQUIRED)
endif()

# Third Party Plugin Package Requirements
# ----------------------------------------------
if (PXR_BUILD_PRMAN_PLUGIN)
    find_package(Renderman REQUIRED)
endif()

if (PXR_BUILD_ALEMBIC_PLUGIN)
    find_package(Alembic REQUIRED)
    set(REQUIRES_Imath TRUE)
    if (PXR_ENABLE_HDF5_SUPPORT)
        find_package(HDF5 REQUIRED
            COMPONENTS
                HL
            REQUIRED
        )

        if(NOT HDF5_LIBRARIES)
            set(HDF5_LIBRARIES ${HDF5_EXPORT_LIBRARIES})
        endif()
    endif()

    if(NOT ALEMBIC_LIBRARIES)
        set(ALEMBIC_FOUND TRUE)
        set(ALEMBIC_LIBRARIES Alembic::Alembic)
    endif()
endif()

if (PXR_BUILD_DRACO_PLUGIN)
    find_package(Draco REQUIRED)

    if(NOT DRACO_LIBRARY)
        set(DRACO_LIBRARY draco::draco)
    endif()
endif()

if (PXR_ENABLE_MATERIALX_SUPPORT)
    find_package(MaterialX REQUIRED)
    add_definitions(-DPXR_MATERIALX_SUPPORT_ENABLED)
endif()

if(PXR_ENABLE_OSL_SUPPORT)
    find_package(OSL REQUIRED)
    set(REQUIRES_Imath TRUE)
    add_definitions(-DPXR_OSL_SUPPORT_ENABLED)
endif()

if (PXR_BUILD_ANIMX_TESTS)
    find_package(AnimX REQUIRED)
endif()

# ----------------------------------------------

# Try and find Imath or fallback to OpenEXR
# Use ImathConfig.cmake, 
# Refer: https://github.com/AcademySoftwareFoundation/Imath/blob/main/docs/PortingGuide2-3.md#openexrimath-3x-only
if(REQUIRES_Imath)
    find_package(Imath CONFIG)
    if (NOT Imath_FOUND)
        MESSAGE(STATUS "Imath not found. Looking for OpenEXR instead.")
        find_package(OpenEXR REQUIRED)
    endif()
endif()

set(BUILD_SHARED_LIBS "${build_shared_libs}")
