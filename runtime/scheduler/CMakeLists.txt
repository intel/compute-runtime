# Copyright (c) 2017, Intel Corporation
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

add_custom_target(scheduler)
set (SCHEDULER_OUTDIR_WITH_ARCH "${TargetDir}/scheduler/${NEO_ARCH}")
set_target_properties(scheduler PROPERTIES FOLDER "scheduler")

# Set sources variable for parent CMakeLists file.
set (RUNTIME_SRCS_SCHEDULER
  scheduler/scheduler.cl
  PARENT_SCOPE
)

set (SCHEDULER_KERNEL scheduler.cl)
set (SCHEDULER_INCLUDE_OPTIONS "-I$<JOIN:${IGDRCL__IGC_INCLUDE_DIR}, -I>")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
    set(SCHEDULER_DEBUG_OPTION "-D DEBUG")
else()
    set(SCHEDULER_DEBUG_OPTION "")
endif()

set(SCHEDULER_INCLUDE_DIR ${TargetDir})
set(SCHEDULER_CPP "")

function(compile_kernel target gen_name gen_num kernel)
  set(OUTPUTDIR "${SCHEDULER_OUTDIR_WITH_ARCH}/${gen_name}")
  set(SCHEDULER_INCLUDE_OPTIONS "${SCHEDULER_INCLUDE_OPTIONS} -I ../gen${gen_num}")

  get_filename_component(BASENAME ${kernel} NAME_WE)

  set(OUTPUTPATH "${OUTPUTDIR}/${BASENAME}_${gen_name}.bin")

  unset(SCHEDULER_CPP)
  set(SCHEDULER_CPP_TEMP ${OUTPUTDIR}/${BASENAME}_${gen_name}.cpp)
  set(SCHEDULER_CPP scheduler/${NEO_ARCH}/${gen_name}/${BASENAME}_${gen_name}.cpp PARENT_SCOPE)

  if(MSVC)
    add_custom_command(
      OUTPUT ${OUTPUTPATH} ${SCHEDULER_CPP_TEMP}
      COMMAND cloc -q -file ${kernel} -device ${gen_name} -cl-intel-greater-than-4GB-buffer-required -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "-cl-kernel-arg-info ${SCHEDULER_INCLUDE_OPTIONS} ${SCHEDULER_DEBUG_OPTION} -cl-std=CL2.0"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${kernel} cloc copy_compiler_files
    )
  else()
    add_custom_command(
      OUTPUT ${OUTPUTPATH} ${SCHEDULER_CPP_TEMP}
      COMMAND LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cloc> $<TARGET_FILE:cloc> -q -file ${kernel} -device ${gen_name} -cl-intel-greater-than-4GB-buffer-required -${NEO_BITS} -out_dir ${OUTPUTDIR} -cpp_file -options "-cl-kernel-arg-info ${SCHEDULER_INCLUDE_OPTIONS} ${SCHEDULER_DEBUG_OPTION} -cl-std=CL2.0"
      WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${kernel} cloc copy_compiler_files
    )
  endif()

  add_custom_target(${target} DEPENDS ${OUTPUTPATH})
  set_target_properties(${target} PROPERTIES FOLDER "scheduler/${gen_name}")
endfunction()

foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("SUPPORTED" ${GEN_NUM} SUPPORTED_GENX_PLATFORMS)

    foreach(PLATFORM_IT ${SUPPORTED_GENX_PLATFORMS})
      PLATFORM_HAS_2_0(${GEN_NUM} ${PLATFORM_IT} PLATFORM_SUPPORTS_2_0)
      if(COMPILE_BUILT_INS AND ${PLATFORM_SUPPORTS_2_0})
        string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
        compile_kernel(scheduler_${PLATFORM_IT_LOWER} ${PLATFORM_IT_LOWER} ${GEN_NUM} ${SCHEDULER_KERNEL})
        add_dependencies(scheduler scheduler_${PLATFORM_IT_LOWER})
        set (RUNTIME_GENERATED_SCHEDULER_GEN${GEN_NUM}_${PLATFORM_IT} ${SCHEDULER_CPP})
      endif(COMPILE_BUILT_INS AND ${PLATFORM_SUPPORTS_2_0})
      list(APPEND GENERATED_SCHEDULER_CPPS ${SCHEDULER_INCLUDE_DIR}/${RUNTIME_GENERATED_SCHEDULER_GEN${GEN_NUM}_${PLATFORM_IT}})
    endforeach(PLATFORM_IT)

    source_group("generated files\\gen${GEN_NUM}" FILES ${GENERATED_SCHEDULER_CPPS})
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)

add_library(${SCHEDULER_BINARY_LIB_NAME} OBJECT CMakeLists.txt)

if(COMPILE_BUILT_INS)
  target_sources(${SCHEDULER_BINARY_LIB_NAME} PUBLIC ${GENERATED_SCHEDULER_CPPS})
  set_source_files_properties(${GENERATED_SCHEDULER_CPPS} PROPERTIES GENERATED TRUE)
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
