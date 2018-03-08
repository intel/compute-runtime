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

foreach(GEN_NUM RANGE ${MAX_GEN})
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
      ${IGDRCL_SOURCE_DIR}/unit_tests/libult/gen${GEN_NUM}.cpp
      ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/enable_family_full.cpp
    )
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      list (APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
        ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/enable_${PLATFORM_IT_LOWER}.cpp
      )
      list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX
        ${IGDRCL_SOURCE_DIR}/runtime/gen${GEN_NUM}/linux/enable_${PLATFORM_IT_LOWER}.cpp
      )
    endforeach()
  endif()
endforeach()

set(IGDRCL_SRCS_LIB_ULT
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.cpp
)

add_library (igdrcl_libult OBJECT
  ${IGDRCL_SRCS_LIB_ULT}
  ${IGDRCL_SRCS_ENABLE_TESTED_HW}
)
if(UNIX)
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

set(IGDRCL_SRCS_LIB_ULT_CS
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.h
)
add_library (igdrcl_libult_cs OBJECT
  ${IGDRCL_SRCS_LIB_ULT_CS}
)

foreach(target_name igdrcl_libult igdrcl_libult_cs)
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  create_project_source_tree(${target_name} ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)
endforeach()
