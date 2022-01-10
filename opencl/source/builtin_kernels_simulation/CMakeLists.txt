#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(BUILTIN_KERNELS_SIMULATION_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/opencl_c.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/opencl_c.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

string(REPLACE "/WX" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REGEX REPLACE "-Werror[^ \t\n]*" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wsometimes-uninitialized" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wsign-compare" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wunused-variable" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything")
endif()

if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -fPIC")
endif()

macro(macro_for_each_core_type)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    list(APPEND DEFAULT_CORE_PLATFORMS_DEFITIONS DEFAULT_${CORE_TYPE}_PLATFORM=${DEFAULT_SUPPORTED_${CORE_TYPE}_PLATFORM})
  endforeach()
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

add_library(${BIKSIM_LIB_NAME} OBJECT EXCLUDE_FROM_ALL ${BUILTIN_KERNELS_SIMULATION_SRCS})
target_include_directories(${BIKSIM_LIB_NAME} BEFORE PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
)
set_target_properties(${BIKSIM_LIB_NAME} PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_BUILTINS_PROJECTS_FOLDER}")
target_compile_definitions(${BIKSIM_LIB_NAME} PUBLIC ${SUPPORTED_CORE_FLAGS_DEFINITONS} ${DEFAULT_CORE_PLATFORMS_DEFITIONS} MOCKABLE_VIRTUAL=)
