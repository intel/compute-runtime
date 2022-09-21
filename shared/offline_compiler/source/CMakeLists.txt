#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(${OCLOC_NAME}_lib)

set(OCLOC_NAME "ocloc")
set(OCLOC_FOLDER_NAME "offline_compiler")

set(CLOC_LIB_SRCS_LIB
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/create_main.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/ar/ar.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/ar/ar_decoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/ar/ar_decoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/ar/ar_encoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/ar/ar_encoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/elf.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/elf_decoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/elf_decoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/elf_encoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/elf_encoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/elf/ocl_elf.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/device_binary_formats.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/yaml/yaml_parser.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin_decoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin_decoder.h
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}devices.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}devices_additional.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices/devices_base.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}/product_config.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices/product_config_base.inl
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config.h
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config_base.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_aot_config_bdw_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config_bdw_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config_before_xe_hpc.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_hw_info_config_xe_hpc_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_options_parser.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_options_parser.h
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy.h
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy_bdw_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy_dg2_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/debug_helpers.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info.h
    ${NEO_SHARED_DIRECTORY}/helpers${BRANCH_DIR_SUFFIX}hw_info_extended.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/product_config_helper.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/product_config_helper.h
    ${NEO_SHARED_DIRECTORY}/os_interface/os_library.h
    ${OCLOC_DIRECTORY}/source/decoder/binary_decoder.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_decoder.h
    ${OCLOC_DIRECTORY}/source/decoder/binary_encoder.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_encoder.h
    ${OCLOC_DIRECTORY}/source/decoder/helper.cpp
    ${OCLOC_DIRECTORY}/source/decoder/helper.h
    ${OCLOC_DIRECTORY}/source/decoder/iga_wrapper.h
    ${OCLOC_DIRECTORY}/source/decoder/translate_platform_base.h
    ${OCLOC_DIRECTORY}/source/multi_command.cpp
    ${OCLOC_DIRECTORY}/source/multi_command.h
    ${OCLOC_DIRECTORY}/source/ocloc_api.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_api.h
    ${OCLOC_DIRECTORY}/source/ocloc_arg_helper.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_arg_helper.h
    ${OCLOC_DIRECTORY}/source/ocloc_concat.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_concat.h
    ${OCLOC_DIRECTORY}/source/ocloc_dll_options.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_dll_options.h
    ${OCLOC_DIRECTORY}/source/ocloc_error_code.h
    ${OCLOC_DIRECTORY}/source/ocloc_fatbinary.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_fatbinary.h
    ${OCLOC_DIRECTORY}/source/ocloc_fcl_facade.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_fcl_facade.h
    ${OCLOC_DIRECTORY}/source/ocloc_igc_facade.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_igc_facade.h
    ${OCLOC_DIRECTORY}/source/ocloc_validator.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_validator.h
    ${OCLOC_DIRECTORY}/source/offline_compiler.cpp
    ${OCLOC_DIRECTORY}/source/offline_compiler.h
    ${OCLOC_DIRECTORY}/source/offline_compiler_helper.cpp
    ${OCLOC_DIRECTORY}/source/offline_compiler_options.cpp
    ${OCLOC_DIRECTORY}/source/offline_linker.cpp
    ${OCLOC_DIRECTORY}/source/offline_linker.h
    ${OCLOC_DIRECTORY}/source/queries.h
    ${OCLOC_DIRECTORY}/source/utilities/get_git_version_info.h
    ${OCLOC_DIRECTORY}/source/utilities/get_git_version_info.cpp
    ${NEO_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}aot_config_headers/platforms.h
)

if(${IGA_HEADERS_AVAILABLE})
  set(CLOC_LIB_SRCS_LIB ${CLOC_LIB_SRCS_LIB}
      ${OCLOC_DIRECTORY}/source/decoder/iga_wrapper.cpp
      ${OCLOC_DIRECTORY}/source/decoder${BRANCH_DIR_SUFFIX}translate_platform.cpp
  )
else()
  set(CLOC_LIB_SRCS_LIB ${CLOC_LIB_SRCS_LIB}
      ${OCLOC_DIRECTORY}/source/decoder/iga_stubs.cpp
  )
endif()

if(WIN32)
  list(APPEND CLOC_LIB_SRCS_LIB
       ${NEO_SHARED_DIRECTORY}/dll/windows/options_windows.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.h
  )
else()
  list(APPEND CLOC_LIB_SRCS_LIB
       ${NEO_SHARED_DIRECTORY}/dll/linux/options_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.h
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
       ${OCLOC_DIRECTORY}/source/linux/os_library_ocloc_helper.cpp
  )
endif()

string(REPLACE ";" "," ALL_SUPPORTED_PRODUCT_FAMILIES "${ALL_SUPPORTED_PRODUCT_FAMILY}")

set(CLOC_LIB_LIB_FLAGS_DEFINITIONS
    -DCIF_HEADERS_ONLY_BUILD
    -DALL_SUPPORTED_PRODUCT_FAMILIES=${ALL_SUPPORTED_PRODUCT_FAMILIES}
)

set(NEO_SHARED_COREX_CPP_FILES
    hw_info
)

macro(macro_for_each_platform)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      foreach(SRC_FILE ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}hw_info_setup_${PLATFORM_IT_LOWER}.inl
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}hw_info_${PLATFORM_IT_LOWER}.cpp
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}/compiler_hw_info_config_${PLATFORM_IT_LOWER}.inl
        )
        if(EXISTS ${SRC_FILE})
          list(APPEND CLOC_LIB_SRCS_LIB ${SRC_FILE})
        endif()
      endforeach()

      if(WIN32)
        set(SRC_FILE ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}windows/hw_info_extra_${PLATFORM_IT_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND CLOC_LIB_SRCS_LIB ${SRC_FILE})
        endif()
      else()
        set(SRC_FILE ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}linux/hw_info_extra_${PLATFORM_IT_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND CLOC_LIB_SRCS_LIB ${SRC_FILE})
        endif()
      endif()

    endforeach()
  endforeach()
endmacro()

macro(macro_for_each_core_type)
  foreach(SRC_IT ${NEO_SHARED_COREX_CPP_FILES})
    foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
      foreach(BRANCH ${BRANCH_DIR_LIST})
        set(SRC_FILE ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}${SRC_IT})
        if(EXISTS ${SRC_FILE}_${CORE_TYPE_LOWER}.cpp)
          list(APPEND CLOC_LIB_SRCS_LIB ${SRC_FILE}_${CORE_TYPE_LOWER}.cpp)
        endif()
      endforeach()
    endforeach()
  endforeach()
  apply_macro_for_each_platform()
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

set(CLOC_LIB_SRCS
    ${CLOC_LIB_SRCS_LIB}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
add_library(${OCLOC_NAME}_lib SHARED ${CLOC_LIB_SRCS})

add_subdirectories()

create_project_source_tree(${OCLOC_NAME}_lib)
set_target_properties(${OCLOC_NAME}_lib PROPERTIES FOLDER ${OCLOC_FOLDER_NAME})

set(CLOC_LIB_INCLUDES
    ${AOT_CONFIG_HEADERS_DIR}
    ${ENGINE_NODE_DIR}
    ${IGC_OCL_ADAPTOR_DIR}
    ${CIF_BASE_DIR}
    ${NEO__GMM_INCLUDE_DIR}
    ${KHRONOS_HEADERS_DIR}
    ${NEO__IGC_INCLUDE_DIR}
)

target_include_directories(${OCLOC_NAME}_lib BEFORE PRIVATE ${CLOC_LIB_INCLUDES})
target_include_directories(${OCLOC_NAME}_lib BEFORE PRIVATE ${IGA_INCLUDE_DIR})

target_compile_definitions(${OCLOC_NAME}_lib PUBLIC ${CLOC_LIB_LIB_FLAGS_DEFINITIONS} ${SUPPORTED_CORE_FLAGS_DEFINITONS} DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM}
                           IGA_LIBRARY_NAME="${CMAKE_SHARED_LIBRARY_PREFIX}${IGA_LIBRARY_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
)
target_compile_definitions(${OCLOC_NAME}_lib PUBLIC ${NEO__IGC_COMPILE_DEFINITIONS})

if(MSVC)
  target_link_libraries(${OCLOC_NAME}_lib dbghelp)
endif()

if(UNIX)
  target_link_libraries(${OCLOC_NAME}_lib dl pthread)
endif()

set(CLOC_LIB_SRCS_LIB ${CLOC_LIB_SRCS_LIB} PARENT_SCOPE)

set(OCLOC_SRCS
    ${OCLOC_DIRECTORY}/source/ocloc_api.h
    ${OCLOC_DIRECTORY}/source/main.cpp
)

add_executable(${OCLOC_NAME} ${OCLOC_SRCS})
target_link_libraries(${OCLOC_NAME} ${OCLOC_NAME}_lib)

create_project_source_tree(${OCLOC_NAME})
set_target_properties(${OCLOC_NAME} PROPERTIES FOLDER ${OCLOC_FOLDER_NAME})

if(MSVC)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(OCLOC_BITNESS_SUFFIX 32)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OCLOC_BITNESS_SUFFIX 64)
  endif()
else()
  set(OCLOC_BITNESS_SUFFIX "")
endif()

if(UNIX)
  if(NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
    get_filename_component(lib_file_name $<TARGET_FILE:${OCLOC_NAME}_lib> NAME_WE)
    set(symbols_file_name ${lib_file_name}.debug)
    set(debug_symbols_target_name "${STRIP_SYMBOLS_TARGET}_${OCLOC_NAME}_lib")
    add_custom_target(${debug_symbols_target_name}
                      COMMAND sh -c "objcopy --only-keep-debug ${lib_file_name} ${symbols_file_name}"
                      COMMAND sh -c "strip -g ${lib_file_name}"
                      COMMAND sh -c "objcopy --add-gnu-debuglink=${symbols_file_name} ${lib_file_name}"
    )
    add_dependencies(${debug_symbols_target_name} ${OCLOC_NAME}_lib)
    add_dependencies(${STRIP_SYMBOLS_TARGET} ${debug_symbols_target_name})
    set_property(GLOBAL APPEND PROPERTY DEBUG_SYMBOL_FILES "${symbols_file_name}")
  endif()

  set_property(GLOBAL APPEND PROPERTY NEO_OCL_COMPONENTS_LIST ${OCLOC_NAME})
  install(FILES $<TARGET_FILE:${OCLOC_NAME}>
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          COMPONENT ${OCLOC_NAME}
  )
  install(FILES $<TARGET_FILE:${OCLOC_NAME}_lib>
          DESTINATION ${CMAKE_INSTALL_LIBDIR}
          PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          COMPONENT ${OCLOC_NAME}
  )
  install(FILES ${OCLOC_DIRECTORY}/source/ocloc_api.h
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
          COMPONENT ${OCLOC_NAME}
  )
elseif(WIN32)
  if(NEO_WINDOWS_INSTALL)
    install(TARGETS ${OCLOC_NAME}
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ocloc
    )
    install(TARGETS ${OCLOC_NAME}_lib
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ocloc
    )
    install(FILES ${OCLOC_DIRECTORY}/source/ocloc_api.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            COMPONENT ocloc
    )
  endif()
endif()

set(OCLOC_OUTPUT_NAME "${OCLOC_NAME}${OCLOC_BITNESS_SUFFIX}")
set_target_properties(${OCLOC_NAME}_lib PROPERTIES OUTPUT_NAME ${OCLOC_OUTPUT_NAME})

add_custom_target(copy_compiler_files DEPENDS ${NEO__IGC_TARGETS})
set_target_properties(copy_compiler_files PROPERTIES FOLDER ${OCLOC_FOLDER_NAME})

if(WIN32)
  foreach(TARGET_tmp ${NEO__IGC_TARGETS})
    add_custom_command(
                       TARGET copy_compiler_files
                       PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${OCLOC_NAME}_lib>
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${TARGET_tmp}> $<TARGET_FILE_DIR:${OCLOC_NAME}_lib>
    )
  endforeach()
endif()
