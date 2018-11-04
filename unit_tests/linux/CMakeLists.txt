#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_linux_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_tests_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main_linux.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/allocator_helper.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/options.cpp
)

set(IGDRCL_SRCS_linux_dll_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_tests_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main_linux_dll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.h
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/debug_settings_manager.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/allocator_helper.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/drm_neo_create.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/options.cpp
)

if(IGDRCL__LIBVA_FOUND)
  list(APPEND IGDRCL_SRCS_linux_dll_tests ${CMAKE_CURRENT_SOURCE_DIR}/va_tests.cpp)
endif(IGDRCL__LIBVA_FOUND)

macro(macro_for_each_platform)
  list(APPEND IGDRCL_SRCS_linux_dll_tests ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/hw_info_${PLATFORM_IT_LOWER}.inl)
endmacro()
macro(macro_for_each_gen)
  apply_macro_for_each_platform()
endmacro()
apply_macro_for_each_gen("TESTED")

foreach(target_name linux_tests linux_dll_tests)
  add_executable(igdrcl_${target_name}
    ${IGDRCL_SRCS_${target_name}}
    $<TARGET_OBJECTS:igdrcl_libult>
    $<TARGET_OBJECTS:igdrcl_libult_cs>
  )
  target_link_libraries(igdrcl_${target_name} ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks ${IGDRCL_EXTRA_LIBS} gmock-gtest)

  set_property(TARGET igdrcl_${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  target_include_directories(igdrcl_${target_name} PRIVATE
    ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common${BRANCH_DIR_SUFFIX}
    ${IGDRCL_SOURCE_DIR}/runtime/dll/linux/devices${BRANCH_DIR_SUFFIX}
  )
  add_dependencies(unit_tests igdrcl_${target_name})
endforeach()
