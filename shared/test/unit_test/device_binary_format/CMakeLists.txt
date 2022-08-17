#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_decoder_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ar/ar_encoder_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ocl_elf_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_patchtokens_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_zebin_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_formats_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_decoder_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_encoder_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_decoder_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_dumper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/patchtokens_validator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/yaml/yaml_parser_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zebin_debug_binary_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zebin_decoder_tests.cpp
)