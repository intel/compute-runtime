#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(ocloc)

set(CLOC_SRCS_LIB
  ${NEO_SOURCE_DIR}/core/helpers/abort.cpp
  ${NEO_SOURCE_DIR}/core/helpers/debug_helpers.cpp
  ${NEO_SOURCE_DIR}/core/helpers/file_io.cpp
  ${NEO_SOURCE_DIR}/core/os_interface/os_library.h
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/binary_decoder.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/binary_decoder.h
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/binary_encoder.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/binary_encoder.h
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/helper.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/helper.h
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/iga_wrapper.h
  ${NEO_SOURCE_DIR}/offline_compiler/decoder/translate_platform_base.h
  ${NEO_SOURCE_DIR}/offline_compiler/offline_compiler_helper.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/offline_compiler.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/offline_compiler.h
  ${NEO_SOURCE_DIR}/offline_compiler/multi_command.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/multi_command.h
  ${NEO_SOURCE_DIR}/offline_compiler/offline_compiler_options.cpp
  ${NEO_SOURCE_DIR}/offline_compiler/${BRANCH_DIR_SUFFIX}/extra_settings.cpp
  ${NEO_SOURCE_DIR}/core/compiler_interface/create_main.cpp
  ${NEO_SOURCE_DIR}/core/helpers/hw_info.cpp
  ${NEO_SOURCE_DIR}/runtime/platform/extensions.cpp
  ${NEO_SOURCE_DIR}/runtime/platform/extensions.h
)

if(${IGA_HEADERS_AVAILABLE})
  set(CLOC_SRCS_LIB ${CLOC_SRCS_LIB}
    ${NEO_SOURCE_DIR}/offline_compiler/decoder/iga_wrapper.cpp
    ${NEO_SOURCE_DIR}/offline_compiler/decoder${BRANCH_DIR_SUFFIX}/translate_platform.cpp
  )
else()
  set(CLOC_SRCS_LIB ${CLOC_SRCS_LIB}
    ${NEO_SOURCE_DIR}/offline_compiler/decoder/iga_stubs.cpp
  )
endif()

if(WIN32)
  list(APPEND CLOC_SRCS_LIB
    ${NEO_SOURCE_DIR}/core/os_interface/windows/os_library_win.cpp
    ${NEO_SOURCE_DIR}/core/os_interface/windows/os_library_win.h
    ${NEO_SOURCE_DIR}/runtime/dll/windows/options_windows.cpp
  )
else()
  list(APPEND CLOC_SRCS_LIB
    ${NEO_SOURCE_DIR}/core/os_interface/linux/os_library_linux.cpp
    ${NEO_SOURCE_DIR}/core/os_interface/linux/os_library_linux.h
    ${NEO_SOURCE_DIR}/runtime/dll/linux/options_linux.cpp
  )
endif()

set(CLOC_LIB_FLAGS_DEFINITIONS
  -DCIF_HEADERS_ONLY_BUILD
)

set(RUNTIME_GENX_CPP_FILES
  hw_info
)

macro(macro_for_each_platform)
  list(APPEND CLOC_SRCS_LIB ${NEO_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/hw_info_${PLATFORM_IT_LOWER}.inl)
endmacro()

macro(macro_for_each_gen)
  foreach(SRC_IT ${RUNTIME_GENX_CPP_FILES})
    set(SRC_FILE ${NEO_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/${SRC_IT})
    if(EXISTS ${SRC_FILE}_${GEN_TYPE_LOWER}.cpp)
      list(APPEND CLOC_SRCS_LIB ${SRC_FILE}_${GEN_TYPE_LOWER}.cpp)
    endif()
  endforeach()
  apply_macro_for_each_platform()
  
  list(APPEND CLOC_SRCS_LIB ${NEO_SOURCE_DIR}/core/${GEN_TYPE_LOWER}/enable_${GEN_TYPE_LOWER}.cpp)
endmacro()

apply_macro_for_each_gen("SUPPORTED")

set(CLOC_SRCS
  ${CLOC_SRCS_LIB}
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
add_executable(ocloc ${CLOC_SRCS})

if(UNIX)
  install(FILES $<TARGET_FILE:ocloc>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    COMPONENT ocloc)
  set_property(GLOBAL APPEND PROPERTY NEO_COMPONENTS_LIST "ocloc")
endif()

add_subdirectories()

create_project_source_tree(ocloc)

set(CLOC_INCLUDES
  ${ENGINE_NODE_DIR}
  ${IGC_OCL_ADAPTOR_DIR}
  ${CIF_BASE_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  ${KHRONOS_HEADERS_DIR}
  ${IGDRCL__IGC_INCLUDE_DIR}
)

target_include_directories(ocloc BEFORE PRIVATE ${CLOC_INCLUDES})
target_include_directories(ocloc BEFORE PRIVATE ${IGA_INCLUDE_DIR})

target_compile_definitions(ocloc PUBLIC ${CLOC_LIB_FLAGS_DEFINITIONS} ${SUPPORTED_GEN_FLAGS_DEFINITONS} DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM} 
                                        IGA_LIBRARY_NAME=${CMAKE_SHARED_LIBRARY_PREFIX}${IGA_LIBRARY_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
target_compile_definitions(ocloc PRIVATE ${IGDRCL__IGC_COMPILE_DEFINITIONS})

if(MSVC)
  target_link_libraries(ocloc dbghelp)
endif()

if(UNIX)
  target_link_libraries(ocloc dl pthread)
endif()

target_link_libraries(ocloc elflib)

set_target_properties(ocloc PROPERTIES FOLDER "offline_compiler")

add_custom_target(copy_compiler_files DEPENDS ${IGDRCL__IGC_TARGETS})
set_target_properties(copy_compiler_files PROPERTIES FOLDER "opencl runtime")

if(WIN32)
  foreach(TARGET_tmp ${IGDRCL__IGC_TARGETS})
    add_custom_command(
      TARGET copy_compiler_files
      PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ocloc>
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${TARGET_tmp}> $<TARGET_FILE_DIR:ocloc>
    )
  endforeach()
endif()

set(CLOC_SRCS_LIB ${CLOC_SRCS_LIB} PARENT_SCOPE)
