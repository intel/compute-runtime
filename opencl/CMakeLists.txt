#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(generate_runtime_lib LIB_NAME MOCKABLE GENERATE_EXEC)
  set(NEO_STATIC_LIB_NAME ${LIB_NAME})
  set(SHARINGS_ENABLE_LIB_NAME "${LIB_NAME}_sharings_enable")
  set(GENERATE_EXECUTABLE ${GENERATE_EXEC})

  if(${MOCKABLE})
    set(NEO_SHARED_LIB ${NEO_SHARED_MOCKABLE_LIB_NAME})
  else()
    set(NEO_SHARED_LIB ${NEO_SHARED_RELEASE_LIB_NAME})
  endif()
  if(NOT BUILD_WITHOUT_RUNTIME)
    add_subdirectory(source "${NEO_BUILD_DIR}/${LIB_NAME}")
  else()
    add_subdirectory(source "${NEO_BUILD_DIR}/${LIB_NAME}" EXCLUDE_FROM_ALL)
  endif()
  target_compile_definitions(${BUILTINS_SOURCES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${BUILTINS_VME_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)

  if(${MOCKABLE})
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
    target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
  else()
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
    target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  endif()
endmacro()

set(NEO_DYNAMIC_LIB_NAME "igdrcl_dll") # single NEO dll
set(NEO_DLL_NAME_BASE "igdrcl")

if(DONT_CARE_OF_VIRTUALS)
  message(STATUS "All targets will use virtuals")

  set(NEO_RELEASE_LIB_NAME "igdrcl_lib")
  set(NEO_MOCKABLE_LIB_NAME ${NEO_RELEASE_LIB_NAME})
  generate_runtime_lib(${NEO_RELEASE_LIB_NAME} TRUE TRUE)
else()
  set(NEO_RELEASE_LIB_NAME "igdrcl_lib_release") # Used by dll/so
  generate_runtime_lib(${NEO_RELEASE_LIB_NAME} FALSE TRUE)
  if(NOT NEO_SKIP_OCL_UNIT_TESTS)
    set(NEO_MOCKABLE_LIB_NAME "igdrcl_lib_mockable") # Used by ULTS
    generate_runtime_lib(${NEO_MOCKABLE_LIB_NAME} TRUE FALSE)
  endif()
endif()

add_subdirectory(test)
