#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/image.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_format_desc_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_format_desc_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_formats.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/image_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_imp.h
)
