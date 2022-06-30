#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(gen_kernel.cmake)

add_custom_target(prepare_test_kernels_for_l0)
set_target_properties(prepare_test_kernels_for_l0 PROPERTIES FOLDER ${TARGET_NAME_L0})

set(TEST_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modules/test_kernel.cl
)

set(TEST_KERNEL_BINDLESS_internal_options
    "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode"
)

set(TEST_KERNEL_BINDLESS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modules/stateful_copy_buffer.cl
)

set(l0_test_kernels_outputs)
macro(macro_for_each_core_type)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    foreach(REVISION_ID ${${PLATFORM_TYPE}_${CORE_TYPE}_REVISIONS})
      if(${CORE_TYPE}_HAS_${PLATFORM_TYPE})
        get_family_name_with_type(${CORE_TYPE} ${PLATFORM_TYPE})
        string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
        set(PLATFORM_LOWER ${DEFAULT_SUPPORTED_${CORE_TYPE}_${PLATFORM_TYPE}_PLATFORM})
        string(TOLOWER ${CORE_TYPE} CORE_TYPE_LOWER)
        level_zero_generate_kernels(l0_test_kernel_outputs ${PLATFORM_LOWER} ${family_name_with_type} ${REVISION_ID} "-g" ${TEST_MODULES})

        #skip Gen8 bindless kernel generation
        if(NOT ("${CORE_TYPE_LOWER}" STREQUAL "gen8"))
          level_zero_generate_kernels_with_internal_options(l0_bindless_test_kernel_outputs ${PLATFORM_LOWER} ${family_name_with_type} "bindless" ${REVISION_ID} "-g" ${TEST_KERNEL_BINDLESS_internal_options} ${TEST_KERNEL_BINDLESS})
        endif()
      endif()
    endforeach()
  endforeach()
endmacro()
apply_macro_for_each_core_type("TESTED")
add_custom_target(l0_common_test_kernels DEPENDS ${l0_test_kernel_outputs} ${l0_bindless_test_kernel_outputs} copy_compiler_files)
set_target_properties(l0_common_test_kernels PROPERTIES FOLDER ${TARGET_NAME_L0})
add_dependencies(prepare_test_kernels_for_l0 l0_common_test_kernels)

macro(macro_for_each_core_type)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    set(ENABLE_L0_MOCKS_CPP ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests${BRANCH_DIR}${CORE_TYPE_LOWER}/enable_l0_mocks_${CORE_TYPE_LOWER}.cpp)
    if(EXISTS ${ENABLE_L0_MOCKS_CPP})
      list(APPEND LIBULT_L0_SOURCES ${ENABLE_L0_MOCKS_CPP})
    endif()
  endforeach()
endmacro()

apply_macro_for_each_core_type("TESTED")

add_library(l0_libult OBJECT EXCLUDE_FROM_ALL
            ${LIBULT_L0_SOURCES}
)

set_target_properties(l0_libult PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(l0_libult PROPERTIES FOLDER ${TARGET_NAME_L0})
set_property(TARGET l0_libult APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(l0_libult PRIVATE $<TARGET_PROPERTY:${L0_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(l0_libult PRIVATE $<TARGET_PROPERTY:${L0_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(l0_libult)
