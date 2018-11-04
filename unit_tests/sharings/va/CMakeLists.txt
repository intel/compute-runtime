#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(IGDRCL__LIBVA_FOUND)
  set(IGDRCL_SRCS_tests_sharings_va
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_from_va_media_surface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_enqueue_acquire_va_media_surfaces_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_enqueue_release_va_media_surfaces_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_extension_function_address_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context_va_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_va_image_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_va_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/va_base_object_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_enable_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/va_sharing_tests.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sharings_va})
endif(IGDRCL__LIBVA_FOUND)
