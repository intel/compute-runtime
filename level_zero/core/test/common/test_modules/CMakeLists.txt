#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(gen_kernel.cmake)

add_custom_target(prepare_test_kernels_for_l0)
set_target_properties(prepare_test_kernels_for_l0 PROPERTIES FOLDER ${TARGET_NAME_L0})

set(TEST_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_kernel.cl
)

set(TEST_KERNEL_BINDLESS_internal_options
    "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode"
)

set(TEST_KERNEL_BINDLESS
    ${CMAKE_CURRENT_SOURCE_DIR}/stateful_copy_buffer.cl
)

macro(macro_for_each_platform)
  foreach(REVISION_CONFIG ${${PLATFORM_IT}_${CORE_TYPE}_REVISIONS})
    parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
    level_zero_generate_kernels(l0_test_kernel_outputs ${PLATFORM_IT_LOWER} ${DEVICE_ID} ${REVISION_ID} "-g" ${TEST_MODULES})

    #skip Gen8 bindless kernel generation
    if(NOT ("${CORE_TYPE_LOWER}" STREQUAL "gen8"))
      level_zero_generate_kernels_with_internal_options(l0_bindless_test_kernel_outputs ${PLATFORM_IT_LOWER} "bindless" ${DEVICE_ID} ${REVISION_ID} "-g" ${TEST_KERNEL_BINDLESS_internal_options} ${TEST_KERNEL_BINDLESS})
    endif()
  endforeach()
endmacro()

macro(macro_for_each_core_type)
  apply_macro_for_each_platform("TESTED")
endmacro()

set(PREVIOUS_KERNELS)
apply_macro_for_each_core_type("TESTED")
add_custom_target(l0_common_test_kernels DEPENDS ${l0_test_kernel_outputs} ${l0_bindless_test_kernel_outputs} copy_compiler_files)
set_target_properties(l0_common_test_kernels PROPERTIES FOLDER ${TARGET_NAME_L0})
add_dependencies(prepare_test_kernels_for_l0 l0_common_test_kernels)

add_subdirectories()
