#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_linux_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_tests_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main_linux.cpp
  ${NEO_SOURCE_DIR}/unit_tests/aub_stream_mocks/aub_stream_interface_mock.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/os_interface.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/allocator_helper.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/create_drm_memory_manager.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
)

set(IGDRCL_SRCS_linux_dll_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_tests_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main_linux_dll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.h
  ${NEO_SOURCE_DIR}/runtime/dll/linux/allocator_helper.cpp
  ${NEO_SOURCE_DIR}/runtime/dll/linux/drm_neo_create.cpp
  ${NEO_SOURCE_DIR}/runtime/dll/linux/options_linux.cpp
  ${NEO_SOURCE_DIR}/runtime/dll/linux/os_interface.cpp
  ${NEO_SOURCE_DIR}/unit_tests/aub_stream_mocks/aub_stream_interface_mock.cpp
  ${NEO_SOURCE_DIR}/unit_tests/linux${BRANCH_DIR_SUFFIX}/drm_other_requests.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/create_drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
)

if(NEO__LIBVA_FOUND)
  list(APPEND IGDRCL_SRCS_linux_dll_tests ${CMAKE_CURRENT_SOURCE_DIR}/va_tests.cpp)
endif(NEO__LIBVA_FOUND)

macro(macro_for_each_platform)
  list(APPEND IGDRCL_SRCS_linux_dll_tests ${NEO_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/hw_info_${PLATFORM_IT_LOWER}.inl)
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
    $<TARGET_OBJECTS:mock_gmm>
  )
  target_link_libraries(igdrcl_${target_name} ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks ${IGDRCL_EXTRA_LIBS} gmock-gtest)

  set_property(TARGET igdrcl_${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  target_include_directories(igdrcl_${target_name} PRIVATE
    ${NEO_SOURCE_DIR}/core/unit_tests/test_macros${BRANCH_DIR_SUFFIX}
    ${NEO_SOURCE_DIR}/runtime/dll/linux/devices${BRANCH_DIR_SUFFIX}
    ${NEO_SOURCE_DIR}/unit_tests/gen_common${BRANCH_DIR_SUFFIX}
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_dependencies(unit_tests igdrcl_${target_name})
endforeach()
add_subdirectories()
