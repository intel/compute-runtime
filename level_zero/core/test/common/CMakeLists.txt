#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(gen_kernel.cmake)

add_custom_target(prepare_l0_test_kernels)
set_target_properties(prepare_l0_test_kernels PROPERTIES FOLDER ${TARGET_NAME_L0})

set(TEST_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modules/test_kernel.cl
)

set(l0_test_kernels_outputs)
macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      string(TOLOWER ${PLATFORM_TYPE} PLATFORM_TYPE_LOWER)
      set(PLATFORM_LOWER ${DEFAULT_SUPPORTED_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})
      level_zero_gen_kernels(l0_test_kernel_outputs ${PLATFORM_LOWER} ${family_name_with_type} "-g" ${TEST_MODULES})
    endif()
  endforeach()
endmacro()
apply_macro_for_each_gen("TESTED")
add_custom_target(l0_common_test_kernels DEPENDS ${l0_test_kernel_outputs} copy_compiler_files)
set_target_properties(l0_common_test_kernels PROPERTIES FOLDER ${TARGET_NAME_L0})
add_dependencies(prepare_l0_test_kernels l0_common_test_kernels)

