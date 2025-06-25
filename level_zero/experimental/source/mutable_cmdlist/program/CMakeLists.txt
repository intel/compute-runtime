#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mcl_decoder.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mcl_decoder.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mcl_encoder.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mcl_encoder.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mcl_program.h
)
