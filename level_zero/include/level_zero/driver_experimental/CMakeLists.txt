#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_API_DRIVER_EXPERIMENTAL_INCLUDE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_bindless_image_exp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_cmdlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zex_module.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${L0_API_DRIVER_EXPERIMENTAL_INCLUDE_FILES}
)

if(UNIX)
  install(FILES
          ${L0_API_DRIVER_EXPERIMENTAL_INCLUDE_FILES}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/level_zero/driver_experimental
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )
endif()

add_subdirectories()