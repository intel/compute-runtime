#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_DEVICE_BINARY_FORMAT_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_validator_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_DEVICE_BINARY_FORMAT_TESTS ${NEO_DEVICE_BINARY_FORMAT_TESTS})
