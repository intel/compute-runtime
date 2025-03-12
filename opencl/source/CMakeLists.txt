#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

project(neo)
set(MSVC_DEF_ADDITIONAL_EXPORTS "")
set(OPENCL_RUNTIME_PROJECTS_FOLDER "opencl runtime")
set(OPENCL_BUILTINS_PROJECTS_FOLDER "built_ins")

hide_subdir(dll)

add_library(${NEO_STATIC_LIB_NAME} STATIC EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/enable_cores.cmake
)

add_subdirectories()
include(enable_cores.cmake)

if(MSVC)
  if("${IGDRCL_OPTION__BITS}" STREQUAL "32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif()
endif()

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
                           ${CMAKE_CURRENT_BINARY_DIR}
                           ${KMDAF_HEADERS_DIR}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_HEADERS_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${CIF_BASE_DIR}
                           ${IGC_OCL_ADAPTOR_DIR}
                           ${VISA_INCLUDE_DIR}
                           ${NEO__IGC_INCLUDE_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
)

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC
                           ""
                           ${NEO__IGC_COMPILE_DEFINITIONS}
)

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC ${WDK_INCLUDE_PATHS})
endif()

if(WIN32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
                             ${NEO_SHARED_DIRECTORY}/os_interface/windows
                             ${NEO_SOURCE_DIR}/opencl/source/os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL=1)
else()
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
                             ${NEO_SHARED_DIRECTORY}/os_interface/linux
                             ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux
  )
endif()

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC GMM_LIB_DLL DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_CORE_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER ${OPENCL_RUNTIME_PROJECTS_FOLDER})

set(EXPORTS_FILENAME "")
if(WIN32)
  set(EXPORTS_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/OpenCLExports${IGDRCL_OPTION__BITS}.def")
  set(MSVC_DEF_LIB_NAME "igdrcl${IGDRCL_OPTION__BITS}")
  set(MSVC_DEF_HEADER "This file was generated during CMake project configuration - please don't edit")
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLReleaseExports.def.in" "${EXPORTS_FILENAME}")
  else()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLInternalExports.def.in" "${EXPORTS_FILENAME}")
  endif()
elseif(UNIX)
  set(EXPORTS_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/ocl.exports")
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl_release.exports" "${EXPORTS_FILENAME}" COPYONLY)
  else()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl_internal.exports" "${EXPORTS_FILENAME}" COPYONLY)
  endif()
endif()

if(${GENERATE_EXECUTABLE})
  list(APPEND NEO_DYNAMIC_LIB__TARGET_OBJECTS
       $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
       $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
       $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
       $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
       $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
  )

  add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
              ${NEO_DYNAMIC_LIB__TARGET_OBJECTS}
  )
  if(DEFINED AUB_STREAM_PROJECT_NAME)
    target_sources(${NEO_DYNAMIC_LIB_NAME}
                   PRIVATE
                   $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>
                   ${NEO_SOURCE_DIR}/shared/source/aub/aub_stream_interface.cpp
    )
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${AUB_STREAM_PROJECT_NAME})
  else()
    target_sources(${NEO_DYNAMIC_LIB_NAME}
                   PRIVATE
                   $<TARGET_OBJECTS:aub_stream_stubs>
    )
  endif()

  if(UNIX)
    if(NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
      get_filename_component(lib_file_name $<TARGET_FILE:${NEO_DYNAMIC_LIB_NAME}> NAME_WE)
      set(symbols_file_name ${lib_file_name}.debug)
      set(debug_symbols_target_name "${STRIP_SYMBOLS_TARGET}_${NEO_DYNAMIC_LIB_NAME}")
      add_custom_target(${debug_symbols_target_name}
                        COMMAND sh -c "objcopy --only-keep-debug ${lib_file_name} ${symbols_file_name}"
                        COMMAND sh -c "strip -g ${lib_file_name}"
                        COMMAND sh -c "objcopy --add-gnu-debuglink=${symbols_file_name} ${lib_file_name}"
      )
      add_dependencies(${debug_symbols_target_name} ${NEO_DYNAMIC_LIB_NAME})
      add_dependencies(${STRIP_SYMBOLS_TARGET} ${debug_symbols_target_name})
      set_property(GLOBAL PROPERTY IGDRCL_SYMBOL_FILE "${symbols_file_name}")
    endif()

    install(FILES
            $<TARGET_FILE:${NEO_DYNAMIC_LIB_NAME}>
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/intel-opencl
            COMPONENT opencl
    )
    set_property(GLOBAL APPEND PROPERTY NEO_OCL_COMPONENTS_LIST "opencl")
  endif()

  if(NOT DISABLED_GTPIN_SUPPORT)
    macro(macro_for_each_core_type)
      foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/gtpin_setup_${CORE_TYPE_LOWER}.cpp)
          target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR}${CORE_TYPE_LOWER}/gtpin_setup_${CORE_TYPE_LOWER}.cpp)
        endif()
      endforeach()
    endmacro()
    apply_macro_for_each_core_type("SUPPORTED")
  endif()

  add_subdirectory(dll)

  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${NEO_STATIC_LIB_NAME} ${NEO_SHARED_LIB} ${NEO_STATIC_LIB_NAME} ${NEO_SHARED_LIB} ${NEO_EXTRA_LIBS})

  target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
                             ${CMAKE_CURRENT_BINARY_DIR}
  )

  if(WIN32)
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} PRIVATE dxgi)
    add_dependencies(${NEO_DYNAMIC_LIB_NAME} ${GMM_TARGET_NAME})
    target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
    )
  else()
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${GMM_LINK_NAME})
    target_include_directories(${NEO_DYNAMIC_LIB_NAME} PRIVATE
                               ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}
    )
    if(NOT ENABLE_DYNAMIC_MEMORY_TRACKING)
      set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
                   APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${EXPORTS_FILENAME}"
      )
    endif()
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
                 APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bsymbolic"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
    target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE
                   ${NEO_SHARED_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
    )
  endif()

  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
                        DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        RELEASEINTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
  )
  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER ${OPENCL_RUNTIME_PROJECTS_FOLDER})
  create_project_source_tree_with_exports(${NEO_DYNAMIC_LIB_NAME} "${EXPORTS_FILENAME}")
endif()

create_project_source_tree(${NEO_STATIC_LIB_NAME})

if(UNIX AND NOT (TARGET clang-tidy))
  add_custom_target(clang-tidy)
  add_custom_command(
                     TARGET clang-tidy
                     POST_BUILD
                     COMMAND echo clang-tidy...
                     COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp | xargs --verbose -I{} -P`nproc` clang-tidy-8 -p ${NEO_BINARY_DIR} {} | tee ${NEO_BINARY_DIR}/clang-tidy.log
                     WORKING_DIRECTORY ${NEO_SOURCE_DIR}
  )
endif()
