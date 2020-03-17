#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

##
## L0 tests settings
##


if(NOT SKIP_L0_UNIT_TESTS)
# These need to be added to a project to enable platform support in ULTs
if(TESTS_GEN8)
    set(COMPUTE_RUNTIME_ULT_GEN8
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen8.cpp
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen8/cmd_parse_gen8.cpp
    )
endif()

if(TESTS_GEN9)
    set(COMPUTE_RUNTIME_ULT_GEN9
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen9.cpp
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen9/cmd_parse_gen9.cpp
    )
endif()

if(TESTS_GEN11)
    set(COMPUTE_RUNTIME_ULT_GEN11
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen11.cpp
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen11/cmd_parse_gen11.cpp
    )
endif()

if(TESTS_GEN12LP)
    set(COMPUTE_RUNTIME_ULT_GEN12LP
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen12lp.cpp
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen12lp/cmd_parse_gen12lp.cpp
    )
    include_directories(${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen12lp/cmd_parse${BRANCH_DIR_SUFFIX}/)
endif()

## ULT related settings

#Extract compute runtime COMPILE_DEFINITIONS
if(NOT SKIP_L0_UNIT_TESTS)
    get_property(COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS
    TARGET ${NEO_MOCKABLE_LIB_NAME}
    PROPERTY COMPILE_DEFINITIONS
)
endif()

#Append additional definitions
set(COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS
    ${COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS}
    CL_TARGET_OPENCL_VERSION=220
    CL_USE_DEPRECATED_OPENCL_1_1_APIS
    CL_USE_DEPRECATED_OPENCL_1_2_APIS
    CL_USE_DEPRECATED_OPENCL_2_0_APIS
)

if(WIN32)
    set(COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS
        ${COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS}
        WDDM_VERSION_NUMBER=23
        CONST_FROM_WDK_10_0_18328_0=
    )
endif()

#Extract compute runtime INCLUDE_DIRECTORIES
get_property(COMPUTE_RUNTIME_MOCKABLE_INCLUDES
    TARGET ${NEO_MOCKABLE_LIB_NAME}
    PROPERTY INCLUDE_DIRECTORIES
)

# Create a library that has the missing ingredients to link
add_library(compute_runtime_mockable_extra
    STATIC
    EXCLUDE_FROM_ALL
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/utilities/cpuintrinsics.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/source/aub/aub_stream_interface.cpp
        ${COMPUTE_RUNTIME_DIR}/shared/source/debug_settings/debug_settings_manager.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/abort.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/helpers/built_ins_helper.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/helpers/debug_helpers.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/helpers/test_files.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/os_interface.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/source_level_debugger.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/source_level_debugger_library.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_cif.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_csr.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_gmm_page_table_mngr.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_compilers.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks${BRANCH_SUFIX_DIR}/mock_gmm_client_context.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_gmm_resource_info.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_gmm_client_context_base.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_program.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_sip.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/utilities/debug_settings_reader_creator.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/create_tbx_sockets.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_deferred_deleter.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/ult_configuration.cpp
)

set_property(TARGET compute_runtime_mockable_extra APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
set_target_properties(compute_runtime_mockable_extra PROPERTIES FOLDER ${TARGET_NAME_L0})

# These need to be added to a project to enable platform support in ULTs

#Additional includes for ULT builds
target_include_directories(compute_runtime_mockable_extra
    PUBLIC
        ${COMPUTE_RUNTIME_MOCKABLE_INCLUDES}
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/gmm_memory
        ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)

#Additional compile definitions for ULT builds
target_compile_definitions(compute_runtime_mockable_extra
    PUBLIC
        ${COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS}
)

target_link_libraries(compute_runtime_mockable_extra
    gmock-gtest
)

if(WIN32)
    target_sources(compute_runtime_mockable_extra
        PRIVATE
            ${COMPUTE_RUNTIME_DIR}/shared/source/dll/windows/environment_variables.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_gmm_memory_base.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_wddm.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mocks/mock_compilers_windows.cpp
    )

    target_link_libraries(compute_runtime_mockable_extra
        ws2_32
    )
endif()

if(UNIX)
    target_sources(compute_runtime_mockable_extra
        PRIVATE
            ${COMPUTE_RUNTIME_DIR}/opencl/source/dll/linux/allocator_helper.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/source/tbx/tbx_sockets_imp.cpp
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/drm_mock.cpp
)
    target_link_libraries(compute_runtime_mockable_extra
        dl
    )
endif()
set_target_properties(compute_runtime_mockable_extra PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

