#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}additional_extension_function_address.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}extension_injector.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle.h
               ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/driver.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/driver_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/extension_function_address.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/extension_function_address.h
               ${CMAKE_CURRENT_SOURCE_DIR}/extension_injector.h
               ${CMAKE_CURRENT_SOURCE_DIR}/host_pointer_manager.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/host_pointer_manager.h
)
