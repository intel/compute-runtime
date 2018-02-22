# Copyright (c) 2018, Intel Corporation
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

set(OPTIONAL_RUNTIME_GENX_FILES
  hw_info.cpp
)

foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    foreach(SRC_IT ${OPTIONAL_RUNTIME_GENX_FILES})
      set(SRC_FILE ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/${SRC_IT})
      if(EXISTS ${SRC_FILE})
        list(APPEND CLOC_SRCS_LIB ${SRC_FILE})
      endif()
    endforeach()
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    list (APPEND IGDRCL_SRCS_LIB_ULT
      ${IGDRCL_SOURCE_DIR}/unit_tests/libult/gen${GEN_NUM}.cpp
      ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/enable_family_full.cpp
    )
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      list (APPEND IGDRCL_SRCS_LIB_ULT
        ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/enable_${PLATFORM_IT_LOWER}.cpp
      )
      if (NOT WIN32)
        list (APPEND IGDRCL_SRCS_LIB_ULT
          ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/linux/enable_${PLATFORM_IT_LOWER}.cpp
        )
      endif (NOT WIN32)
    endforeach(PLATFORM_IT)
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)

list (APPEND IGDRCL_SRCS_LIB_ULT
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.cpp
)

add_library (igdrcl_libult OBJECT
  ${IGDRCL_SRCS_LIB_ULT}
)
add_library (igdrcl_libult_cs OBJECT
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.h
)

if(MSVC)
  target_include_directories(igdrcl_libult PRIVATE ${WDK_INCLUDE_PATHS})
endif(MSVC)

if (WIN32)
  target_compile_definitions(igdrcl_libult PUBLIC OGL=1)
  target_compile_definitions(igdrcl_libult PUBLIC INSTR_WIN_UMD=1)
else (WIN32)
  target_compile_definitions(igdrcl_libult PUBLIC OGL_GEM=1)
endif (WIN32)

target_compile_definitions(igdrcl_libult PUBLIC DEFAULT_PLATFORM=${DEFAULT_PLATFORM})
target_include_directories(igdrcl_libult PRIVATE
  ${GMM_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${INSTRUMENTATION_INCLUDE_PATH}
  ${GTEST_INCLUDE_DIR}
  ${KHRONOS_HEADERS_DIR}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
  ${GMOCK_INCLUDE_DIR}
)
target_include_directories(igdrcl_libult_cs PRIVATE
  ${GMM_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${KHRONOS_HEADERS_DIR}
)
target_include_directories(igdrcl_libult BEFORE PRIVATE ${HW_SRC_INCLUDE_PATH})
target_include_directories(igdrcl_libult_cs BEFORE PRIVATE ${HW_SRC_INCLUDE_PATH})

# if(HAVE_INSTRUMENTATION)
# target_include_directories(igdrcl_libult PRIVATE $<TARGET_PROPERTY:instrumentation_umd,INTERFACE_INCLUDE_DIRECTORIES>)
# endif()

set_target_properties (igdrcl_libult PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties (igdrcl_libult_cs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(igdrcl_libult PUBLIC ${IGDRCL_LIB_FLAGS_DEFINITIONS} ${TESTED_GEN_FLAGS_DEFINITONS})
target_compile_definitions(igdrcl_libult_cs PUBLIC ${IGDRCL_LIB_FLAGS_DEFINITIONS} ${TESTED_GEN_FLAGS_DEFINITONS})

set_property(TARGET igdrcl_libult APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(igdrcl_libult PROPERTIES FOLDER "opencl runtime")
set_target_properties(igdrcl_libult_cs PROPERTIES FOLDER "opencl runtime")

target_compile_definitions(igdrcl_libult PUBLIC MOCKABLE_VIRTUAL=virtual)
target_compile_definitions(igdrcl_libult_cs PUBLIC MOCKABLE_VIRTUAL=virtual)
