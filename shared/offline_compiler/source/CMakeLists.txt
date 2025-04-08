#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(${OCLOC_NAME}_lib)

set(OCLOC_NAME "ocloc")
set(OCLOC_FOLDER_NAME "offline_compiler")

append_sources_from_properties(RELEASE_HELPER_SOURCES
                               NEO_CORE_RELEASE_HELPER
                               NEO_CORE_ENABLE_RELEASE_HELPER
)

set(CLOC_LIB_SRCS_LIB
    ${RELEASE_HELPER_SOURCES}
    ${NEO_SHARED_DIRECTORY}/ail/ail_configuration.h
    ${NEO_SHARED_DIRECTORY}/ail/ail_configuration.cpp
    ${NEO_SHARED_DIRECTORY}/ail${BRANCH_DIR_SUFFIX}ail_configuration_extra.cpp
    ${NEO_SHARED_DIRECTORY}/ail/ail_configuration_base.inl
    ${NEO_SHARED_DIRECTORY}/ail/ail_ov_comp_wa.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options_extra.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface${BRANCH_DIR_SUFFIX}compiler_options_extra.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_cache.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_cache.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/create_main.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface${BRANCH_DIR_SUFFIX}oclc_extensions_extra.cpp
    ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions_extra.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/igc_platform_helper.h
    ${NEO_SHARED_DIRECTORY}/compiler_interface/igc_platform_helper.cpp
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
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin/zebin_decoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin/zebin_decoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin/zeinfo_decoder.cpp
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin/zeinfo_decoder.h
    ${NEO_SHARED_DIRECTORY}/device_binary_format/zebin/${BRANCH_DIR_SUFFIX}zeinfo_decoder_ext.cpp
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}devices.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices/devices_base.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}/product_config.inl
    ${NEO_SHARED_DIRECTORY}/dll/devices/product_config_base.inl
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_product_helper.h
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_product_helper.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_product_helper_base.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_aot_config_tgllp_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_product_helper_before_xe_hpc.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_product_helper_xe_hpc_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_options_parser.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/compiler_options_parser.h
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy.h
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy_tgllp_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/cache_policy_dg2_and_later.inl
    ${NEO_SHARED_DIRECTORY}/helpers/debug_helpers.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info.h
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info_helper.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/hw_info_helper.h
    ${NEO_SHARED_DIRECTORY}/helpers/path.h
    ${NEO_SHARED_DIRECTORY}/helpers/product_config_helper.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/product_config_helper.h
    ${NEO_SHARED_DIRECTORY}/kernel/${BRANCH_DIR_SUFFIX}kernel_descriptor_ext.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/os_library.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/os_library.h
    ${NEO_SHARED_DIRECTORY}/sku_info/definitions${BRANCH_DIR_SUFFIX}sku_info.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/directory.h
    ${NEO_SHARED_DIRECTORY}/utilities/io_functions.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/io_functions.h
    ${NEO_SHARED_DIRECTORY}/utilities/logger.cpp
    ${NEO_SHARED_DIRECTORY}/utilities/logger.h
    ${OCLOC_DIRECTORY}/source/default_cache_config.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_decoder.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_decoder.h
    ${OCLOC_DIRECTORY}/source/decoder/binary_encoder.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_encoder.h
    ${OCLOC_DIRECTORY}/source/decoder/helper.cpp
    ${OCLOC_DIRECTORY}/source/decoder/helper.h
    ${OCLOC_DIRECTORY}/source/decoder/iga_wrapper.h
    ${OCLOC_DIRECTORY}/source/decoder/translate_platform_base.h
    ${OCLOC_DIRECTORY}/source/decoder/zebin_manipulator.cpp
    ${OCLOC_DIRECTORY}/source/decoder/zebin_manipulator.h
    ${OCLOC_DIRECTORY}/source/multi_command.cpp
    ${OCLOC_DIRECTORY}/source/multi_command.h
    ${OCLOC_DIRECTORY}/source/ocloc_api.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_api.h
    ${OCLOC_DIRECTORY}/source/offline_compiler_ext.h
    ${OCLOC_DIRECTORY}/source${BRANCH_DIR_SUFFIX}/offline_compiler_ext.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_arg_helper.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_arg_helper.h
    ${OCLOC_DIRECTORY}/source/ocloc_concat.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_concat.h
    ${OCLOC_DIRECTORY}/source/ocloc_dll_options.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_dll_options.h
    ${OCLOC_DIRECTORY}/source/ocloc_fatbinary.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_fatbinary.h
    ${OCLOC_DIRECTORY}/source/ocloc_fcl_facade.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_fcl_facade.h
    ${OCLOC_DIRECTORY}/source/ocloc_igc_facade.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_igc_facade.h
    ${OCLOC_DIRECTORY}/source/ocloc_interface.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_interface.h
    ${OCLOC_DIRECTORY}/source/ocloc_supported_devices_helper.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_supported_devices_helper.h
    ${OCLOC_DIRECTORY}/source/ocloc_validator.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_validator.h
    ${OCLOC_DIRECTORY}/source/offline_compiler.cpp
    ${OCLOC_DIRECTORY}/source/offline_compiler.h
    ${OCLOC_DIRECTORY}/source/offline_compiler_helper.cpp
    ${NEO_SOURCE_DIR}/shared/source/debug_settings/debug_settings_manager.cpp
    ${NEO_SOURCE_DIR}/shared/source/utilities/debug_settings_reader.cpp
    ${NEO_SOURCE_DIR}/shared/source/utilities/debug_file_reader.cpp
    ${NEO_SOURCE_DIR}/shared/source/os_interface/debug_env_reader.cpp
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
       ${NEO_SHARED_DIRECTORY}/ail/windows/ail_configuration_windows.cpp
       ${NEO_SHARED_DIRECTORY}/compiler_interface/windows/compiler_cache_windows.cpp
       ${NEO_SHARED_DIRECTORY}/compiler_interface/windows/os_compiler_cache_helper.cpp
       ${NEO_SHARED_DIRECTORY}/debug_settings/windows/debug_variables_helper.cpp
       ${NEO_SHARED_DIRECTORY}/dll/windows${BRANCH_DIR_SUFFIX}/options_windows.cpp
       ${NEO_SHARED_DIRECTORY}/dll/windows/options_windows.inl
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_inc.h
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.h
       ${NEO_SHARED_DIRECTORY}/helpers/windows/path.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/sys_calls.cpp
       ${NEO_SHARED_DIRECTORY}/utilities/windows/directory.cpp
       ${NEO_SHARED_DIRECTORY}/ail/windows${BRANCH_DIR_SUFFIX}/ail_ov_comp_wa_windows.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/debug_registry_reader.cpp
       ${OCLOC_DIRECTORY}/source/windows/ocloc_supported_devices_helper_windows.cpp
       ${OCLOC_DIRECTORY}/source/windows/ocloc_reg_path.cpp
  )
else()
  list(APPEND CLOC_LIB_SRCS_LIB
       ${NEO_SHARED_DIRECTORY}/ail/linux/ail_configuration_linux.cpp
       ${NEO_SHARED_DIRECTORY}/compiler_interface/linux/compiler_cache_linux.cpp
       ${NEO_SHARED_DIRECTORY}/compiler_interface/linux/os_compiler_cache_helper.cpp
       ${NEO_SHARED_DIRECTORY}/debug_settings/linux/debug_variables_helper.cpp
       ${NEO_SHARED_DIRECTORY}/dll/linux${BRANCH_DIR_SUFFIX}/options_linux.cpp
       ${NEO_SHARED_DIRECTORY}/dll/linux/options_linux.inl
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_inc.h
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_helper.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.h
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/settings_reader_create.cpp
       ${NEO_SHARED_DIRECTORY}/helpers/linux/path.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
       ${NEO_SHARED_DIRECTORY}/utilities/linux/directory.cpp
       ${NEO_SHARED_DIRECTORY}/ail/linux/ail_ov_comp_wa_linux.cpp
       ${OCLOC_DIRECTORY}/source/linux/ocloc_supported_devices_helper_linux.cpp
       ${OCLOC_DIRECTORY}/source/linux/ocloc_reg_path.cpp
  )
endif()

set(ALL_OCLOC_PRODUCT_FAMILY "")
set(ALL_OCLOC_PRODUCT_TO_PRODUCT_FAMILY "")
set(OCLOC_SUPPORTED_CORE_FLAGS_DEFINITONS "")

get_cmake_property(OCLOC_SUPPORT_VARS VARIABLES)
list(FILTER OCLOC_SUPPORT_VARS INCLUDE REGEX "OCLOC_SUPPORT_.+")
foreach(OCLOC_SUPPORT_VAR ${OCLOC_SUPPORT_VARS})
  string(REPLACE "OCLOC_SUPPORT_" "SUPPORT_" SUPPORT_VAR ${OCLOC_SUPPORT_VAR})
  set(${SUPPORT_VAR} ${${OCLOC_SUPPORT_VAR}})
endforeach()

macro(SET_FLAGS_FOR CORE_TYPE)
  foreach(SKU_NAME ${ARGN})
    if(SUPPORT_${SKU_NAME} OR NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC)
      if(NOT SUPPORT_${CORE_TYPE})
        message(STATUS "Auto-Enabling ${CORE_TYPE} support for ${SKU_NAME} in ocloc")
        set(SUPPORT_${CORE_TYPE} TRUE)
      endif()
    endif()
    string(TOLOWER ${CORE_TYPE} MAP_${SKU_NAME}_CORE_lower)
    string(TOLOWER ${SKU_NAME} MAP_${SKU_NAME}_lower)
    set(MAP_${SKU_NAME}_CORE_lower "${CORE_PREFIX}${MAP_${SKU_NAME}_CORE_lower}${CORE_SUFFIX}")
    set(MAP_${SKU_NAME}_lower ${MAP_${SKU_NAME}_lower})
  endforeach()

  if(NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC)
    set(SUPPORT_${CORE_TYPE} TRUE)
  elseif(NOT SUPPORT_${CORE_TYPE})
    set(SUPPORT_${CORE_TYPE} ${SUPPORT_GEN_DEFAULT})
  endif()

  if(SUPPORT_${CORE_TYPE} OR NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC)
    list(APPEND ALL_SUPPORTED_CORE_FAMILIES ${CORE_TYPE})
    list(REMOVE_DUPLICATES ALL_SUPPORTED_CORE_FAMILIES)

    foreach(${CORE_TYPE}_PLATFORM ${ARGN})
      if(NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC)
        set(SUPPORT_${${CORE_TYPE}_PLATFORM} TRUE)
      elseif(NOT SUPPORT_${${CORE_TYPE}_PLATFORM})
        set(SUPPORT_${${CORE_TYPE}_PLATFORM} ${SUPPORT_PLATFORM_DEFAULT})
      endif()
    endforeach()
  endif()
endmacro()

macro(ADD_PRODUCT TYPE PRODUCT ITEM)
  if(${TYPE} MATCHES "SUPPORTED")
    list(APPEND ALL_OCLOC_PRODUCT_FAMILY ${ITEM})
    list(APPEND ALL_OCLOC_PRODUCT_TO_PRODUCT_FAMILY ${PRODUCT})
  endif()
endmacro()

macro(ENABLE_ADDITIONAL_SKU SKU_NAME)
  set(SUPPORT_${SKU_NAME} TRUE)
endmacro()

macro(TEST_ADDITIONAL_SKU SKU_NAME)
endmacro()

macro(DISABLE_WDDM_LINUX_FOR SKU_NAME)
endmacro()

macro(DISABLE_FLAGS_FOR CORE_TYPE)
  set(SUPPORT_${CORE_TYPE} FALSE)
  foreach(SKU_NAME ${ARGN})
    set(SUPPORT_${SKU_NAME} FALSE)
  endforeach()
endmacro()
if(NOT NEO_SINGLE_SKU_BUILD)
  set(NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC TRUE)
endif()
include(${NEO_SOURCE_DIR}/cmake/setup_platform_flags.cmake)

unset(NEO_FORCE_ENABLE_PLATFORMS_FOR_OCLOC)
set(NEO_SKIP_BASE_PLATFORMS TRUE)
if(NOT ${NEO_SOURCE_DIR}/cmake/setup_platform_flags.cmake MATCHES ${NEO_SOURCE_DIR}/cmake${BRANCH_DIR_SUFFIX}setup_platform_flags.cmake)
  include(${NEO_SOURCE_DIR}/cmake${BRANCH_DIR_SUFFIX}setup_platform_flags.cmake)
endif()

message(STATUS "[OCLOC] All supported platforms: ${ALL_OCLOC_PRODUCT_TO_PRODUCT_FAMILY}")
foreach(PRODUCT ${ALL_OCLOC_PRODUCT_TO_PRODUCT_FAMILY})
  add_definitions("-DSUPPORT_${PRODUCT}")
endforeach()

string(REPLACE ";" "," ALL_SUPPORTED_PRODUCT_FAMILIES "${ALL_OCLOC_PRODUCT_FAMILY}")

set(CLOC_LIB_LIB_FLAGS_DEFINITIONS
    -DCIF_HEADERS_ONLY_BUILD
    -DALL_SUPPORTED_PRODUCT_FAMILIES=${ALL_SUPPORTED_PRODUCT_FAMILIES}
)

set(NEO_SHARED_COREX_CPP_FILES
    hw_info
    enable
)

macro(macro_for_each_platform)
  list(APPEND OCLOC_SUPPORTED_CORE_FLAGS_DEFINITONS "-DSUPPORT_${PLATFORM_IT}")
  if(EXISTS ${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX})
    include_directories(${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX})
  elseif(EXISTS ${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/definitions)
    include_directories(${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/definitions)
  endif()
  include_directories(${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX})
  include_directories(${NEO_SHARED_DIRECTORY}/ail/${CORE_TYPE_LOWER}/${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX})
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      foreach(SRC_FILE ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}hw_info_setup_${PLATFORM_IT_LOWER}.inl
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX}compiler_product_helper_${PLATFORM_IT_LOWER}.inl
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}device_ids_configs_${PLATFORM_IT_LOWER}.cpp
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}hw_info_${PLATFORM_IT_LOWER}.cpp
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}compiler_product_helper_${PLATFORM_IT_LOWER}.inl
                ${NEO_SOURCE_DIR}/shared/source${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}enable_compiler_product_helper_${PLATFORM_IT_LOWER}.cpp
                ${NEO_SOURCE_DIR}/shared/source/ail${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/ail_configuration_${PLATFORM_IT_LOWER}.cpp
        )
        if(${SRC_FILE} IN_LIST CLOC_LIB_SRCS_LIB)
          continue()
        endif()
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
  list(APPEND OCLOC_SUPPORTED_CORE_FLAGS_DEFINITONS "-DSUPPORT_${CORE_TYPE}")
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
  apply_macro_for_each_platform("SUPPORTED")
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

set(CLOC_LIB_SRCS
    ${CLOC_LIB_SRCS_LIB}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/file_io_load.cpp
)
add_library(${OCLOC_NAME}_lib SHARED ${CLOC_LIB_SRCS}
            ${NEO_SOURCE_DIR}/shared/source/utilities/debug_settings_reader_creator.cpp
)
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

if(WIN32)
  list(APPEND CLOC_LIB_INCLUDES
       ${NEO_SHARED_DIRECTORY}/os_interface/windows
  )
else()
  list(APPEND CLOC_LIB_INCLUDES
       ${NEO_SHARED_DIRECTORY}/os_interface/linux
  )
endif()

target_include_directories(${OCLOC_NAME}_lib BEFORE PRIVATE ${CLOC_LIB_INCLUDES})
target_include_directories(${OCLOC_NAME}_lib BEFORE PRIVATE ${IGA_INCLUDE_DIR})

target_compile_definitions(${OCLOC_NAME}_lib PUBLIC ${CLOC_LIB_LIB_FLAGS_DEFINITIONS} ${OCLOC_SUPPORTED_CORE_FLAGS_DEFINITONS} DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM}
                           IGA_LIBRARY_NAME="${IGA_LIBRARY_FILENAME}"
                           ${AOT_DEFINITIONS}
)
target_compile_definitions(${OCLOC_NAME}_lib PUBLIC ${NEO__IGC_COMPILE_DEFINITIONS})

if(WIN32)
  target_link_libraries(${OCLOC_NAME}_lib dbghelp)
endif()

if(UNIX)
  target_link_libraries(${OCLOC_NAME}_lib dl pthread)
  set_property(TARGET ${OCLOC_NAME}_lib
               APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/ocloc.exports"
  )
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

if(WIN32)
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

  if(NEO_CURRENT_PLATFORMS_SUPPORT)
    install(FILES ${OCLOC_DIRECTORY}/source/ocloc_api.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
            COMPONENT ${OCLOC_NAME}
    )
  endif()
elseif(WIN32)
  if(NEO_WINDOWS_INSTALL)
    install(TARGETS ${OCLOC_NAME}
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ocloc
    )
    install(TARGETS ${OCLOC_NAME}_lib
            RUNTIME
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ocloc
            ARCHIVE
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ocloc
    )
    install(FILES ${OCLOC_DIRECTORY}/source/ocloc_api.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            COMPONENT ocloc
    )
  endif()
endif()

if(WIN32)
  set(LIBOCLOC_OUTPUT_NAME_SUFFIX ${OCLOC_BITNESS_SUFFIX})
elseif(UNIX)
  if(NOT NEO_CURRENT_PLATFORMS_SUPPORT)
    set(LIBOCLOC_OUTPUT_NAME_SUFFIX ${NEO__SO_NAME_SUFFIX})
  elseif(NEO_CURRENT_PLATFORMS_SUPPORT AND NOT NEO_LEGACY_PLATFORMS_SUPPORT)
    set(NEO_OCLOC_FORMER_LIB_NAME ${CMAKE_SHARED_LIBRARY_PREFIX}${OCLOC_NAME}${NEO__LEGACY_NAME_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()

  if(NOT NEO_BUILD_UNVERSIONED_OCLOC)
    set(OCLOC_OUTPUT_NAME_SUFFIX "-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_OCLOC_VERSION_MODE}")
  endif()
endif()
set_target_properties(${OCLOC_NAME} PROPERTIES OUTPUT_NAME ${OCLOC_NAME}${OCLOC_OUTPUT_NAME_SUFFIX})
set_target_properties(${OCLOC_NAME}_lib PROPERTIES OUTPUT_NAME ${OCLOC_NAME}${LIBOCLOC_OUTPUT_NAME_SUFFIX})

set(NEO_OCLOC_CURRENT_LIB_NAME $<TARGET_FILE_NAME:${OCLOC_NAME}_lib>)

message(STATUS "ocloc_lib current name: ${NEO_OCLOC_CURRENT_LIB_NAME}")
message(STATUS "ocloc_lib former name: ${NEO_OCLOC_FORMER_LIB_NAME}")

add_definitions(-DNEO_OCLOC_CURRENT_LIB_NAME="${NEO_OCLOC_CURRENT_LIB_NAME}")
add_definitions(-DNEO_OCLOC_FORMER_LIB_NAME="${NEO_OCLOC_FORMER_LIB_NAME}")

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
