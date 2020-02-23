#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_DEVICE_BINARY_FORMAT
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_decoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_encoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_encoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ocl_elf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_patchtokens.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_formats.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_formats.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_validator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_validator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf.h
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_decoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_decoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_encoder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_encoder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/elf/ocl_elf.h
)

set_property(GLOBAL PROPERTY NEO_DEVICE_BINARY_FORMAT ${NEO_DEVICE_BINARY_FORMAT})
