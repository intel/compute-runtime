#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WDDM_LINUX)
  include_directories(${NEO__GMM_INCLUDE_DIR} ${WDK_INCLUDE_PATHS})
  add_definitions(-DLHDM=1)
  add_definitions(-DWDDM_LINUX=1)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  add_library(wsl_compute_helper SHARED
              wsl_compute_helper.cpp
              wsl_compute_helper.h
              wsl_compute_helper_types_demarshall.cpp
              wsl_compute_helper_types_demarshall.h
              wsl_compute_helper_types_marshall.cpp
              wsl_compute_helper_types_marshall.h
              wsl_compute_helper_types_tokens.h
              wsl_compute_helper_types_tokens_structs.h
              gmm_resource_info_struct.h
              gmm_resource_info_accessor.h
  )
endif()
