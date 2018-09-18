#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(scheduler)
set(SCHEDULER_OUTDIR_WITH_ARCH "${TargetDir}/scheduler/${NEO_ARCH}")
set_target_properties(scheduler PROPERTIES FOLDER "scheduler")

set (SCHEDULER_KERNEL scheduler.cl)
set (SCHEDULER_INCLUDE_OPTIONS "-I$<JOIN:${IGDRCL__IGC_INCLUDE_DIR}, -I>")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
  set(SCHEDULER_DEBUG_OPTION "-D DEBUG")
else()
  set(SCHEDULER_DEBUG_OPTION "")
endif()

set(SCHEDULER_INCLUDE_DIR ${TargetDir})

function(compile_kernel target gen_type platform_type kernel)
  get_family_name_with_type(${gen_type} ${platform_type})
  string(TOLOWER ${gen_type} gen_type_lower)
  # get filename
  set(OUTPUTDIR "${SCHEDULER_OUTDIR_WITH_ARCH}/${gen_type_lower}")
  set(SCHEDULER_INCLUDE_OPTIONS "${SCHEDULER_INCLUDE_OPTIONS} -I ../${gen_type_lower}")

  get_filename_component(BASENAME ${kernel} NAME_WE)

  set(OUTPUTPATH "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}.bin")

  set(SCHEDULER_CPP "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}.cpp")
  if(WIN32)
    set(cloc_cmd_prefix cloc)
  else()
    if(DEFINED IGDRCL__IGC_LIBRARY_PATH)
      set(cloc_cmd_prefix LD_LIBRARY_PATH=${IGDRCL__IGC_LIBRARY_PATH} $<TARGET_FILE:cloc>)
    else()
      set(cloc_cmd_prefix LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc>)
    endif()
  endif()
  add_custom_command(
    OUTPUT ${OUTPUTPATH}
    COMMAND ${cloc_cmd_prefix} -q -file ${kernel} -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} -cl-intel-greater-than-4GB-buffer-required -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "-cl-kernel-arg-info ${SCHEDULER_INCLUDE_OPTIONS} ${SCHEDULER_DEBUG_OPTION} -cl-std=CL2.0"
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${kernel} cloc copy_compiler_files
  )
  set(SCHEDULER_CPP ${SCHEDULER_CPP} PARENT_SCOPE)

  add_custom_target(${target} DEPENDS ${OUTPUTPATH})
  set_target_properties(${target} PROPERTIES FOLDER "scheduler/${gen_type_lower}")
endfunction()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE "CORE" "LP")
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      set(PLATFORM_2_0_LOWER ${DEFAULT_SUPPORTED_2_0_${GEN_TYPE}_${PLATFORM_TYPE}_PLATFORM})
      if(COMPILE_BUILT_INS AND PLATFORM_2_0_LOWER)
        compile_kernel(scheduler_${family_name_with_type} ${GEN_TYPE} ${PLATFORM_TYPE} ${SCHEDULER_KERNEL})
        add_dependencies(scheduler scheduler_${family_name_with_type})
        list(APPEND SCHEDULER_TARGETS scheduler_${family_name_with_type})
        list(APPEND GENERATED_SCHEDULER_CPPS ${SCHEDULER_CPP})
      endif()
    endif()
  endforeach()
  source_group("generated files\\${GEN_TYPE_LOWER}" FILES ${GENERATED_SCHEDULER_CPPS})
endmacro()

apply_macro_for_each_gen("SUPPORTED")

add_library(${SCHEDULER_BINARY_LIB_NAME} OBJECT CMakeLists.txt)

if(COMPILE_BUILT_INS)
  target_sources(${SCHEDULER_BINARY_LIB_NAME} PUBLIC ${GENERATED_SCHEDULER_CPPS})
  set_source_files_properties(${GENERATED_SCHEDULER_CPPS} PROPERTIES GENERATED TRUE)
  foreach(SCHEDULER_TARGET ${SCHEDULER_TARGETS})
    add_dependencies(${SCHEDULER_BINARY_LIB_NAME} ${SCHEDULER_TARGET})
  endforeach()
endif(COMPILE_BUILT_INS)

set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES FOLDER "scheduler")

add_dependencies(${SCHEDULER_BINARY_LIB_NAME} scheduler)

target_include_directories(${SCHEDULER_BINARY_LIB_NAME} PRIVATE
  ${KHRONOS_HEADERS_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
)
