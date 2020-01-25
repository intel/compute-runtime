#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_DEVICE_BINARY_FORMAT
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_validator.inl
)

set_property(GLOBAL PROPERTY NEO_DEVICE_BINARY_FORMAT ${NEO_DEVICE_BINARY_FORMAT})
