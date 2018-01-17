# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(IGDRCL_SRCS_tests_gen9
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_gen9.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gen_cmd_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_data_tests_gen9.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_sample.cpp
  PARENT_SCOPE
)

if(NOT MSVC)
  set(IGDRCL_SRCS_tests_gen9_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_mapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/device_factory_tests.cpp
    PARENT_SCOPE
  )
endif(NOT MSVC)

if(TESTS_SKL)
  set(IGDRCL_SRCS_tests_gen9_skl
    ${CMAKE_CURRENT_SOURCE_DIR}/skl/device_tests_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/skl/test_device_caps_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/skl/test_preamble_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/skl/test_sample_skl.cpp
    PARENT_SCOPE
  )
  if(NOT MSVC)
    set(IGDRCL_SRCS_tests_gen9_skl_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/skl/linux/hw_info_config_tests.cpp
      PARENT_SCOPE
    )
  endif(NOT MSVC)
endif()

if(TESTS_GLK)
  set(IGDRCL_SRCS_tests_gen9_glk
    ${CMAKE_CURRENT_SOURCE_DIR}/glk/test_device_caps_glk.cpp
    PARENT_SCOPE
  )
  if(NOT MSVC)
    set(IGDRCL_SRCS_tests_gen9_glk_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/glk/linux/hw_info_config_tests.cpp
      PARENT_SCOPE
    )
  endif(NOT MSVC)
endif()

if(TESTS_CFL)
  set(IGDRCL_SRCS_tests_gen9_cfl
    ${CMAKE_CURRENT_SOURCE_DIR}/cfl/test_device_caps_cfl.cpp
    PARENT_SCOPE
  )
  if(NOT MSVC)
    set(IGDRCL_SRCS_tests_gen9_cfl_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/cfl/linux/hw_info_config_tests.cpp
      PARENT_SCOPE
  )
  endif(NOT MSVC)
endif()

if(TESTS_BXT)
  set(IGDRCL_SRCS_tests_gen9_bxt
    ${CMAKE_CURRENT_SOURCE_DIR}/bxt/device_tests_bxt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bxt/test_device_caps_bxt.cpp
    PARENT_SCOPE
  )
  if(NOT MSVC)
    set(IGDRCL_SRCS_tests_gen9_bxt_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/bxt/linux/hw_info_config_tests.cpp
      PARENT_SCOPE
    )
  endif(NOT MSVC)
endif()

if(TESTS_KBL)
  set(IGDRCL_SRCS_tests_gen9_kbl
    ${CMAKE_CURRENT_SOURCE_DIR}/kbl/test_device_caps_kbl.cpp
    PARENT_SCOPE
  )
  
  if(NOT MSVC)
    set(IGDRCL_SRCS_tests_gen9_kbl_linux
      ${CMAKE_CURRENT_SOURCE_DIR}/kbl/linux/hw_info_config_tests.cpp
      PARENT_SCOPE
    )
  endif(NOT MSVC)
endif()

if(MSVC)
  set(IGDRCL_SRCS_tests_gen9_windows
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_mapper_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/os_interface_tests.cpp"
    PARENT_SCOPE
  )
endif(MSVC)
