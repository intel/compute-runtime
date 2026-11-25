#
# Copyright (C) 2021-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# disable optimizations for ults
if(NOT ULTS_ENABLE_OPTIMIZATIONS)
  if(MSVC)
    string(REPLACE "/O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
    string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASEINTERNAL ${CMAKE_CXX_FLAGS_RELEASEINTERNAL})
    string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASEINTERNAL ${CMAKE_C_FLAGS_RELEASEINTERNAL})
  else()
    set(flags_variables CMAKE_CXX_FLAGS)
    foreach(config ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
      string(TOUPPER ${config} config_upper)
      list(APPEND flags_variables CMAKE_CXX_FLAGS_${config_upper})
    endforeach()
    foreach(flags_variable ${flags_variables})
      string(REGEX REPLACE "-O[1-3]" "" ${flags_variable} "${${flags_variable}}")
      #disable fortify source as this requires optimization to be on
      string(REPLACE "-Wp,-D_FORTIFY_SOURCE=2" "" ${flags_variable} "${${flags_variable}}")
      string(REPLACE "-D_FORTIFY_SOURCE=2" "" ${flags_variable} "${${flags_variable}}")
    endforeach()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O0")
    set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} -O0")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O0")
    set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASEINTERNAL} -O0")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16 AND NOT NEO_DISABLE_LTO)
      # -O0 turns off -ftoplevel-reorder, and LTO in gcc older than 16 then puts most
      # of the code into a single partition, compiled by one process at link time.
      # The flag lets LTO split the code into partitions compiled in parallel.
      # gcc 16 partitions such code correctly.
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftoplevel-reorder")
    endif()
  endif()
endif()
