#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(NEO_CORE_TESTS_PVC
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/product_configs_pvc.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_PVC ${NEO_CORE_TESTS_PVC})
  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_PVC})

endif()
