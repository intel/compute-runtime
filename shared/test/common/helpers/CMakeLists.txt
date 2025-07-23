#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_libult_common PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cmd_buffer_validator.h
               ${CMAKE_CURRENT_SOURCE_DIR}/batch_buffer_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_reader_test_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/gtest_helpers.h
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_args_test_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/raii_gfx_core_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/raii_product_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/relaxed_ordering_commands_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/static_size3.h
               ${CMAKE_CURRENT_SOURCE_DIR}/ult_limits.h
)

add_subdirectories()
