#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_custom_target(scheduler)
set(OPENCL_SCHEDULER_PROJECTS_FOLDER "scheduler")
set(SCHEDULER_OUTDIR_WITH_ARCH "${TargetDir}/scheduler/${NEO_ARCH}")
set_target_properties(scheduler PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_SCHEDULER_PROJECTS_FOLDER}")

set(SCHEDULER_KERNEL scheduler.cl)
if(DEFINED NEO__IGC_INCLUDE_DIR)
  list(APPEND __cloc__options__ "-I$<JOIN:${NEO__IGC_INCLUDE_DIR}, -I>")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  list(APPEND __cloc__options__ "-D DEBUG")
endif()

set(SCHEDULER_INCLUDE_DIR ${TargetDir})

function(compile_kernel target gen_type platform_type kernel)
  get_family_name_with_type(${gen_type} ${platform_type})
  string(TOLOWER ${gen_type} gen_type_lower)
  # get filename
  set(OUTPUTDIR "${SCHEDULER_OUTDIR_WITH_ARCH}/${gen_type_lower}")
  list(APPEND __cloc__options__ "-I ../${gen_type_lower}")

  get_filename_component(BASENAME ${kernel} NAME_WE)

  set(OUTPUTPATH "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}.bin")

  set(SCHEDULER_CPP "${OUTPUTDIR}/${BASENAME}_${family_name_with_type}.cpp")

  list(APPEND __cloc__options__ "-cl-kernel-arg-info")
  list(APPEND __cloc__options__ "-cl-std=CL2.0")
  list(APPEND __cloc__options__ "-cl-intel-disable-a64WA")
  add_custom_command(
                     OUTPUT ${OUTPUTPATH}
                     COMMAND ocloc -q -file ${kernel} -device ${DEFAULT_SUPPORTED_${gen_type}_${platform_type}_PLATFORM} -cl-intel-greater-than-4GB-buffer-required -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "$<JOIN:${__cloc__options__}, >" -internal_options "-cl-intel-no-spill"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${kernel} ocloc copy_compiler_files
  )
  set(SCHEDULER_CPP ${SCHEDULER_CPP} PARENT_SCOPE)

  add_custom_target(${target} DEPENDS ${OUTPUTPATH})
  set_target_properties(${target} PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_SCHEDULER_PROJECTS_FOLDER}/${gen_type_lower}")
endfunction()

macro(macro_for_each_gen)
  foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
    if(${GEN_TYPE}_HAS_${PLATFORM_TYPE} AND SUPPORT_DEVICE_ENQUEUE_${GEN_TYPE})
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

add_library(${SCHEDULER_BINARY_LIB_NAME} OBJECT EXCLUDE_FROM_ALL CMakeLists.txt)

if(COMPILE_BUILT_INS)
  target_sources(${SCHEDULER_BINARY_LIB_NAME} PUBLIC ${GENERATED_SCHEDULER_CPPS})
  set_source_files_properties(${GENERATED_SCHEDULER_CPPS} PROPERTIES GENERATED TRUE)
  foreach(SCHEDULER_TARGET ${SCHEDULER_TARGETS})
    add_dependencies(${SCHEDULER_BINARY_LIB_NAME} ${SCHEDULER_TARGET})
  endforeach()
endif()

set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${SCHEDULER_BINARY_LIB_NAME} PROPERTIES FOLDER "${OPENCL_RUNTIME_PROJECTS_FOLDER}/${OPENCL_SCHEDULER_PROJECTS_FOLDER}")

add_dependencies(${SCHEDULER_BINARY_LIB_NAME} scheduler)

target_include_directories(${SCHEDULER_BINARY_LIB_NAME} PRIVATE
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
)
