#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DEFINED NEO_CORE_RELEASE_LIB_NAME)
    MESSAGE(FATAL_ERROR "NEO Core library name undefined!")
endif()

include(enable_gens.cmake)

function(generate_core_lib LIB_NAME MOCKABLE)
    add_library(${LIB_NAME} STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/enable_gens.cmake
        ${CORE_SOURCES}
    )
    if(${MOCKABLE})
        target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
    else()
        target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
    endif()

    list(APPEND LIB_FLAGS_DEFINITIONS ${SUPPORTED_GEN_FLAGS_DEFINITONS})

    target_compile_definitions(${LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})

    target_include_directories(${LIB_NAME} PUBLIC
        ${ENGINE_NODE_DIR}
        ${NEO__GMM_INCLUDE_DIR}
        ${CIF_BASE_DIR}
        ${IGC_OCL_ADAPTOR_DIR}
        ${NEO__IGC_INCLUDE_DIR}
        ${KHRONOS_HEADERS_DIR}
    )

    if(WIN32)
      target_include_directories(${LIB_NAME} PUBLIC
        ${WDK_INCLUDE_PATHS}
        ${NEO_SOURCE_DIR}/core/os_interface/windows
      )
    else()
      target_include_directories(${LIB_NAME} PUBLIC
        ${I915_INCLUDES_DIR}
        ${NEO_SOURCE_DIR}/core/os_interface/linux
      )
    endif()

    create_project_source_tree(${LIB_NAME})
endfunction()

add_subdirectories()

set(CORE_SOURCES ${CORE_SRCS_GENX_ALL_BASE})

append_sources_from_properties(CORE_SOURCES
    NEO_CORE_COMMAND_CONTAINER
    NEO_CORE_DEBUG_SETTINGS
    NEO_CORE_DEBUGGER
    NEO_CORE_EXECUTION_ENVIRONMENT
    NEO_CORE_HELPERS
    NEO_CORE_INDIRECT_HEAP
    NEO_CORE_SKU_INFO_BASE
    NEO_CORE_UTILITIES
    NEO_UNIFIED_MEMORY
)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -fPIC")
endif()

if(DONT_CARE_OF_VIRTUALS)
    generate_core_lib(${NEO_CORE_RELEASE_LIB_NAME} TRUE)
else()
    generate_core_lib(${NEO_CORE_RELEASE_LIB_NAME} FALSE)
    if(NOT SKIP_UNIT_TESTS)
        generate_core_lib(${NEO_CORE_MOCKABLE_LIB_NAME} TRUE)

    endif()
endif()

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_LINK ${CORE_SRCS_LINK})
