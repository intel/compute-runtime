# Copyright (c) 2017 - 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(BUILTIN_KERNELS_SIMULATION_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/opencl_c.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opencl_c.h
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_simulation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_simulation.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_simulation.h
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

ENABLE_WUD()

list (APPEND HEADER_INCLUDES ${IGDRCL_SOURCE_DIR}/runtime ${UMKM_SHAREDDATA_INCLUDE_PATHS})

foreach(GEN_NUM RANGE ${MAX_GEN})
  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    list(APPEND DEFAULT_GEN_PLATFORMS_DEFITIONS DEFAULT_GEN${GEN_NUM}_PLATFORM=${DEFAULT_SUPPORTED_GEN${GEN_NUM}_PLATFORM})
    list (APPEND BUILTIN_KERNELS_SIMULATION_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/gen${GEN_NUM}/scheduler_simulation.cpp)
  endif()
endforeach()

add_library(${BIKSIM_LIB_NAME} OBJECT ${BUILTIN_KERNELS_SIMULATION_SRCS})
target_include_directories(${BIKSIM_LIB_NAME} BEFORE PRIVATE ${HEADER_INCLUDES})
target_include_directories(${BIKSIM_LIB_NAME} PRIVATE
  ${KHRONOS_HEADERS_DIR}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)

set_vs_props(${BIKSIM_LIB_NAME})
set_target_properties(${BIKSIM_LIB_NAME} PROPERTIES FOLDER "built_ins")
target_compile_definitions(${BIKSIM_LIB_NAME} PUBLIC ${SUPPORTED_GEN_FLAGS_DEFINITONS} ${DEFAULT_GEN_PLATFORMS_DEFITIONS})
