#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

option(BUILD_WITH_L0 "Build level zero by default" ON)
if(BUILD_WITH_L0 AND "${NEO_BITS}" STREQUAL "64")

    set(TARGET_NAME_L0 ze_intel_gpu)

    # Level Zero third party detection
    if(DEFINED LEVEL_ZERO_ROOT)
        get_filename_component(LEVEL_ZERO_ROOT "${LEVEL_ZERO_ROOT}" ABSOLUTE)
    else()
        get_filename_component(LEVEL_ZERO_ROOT_tmp "${NEO_SOURCE_DIR}/../level_zero" ABSOLUTE)
        # Level Zero Headers if read from the git repo are in include/core & include/tools.
        # To support the installation path of level_zero headers which is include/level_zero/*
        # the header files are combined into the path include/level_zero/* in the commands below.
        if(IS_DIRECTORY "${LEVEL_ZERO_ROOT_tmp}")
            set(CUSTOM_L0_INCLUDE_PATH "${LEVEL_ZERO_ROOT_tmp}/include/level_zero/")
            file(GLOB LEVEL_ZERO_SOURCE_HEADERS
                "${LEVEL_ZERO_ROOT_tmp}/include/core/*"
                "${LEVEL_ZERO_ROOT_tmp}/include/tools/*"
            )
            file(MAKE_DIRECTORY ${CUSTOM_L0_INCLUDE_PATH})
            file(COPY ${LEVEL_ZERO_SOURCE_HEADERS} DESTINATION ${CUSTOM_L0_INCLUDE_PATH})
            set(LEVEL_ZERO_ROOT "${LEVEL_ZERO_ROOT_tmp}")
        endif()
    endif()

    project(level-zero-gpu VERSION 0.4.1)
    include(cmake/source_tree.cmake)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/platforms.cmake AND NOT SKIP_L0_UNIT_TESTS)
        include(cmake/${BRANCH_TYPE}/platforms.cmake)
    endif()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(LevelZero)
    if(NOT LevelZero_FOUND)
      message(STATUS "Level zero headers not found")
      return()
    endif()

    execute_process (
        COMMAND git rev-parse --short=4 origin/integration
        OUTPUT_VARIABLE L0_GIT_HEAD_COMMIT_SHA
    )

    string(STRIP "${L0_GIT_HEAD_COMMIT_SHA}" L0_GIT_HEAD_COMMIT_SHA)

    add_definitions( -DL0_PROJECT_VERSION_MAJOR="${PROJECT_VERSION_MAJOR}" )
    add_definitions( -DL0_PROJECT_VERSION_MINOR="${PROJECT_VERSION_MINOR}" )

    add_definitions( -DZE_ENABLE_OCL_INTEROP=1)

    # Generate VERSION and VERSION_SUFFIX (if building in tree) files to
    # facilitate artifact publishing.
    file(WRITE "${CMAKE_BINARY_DIR}/VERSION" "${PROJECT_VERSION}")
    find_program(GIT_EXE NAMES "git")
    if(GIT_EXE AND EXISTS "${NEO_SOURCE_DIR}/.git")
        if(MSVC)
            execute_process(
                COMMAND CMD /c git rev-list --count HEAD
                OUTPUT_VARIABLE VERSION_SUFFIX
            )
        else()
            execute_process(
                COMMAND git rev-list --count HEAD
                OUTPUT_VARIABLE VERSION_SUFFIX
            )
        endif()
        # ensure no trailing newlines are written to the file
        string(STRIP "${VERSION_SUFFIX}" VERSION_SUFFIX)
        file(WRITE "${CMAKE_BINARY_DIR}/VERSION_SUFFIX" "${VERSION_SUFFIX}")
    endif()

    #Define a path for custom commands to work around MSVC
    set(CUSTOM_COMMAND_BINARY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    if(MSVC)
        #MSVC implicitly adds $<CONFIG> to the output path
        if(NOT "${CMAKE_GENERATOR}" STREQUAL "Ninja")
            set(CUSTOM_COMMAND_BINARY_DIR ${CUSTOM_COMMAND_BINARY_DIR}/$<CONFIG>)
        endif()
    endif()

    if(UNIX)
        # Load GNUInstallDirs to determine install targets for Linux packages
        include(GNUInstallDirs)
    endif()

    if(NOT MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -fPIC")
    endif()

    set(L0_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT DEFINED COMPUTE_RUNTIME_DIR)
        get_filename_component(COMPUTE_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
    endif()
    # Option to disable tests
    option(${PROJECT_NAME}_BUILD_TESTS "Build unit tests." ON)

    if(SKIP_UNIT_TESTS)
        set(SKIP_L0_UNIT_TESTS TRUE)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/l0_tests.cmake)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/l0_tests.cmake)
    endif()

    # Copy third_party_binaries to output BIN folder
    add_custom_target(copy_third_party_files)
    set_target_properties(copy_third_party_files PROPERTIES FOLDER ${TARGET_NAME_L0})

    if(DEFINED NEO__IGC_TARGETS)
        if(WIN32)
            add_dependencies(copy_third_party_files copy_compiler_files)
        else()
            add_dependencies(copy_third_party_files ${NEO__IGC_TARGETS})
            foreach(TARGET_tmp ${NEO__IGC_TARGETS})
                if(UNIX)
                    add_custom_command(
                      TARGET copy_third_party_files
                      PRE_BUILD
                      COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_SONAME_FILE:${TARGET_tmp}> "${CUSTOM_COMMAND_BINARY_DIR}/"
                    )
                endif()
                add_custom_command(
                  TARGET copy_third_party_files
                  PRE_BUILD
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${TARGET_tmp}> "${CUSTOM_COMMAND_BINARY_DIR}/"
                )
            endforeach()
        endif()
    else()
        if(DEFINED IGC_DIR) # Only copy igc libs if available
            message(STATUS "L0::Igc Dir: ${IGC_DIR}")
            add_custom_command(
                TARGET copy_third_party_files
                PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${IGC_DIR}/lib" "${CUSTOM_COMMAND_BINARY_DIR}/"
                DEPENDS "${IGC_DIR}/lib"
            )
        endif()
    endif()

    if(TARGET ${GMM_TARGET_NAME})
        message(STATUS "L0::Gmm Target: ${GMM_TARGET_NAME}")
        add_dependencies(copy_third_party_files ${GMM_TARGET_NAME})
        if(UNIX)
            add_custom_command(
                TARGET copy_third_party_files
                PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_SONAME_FILE:${GMM_TARGET_NAME}>" "${CUSTOM_COMMAND_BINARY_DIR}/"
            )
        endif()
        add_custom_command(
            TARGET copy_third_party_files
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${GMM_TARGET_NAME}>" "${CUSTOM_COMMAND_BINARY_DIR}/"
        )
    else()
        if(DEFINED GMM_DIR) # Only copy gmm libs if available
            message(STATUS "L0::Gmm Dir: ${GMM_DIR}")
            add_custom_command(
                TARGET copy_third_party_files
                PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${GMM_DIR}/lib" "${CUSTOM_COMMAND_BINARY_DIR}/"
                DEPENDS "${GMM_DIR}/lib"
            )
        endif()
    endif()

    # Get build type
    string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)

    if("${BUILD_TYPE}" STREQUAL "debug")
        add_definitions(-DZE_DEBUG)
    endif()

    include_directories(${COMPUTE_RUNTIME_DIR}/third_party/opencl_headers)
    include_directories(${LevelZero_INCLUDE_DIRS})
    include_directories(${NEO_SOURCE_DIR}/level_zero/api/experimental${BRANCH_DIR_SUFFIX})
    include_directories(${NEO_SOURCE_DIR}/shared/source/compiler_interface/compiler_options${BRANCH_DIR_SUFFIX})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/hw_helpers${BRANCH_DIR_SUFFIX})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdlist_extended${BRANCH_DIR_SUFFIX})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdqueue_extended${BRANCH_DIR_SUFFIX})
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
    include_directories(ddi${BRANCH_DIR_SUFFIX})
    include_directories(tools/source)
    include_directories(experimental${BRANCH_DIR_SUFFIX}/source)

    # Create our shared library/DLL
    add_library(${TARGET_NAME_L0}
      SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${NEO_CORE_DIRECTORY}/dll/options_dll.cpp
        ${NEO_CORE_DIRECTORY}/gmm_helper/resource_info.cpp
        ${NEO_CORE_DIRECTORY}/utilities/cpuintrinsics.cpp
        ${NEO_CORE_DIRECTORY}/utilities/debug_settings_reader_creator.cpp
        ${NEO_CORE_DIRECTORY}/gmm_helper/page_table_mngr.cpp
        ${NEO_CORE_DIRECTORY}/gmm_helper/resource_info.cpp
        ${NEO_CORE_DIRECTORY}/helpers/abort.cpp
        ${NEO_CORE_DIRECTORY}/helpers/debug_helpers.cpp
    )

    target_sources(${TARGET_NAME_L0}
        PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/source/aub/aub_stream_interface.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/create_command_stream.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/create_deferred_deleter.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/create_tbx_sockets.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/get_devices.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/source_level_debugger.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/helpers/built_ins_helper.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/instrumentation/instrumentation.cpp
    )

    if(WIN32)
        target_sources(${TARGET_NAME_L0}
            PRIVATE
                ${NEO_CORE_DIRECTORY}/gmm_helper/windows/gmm_memory_base.cpp
                ${NEO_CORE_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
                ${NEO_CORE_DIRECTORY}/os_interface/windows/sys_calls.cpp
                ${NEO_CORE_DIRECTORY}/os_interface/windows/os_interface.cpp
                ${NEO_CORE_DIRECTORY}/os_interface/windows/wddm/wddm_calls.cpp
                ${NEO_CORE_DIRECTORY}/os_interface/windows/wddm/wddm_create.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/windows/environment_variables.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/windows/options_windows.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/windows/os_interface.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/windows/create_wddm_memory_manager.cpp
        )
         target_link_libraries(${TARGET_NAME_L0}
             dxgi
             ws2_32
         )
    else()
        target_sources(${TARGET_NAME_L0}
            PRIVATE
                ${NEO_CORE_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/allocator_helper.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/create_drm_memory_manager.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/devices${BRANCH_DIR_SUFFIX}/devices.inl
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/devices/devices_base.inl
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/options_linux.cpp
                ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/os_interface.cpp
        )
    endif()

if(DEFINED AUB_STREAM_DIR)
    target_sources(${TARGET_NAME_L0}
        PRIVATE
            $<TARGET_OBJECTS:aub_stream_all_hw>
    )
endif()
    target_sources(${TARGET_NAME_L0}
        PRIVATE
            $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
    )

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/source/inc)

target_compile_definitions(${TARGET_NAME_L0}
    PUBLIC
        ZE_MAKEDLL
)

get_property(COMPUTE_RUNTIME_DEFINITIONS
    TARGET ${NEO_RELEASE_LIB_NAME}
    PROPERTY COMPILE_DEFINITIONS
)

target_compile_definitions(${TARGET_NAME_L0}
    PRIVATE
        ${COMPUTE_RUNTIME_DEFINITIONS}
)

append_sources_from_properties(L0_SRCS_DLL NEO_CORE_SRCS_LINK)
target_sources(${TARGET_NAME_L0} PRIVATE ${L0_SRCS_DLL})

if(UNIX)
    target_sources(${TARGET_NAME_L0}
        PRIVATE
            ${NEO_CORE_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/drm_neo_create.cpp
    )
    set(OS_SPECIFIC_LIBS dl pthread rt)
    target_include_directories(${TARGET_NAME_L0} PUBLIC
        ${L0_ROOT_DIR}/core/source/os_interface/linux
        ${I915_INCLUDES_DIR}
    )
    target_include_directories(${TARGET_NAME_L0} PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/devices${BRANCH_DIR_SUFFIX}
    )
else()
    set(OS_SPECIFIC_LIBS "")
    target_include_directories(${TARGET_NAME_L0} PUBLIC
        ${L0_ROOT_DIR}/core/source/os_interface/windows
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(L0_BITNESS_SUFIX 32)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(L0_BITNESS_SUFIX 64)
    endif()

    set_target_properties(${TARGET_NAME_L0} PROPERTIES
        DEBUG_OUTPUT_NAME "${TARGET_NAME_L0}${L0_BITNESS_SUFIX}"
        RELEASE_OUTPUT_NAME "${TARGET_NAME_L0}${L0_BITNESS_SUFIX}"
        RELEASEINTERNAL_OUTPUT_NAME "${TARGET_NAME_L0}${L0_BITNESS_SUFIX}"
        OUTPUT_NAME "${TARGET_NAME_L0}${L0_BITNESS_SUFIX}"
    )
    add_dependencies(${TARGET_NAME_L0} ${GMM_TARGET_NAME})
    target_sources(${TARGET_NAME_L0} PRIVATE
      ${NEO_CORE_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
    )
endif()

target_link_libraries(${TARGET_NAME_L0}
    ${NEO_RELEASE_LIB_NAME}
    ${NEO_CORE_RELEASE_LIB_NAME}
    ${NEO_RELEASE_LIB_NAME}
    ${NEO_CORE_RELEASE_LIB_NAME}
    ${OS_SPECIFIC_LIBS}
)
if(UNIX)
    target_link_libraries(${TARGET_NAME_L0} ${GMM_LINK_NAME})
endif()

    add_subdirectory_unique(api)
    add_subdirectory_unique(source)
    add_subdirectory_unique(core/source)
    add_subdirectory_unique(experimental${BRANCH_DIR_SUFFIX}/source)
    add_subdirectory_unique(tools/source)

    create_source_tree(${TARGET_NAME_L0} ${L0_ROOT_DIR}/..)

if(HAVE_INSTRUMENTATION)
    target_link_libraries(${TARGET_NAME_L0}
        ${INSTRUMENTATION_LIB_NAME})
    target_include_directories(${TARGET_NAME_L0} BEFORE PRIVATE
        ${INSTRUMENTATION_INCLUDE_PATH}
  )
endif()

set_property(TARGET ${TARGET_NAME_L0} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
set_target_properties(${TARGET_NAME_L0} PROPERTIES FOLDER ${TARGET_NAME_L0})

set_target_properties(${TARGET_NAME_L0} PROPERTIES
    FOLDER ${TARGET_NAME_L0}
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

if(UNIX)
    install(TARGETS ${TARGET_NAME_L0}
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${PROJECT_NAME}
            NAMELINK_SKIP
    )
else()
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
        DESTINATION Release/lh64
        CONFIGURATIONS Release
    )
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
        DESTINATION Release-Internal/lh64
        CONFIGURATIONS ReleaseInternal
    )
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
        DESTINATION Debug/lh64
        CONFIGURATIONS Debug
    )
endif()

    add_subdirectories()

    if(UNIX)
        message(STATUS "Building LevelZero package")

        set_property(GLOBAL APPEND PROPERTY NEO_COMPONENTS_LIST ${PROJECT_NAME})

        set(L0_PACKAGE_VERSION_DEB "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
        set(L0_PACKAGE_VERSION_RPM "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

        if(NEO__GMM_FOUND)
            list(APPEND _external_package_dependencies_deb "intel-gmmlib(=${NEO__GMM_VERSION})")
            list(APPEND _external_package_dependencies_rpm "intel-gmmlib = ${NEO__GMM_VERSION}")
        else()
            list(APPEND _external_package_dependencies_deb "intel-gmmlib")
            list(APPEND _external_package_dependencies_rpm "intel-gmmlib")
        endif()

        if(NEO__IGC_FOUND)
            list(APPEND _external_package_dependencies_deb "intel-igc-opencl(>=${NEO__IGC_VERSION})")
            list(APPEND _external_package_dependencies_rpm "intel-igc-opencl >= ${NEO__IGC_VERSION}")
        else()
            list(APPEND _external_package_dependencies_deb "intel-igc-opencl")
            list(APPEND _external_package_dependencies_rpm "intel-igc-opencl")
        endif()

        find_program(GIT_EXE NAMES "git")
        if(GIT_EXE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
            if(MSVC)
                execute_process(
                    COMMAND CMD /c git rev-list v${L0_PACKAGE_VERSION_DEB}..HEAD --count
                    OUTPUT_VARIABLE VERSION_SUFFIX
                )
            else()
                execute_process(
                    COMMAND git rev-list v${L0_PACKAGE_VERSION_DEB}..HEAD --count
                    OUTPUT_VARIABLE VERSION_SUFFIX
                )
            endif()
        endif()
        string(STRIP "${VERSION_SUFFIX}" VERSION_SUFFIX)

        string(REPLACE ";" ", " CPACK_DEBIAN_INTEL-LEVEL-ZERO-GPU_PACKAGE_DEPENDS "${_external_package_dependencies_deb}")
        string(REPLACE ";" ", " CPACK_RPM_INTEL-LEVEL-ZERO-GPU_PACKAGE_REQUIRES "${_external_package_dependencies_rpm}")
        set(CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_DEPENDS ${CPACK_DEBIAN_INTEL-LEVEL-ZERO-GPU_PACKAGE_DEPENDS} PARENT_SCOPE)
        set(CPACK_RPM_LEVEL-ZERO-GPU_PACKAGE_REQUIRES ${CPACK_RPM_INTEL-LEVEL-ZERO-GPU_PACKAGE_REQUIRES} PARENT_SCOPE)

        set(CPACK_RPM_LEVEL-ZERO-GPU_FILE_NAME "intel-level-zero-gpu-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm" PARENT_SCOPE)

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/cpack.cmake)
            include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/cpack.cmake)
        endif()

    endif()
endif()
