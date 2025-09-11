#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(BUILD_WITH_L0)
  set(TARGET_NAME_L0 ze_intel_gpu)

  # Level Zero third party detection
  if(DEFINED LEVEL_ZERO_ROOT)
    get_filename_component(LEVEL_ZERO_ROOT "${LEVEL_ZERO_ROOT}" ABSOLUTE)
  else()
    get_filename_component(LEVEL_ZERO_ROOT_tmp "${NEO_SOURCE_DIR}/../level_zero" ABSOLUTE)
    # Level Zero Headers have all includes in include folder and its subdirectories,
    # while devel/sdk package has all the includes in include/level_zero subdirectory.
    if(IS_DIRECTORY "${LEVEL_ZERO_ROOT_tmp}")
      set(CUSTOM_L0_INCLUDE_PATH "${LEVEL_ZERO_ROOT_tmp}/include/level_zero/")
      file(GLOB LEVEL_ZERO_SOURCE_HEADERS
           "${LEVEL_ZERO_ROOT_tmp}/include/*.h"
      )
      if(IS_DIRECTORY "${LEVEL_ZERO_ROOT_tmp}/include/loader")
        list(APPEND LEVEL_ZERO_SOURCE_HEADERS ${LEVEL_ZERO_ROOT_tmp}/include/loader)
      endif()
      file(MAKE_DIRECTORY ${CUSTOM_L0_INCLUDE_PATH})
      file(COPY ${LEVEL_ZERO_SOURCE_HEADERS} DESTINATION ${CUSTOM_L0_INCLUDE_PATH})
      set(LEVEL_ZERO_ROOT "${LEVEL_ZERO_ROOT_tmp}")
    endif()
  endif()

  if(NOT DEFINED DONT_USE_PREBUILT_KERNELS_L0)
    set(DONT_USE_PREBUILT_KERNELS_L0 FALSE)
  endif()

  project(level-zero-gpu VERSION ${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_VERSION_BUILD})
  message(STATUS "Level Zero driver version: ${PROJECT_VERSION}")

  include(cmake/source_tree.cmake)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(LevelZero)
  if(NOT LevelZero_FOUND)
    message(STATUS "Level zero headers not found")
    message(STATUS "Skipping level zero")
    set(BUILD_WITH_L0 FALSE PARENT_SCOPE)
    return()
  endif()

  add_definitions(-DZE_ENABLE_OCL_INTEROP=1)

  file(WRITE "${CMAKE_BINARY_DIR}/VERSION" "${PROJECT_VERSION}")

  #Define a path for custom commands to work around MSVC
  set(CUSTOM_COMMAND_BINARY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  if(MSVC)
    #MSVC implicitly adds $<CONFIG> to the output path
    if(NOT "${CMAKE_GENERATOR}" STREQUAL "Ninja")
      set(CUSTOM_COMMAND_BINARY_DIR ${CUSTOM_COMMAND_BINARY_DIR}/$<CONFIG>)
    endif()
  endif()

  if(UNIX)
    # Netlink and Generic Netlink
    find_path(LIBGENL_INCLUDE_DIR netlink/genl/genl.h PATH_SUFFIXES libnl3)
    if(LIBGENL_INCLUDE_DIR)
      message(STATUS "LibGenl headers directory: ${LIBGENL_INCLUDE_DIR}")
      include_directories(SYSTEM ${LIBGENL_INCLUDE_DIR})
      set(LIBGENL_FOUND TRUE)
    else()
      message(STATUS "LibGenl headers not available. Building without")
    endif()
  endif()

  if(UNIX)
    # Udev library interface
    find_path(LIBUDEV_INCLUDE_DIR libudev.h)
    if(LIBUDEV_INCLUDE_DIR)
      message(STATUS "libudev headers directory: ${LIBUDEV_INCLUDE_DIR}")
      include_directories(SYSTEM ${LIBUDEV_INCLUDE_DIR})
      set(LIBUDEV_FOUND TRUE)
    else()
      message(STATUS "libudev headers not available. Building without")
    endif()
  endif()

  # Firmware Update Library
  get_filename_component(IGSC_DIR_tmp "${NEO_SOURCE_DIR}/../igsc" ABSOLUTE)
  if(EXISTS "${IGSC_DIR_tmp}/lib/cmake")
    find_package(igsc HINTS "${IGSC_DIR_tmp}/lib/cmake")
  else()
    find_package(igsc)
  endif()
  if(igsc_FOUND)
    add_definitions(-DIGSC_PRESENT=1)
    if(EXISTS "${IGSC_DIR_tmp}/lib/cmake")
      get_filename_component(igsc_INCLUDE_DIR "${NEO_SOURCE_DIR}/../igsc/include" ABSOLUTE)
    endif()
    message(STATUS "igsc Library headers directory: ${igsc_INCLUDE_DIR}")
    message(STATUS "igsc version: ${igsc_VERSION}")
    include_directories(SYSTEM ${igsc_INCLUDE_DIR})
  else()
    message(STATUS "igsc Library headers not available. Building without")
  endif()

  if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
  endif()

  if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  endif()

  set(L0_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT DEFINED COMPUTE_RUNTIME_DIR)
    get_filename_component(COMPUTE_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
  endif()
  # Option to disable tests
  option(${PROJECT_NAME}_BUILD_TESTS "Build unit tests." ON)

  # Copy third_party_binaries to output BIN folder
  add_custom_target(copy_third_party_files)
  set_target_properties(copy_third_party_files PROPERTIES FOLDER ${TARGET_NAME_L0})

  if(DEFINED NEO__IGC_TARGETS)
    if(WIN32)
      add_dependencies(copy_third_party_files copy_compiler_files)
    else()
      add_dependencies(copy_third_party_files ${NEO__IGC_TARGETS})
      foreach(TARGET_tmp ${NEO__IGC_TARGETS})
        if(UNIX)
          add_custom_command(
                             TARGET copy_third_party_files
                             PRE_BUILD
                             COMMAND echo Running copy_third_party_files for igc targets. Copy from: $<TARGET_SONAME_FILE:${TARGET_tmp}> to: "${CUSTOM_COMMAND_BINARY_DIR}/"
                             COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                             COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_SONAME_FILE:${TARGET_tmp}> "${CUSTOM_COMMAND_BINARY_DIR}/"
          )
        endif()
        add_custom_command(
                           TARGET copy_third_party_files
                           PRE_BUILD
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${TARGET_tmp}> "${CUSTOM_COMMAND_BINARY_DIR}/"
        )
      endforeach()
    endif()
  else()
    if(DEFINED IGC_DIR) # Only copy igc libs if available
      message(STATUS "L0::Igc Dir: ${IGC_DIR}")
      set(TEMP_IGC_DIRS "${NEO__IGC_LIBRARY_PATH}")
      string(REPLACE ":" ";" TEMP_IGC_DIRS "${NEO__IGC_LIBRARY_PATH}")
      foreach(TEMP_DIR ${TEMP_IGC_DIRS})
        add_custom_command(
                           TARGET copy_third_party_files
                           PRE_BUILD
                           COMMAND echo Running copy_third_party_files for igc dir. Copy from: "${TEMP_DIR}" to: "${CUSTOM_COMMAND_BINARY_DIR}/"
                           COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                           COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}" "${CUSTOM_COMMAND_BINARY_DIR}/"
        )
      endforeach()
    endif()
  endif()

  if(TARGET ${GMM_TARGET_NAME})
    message(STATUS "L0::Gmm Target: ${GMM_TARGET_NAME}")
    add_dependencies(copy_third_party_files ${GMM_TARGET_NAME})
    if(UNIX)
      add_custom_command(
                         TARGET copy_third_party_files
                         PRE_BUILD
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_SONAME_FILE:${GMM_TARGET_NAME}>" "${CUSTOM_COMMAND_BINARY_DIR}/"
      )
    endif()
    add_custom_command(
                       TARGET copy_third_party_files
                       PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${GMM_TARGET_NAME}>" "${CUSTOM_COMMAND_BINARY_DIR}/"
    )
  else()
    if(DEFINED GMM_DIR) # Only copy gmm libs if available
      message(STATUS "L0::Gmm Dir: ${GMM_DIR}")
      add_custom_command(
                         TARGET copy_third_party_files
                         PRE_BUILD
                         COMMAND echo Running copy_third_party_files for gmm dir. Copy from: "${NEO__GMM_LIBRARY_PATH}" to: "${CUSTOM_COMMAND_BINARY_DIR}/"
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${CUSTOM_COMMAND_BINARY_DIR}
                         COMMAND ${CMAKE_COMMAND} -E copy_directory "${NEO__GMM_LIBRARY_PATH}" "${CUSTOM_COMMAND_BINARY_DIR}/"
      )
    endif()
  endif()

  # Get build type
  string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)

  if("${BUILD_TYPE}" STREQUAL "debug")
    add_definitions(-DZE_DEBUG)
  endif()

  include_directories(${COMPUTE_RUNTIME_DIR}/third_party/opencl_headers)

  if(CUSTOM_L0_INCLUDE_PATH)
    include_directories(${CUSTOM_L0_INCLUDE_PATH}/../)
  else()
    include_directories(${LevelZero_INCLUDE_DIRS})
  endif()

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdlist${BRANCH_DIR_SUFFIX})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdlist/cmdlist_extended${BRANCH_DIR_SUFFIX})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/cmdlist/definitions${BRANCH_DIR_SUFFIX})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/source/driver/definitions${BRANCH_DIR_SUFFIX})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/experimental/source/graph/captured_apis${BRANCH_DIR_SUFFIX})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/experimental/source/graph/definitions${BRANCH_DIR_SUFFIX})

  # Create our shared library/DLL
  configure_file(ze_intel_gpu_version.h.in ${NEO_BUILD_DIR}/ze_intel_gpu_version.h)
  set(L0_SHARED_LIB_DLLS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${NEO_SHARED_DIRECTORY}/gmm_helper/page_table_mngr.cpp
      ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info.cpp
      ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
      ${NEO_SHARED_DIRECTORY}/helpers/debug_helpers.cpp
      ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
      ${NEO_SHARED_DIRECTORY}/utilities/cpuintrinsics.cpp
      ${NEO_SHARED_DIRECTORY}/utilities/debug_settings_reader_creator.cpp
      ${NEO_SHARED_DIRECTORY}/utilities/io_functions.cpp
      ${NEO_SHARED_DIRECTORY}/built_ins/sip_init.cpp
      $<TARGET_OBJECTS:neo_shared_dlls_precompiled_objects>
  )

  add_library(${TARGET_NAME_L0} SHARED ${L0_SHARED_LIB_DLLS})
  if(DEFINED AUB_STREAM_PROJECT_NAME)
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${NEO_SHARED_DIRECTORY}/aub/aub_stream_interface.cpp
    )
    target_link_libraries(${TARGET_NAME_L0} PRIVATE ${AUB_STREAM_PROJECT_NAME})
  else()
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   $<TARGET_OBJECTS:aub_stream_stubs>
    )

  endif()

  if(WIN32)
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${NEO_SHARED_DIRECTORY}/dll/wddm/create_gdi.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/windows/debugger_l0_windows.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/windows${BRANCH_DIR_SUFFIX}/options_windows.cpp
                   ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory_base.cpp
                   ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/sys_calls.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/sys_calls_winmm.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_interface_win.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_memory_virtual_alloc.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/wddm_calls.cpp
    )
    target_link_libraries(${TARGET_NAME_L0} PRIVATE
                          dxgi
    )
    if(DEFINED L0_DLL_RC_FILE)
      message(STATUS "Setting L0 Resource Info")
      target_sources(${TARGET_NAME_L0} PRIVATE ${L0_DLL_RC_FILE})
    endif()
    add_dependencies(${TARGET_NAME_L0} ${GMM_TARGET_NAME})
  else()
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX}devices.inl
                   ${NEO_SHARED_DIRECTORY}/dll/devices/devices_base.inl
                   ${NEO_SHARED_DIRECTORY}/dll/linux/debugger_l0_dll_linux.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/linux/drm_neo_create.cpp
                   ${NEO_SHARED_DIRECTORY}/dll/linux${BRANCH_DIR_SUFFIX}/options_linux.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
    )
    if(NOT DISABLE_WDDM_LINUX)
      target_sources(${TARGET_NAME_L0}
                     PRIVATE
                     ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
                     ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp
                     ${NEO_SHARED_DIRECTORY}/dll/wddm/create_gdi.cpp
      )
    endif()
  endif()

  set_target_properties(${TARGET_NAME_L0} PROPERTIES
                        DEBUG_OUTPUT_NAME "${TARGET_NAME_L0}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        RELEASE_OUTPUT_NAME "${TARGET_NAME_L0}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        RELEASEINTERNAL_OUTPUT_NAME "${TARGET_NAME_L0}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
                        OUTPUT_NAME "${TARGET_NAME_L0}${IGDRCL_OPTION__BITS}${NEO__SO_NAME_SUFFIX}"
  )

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   $<TARGET_OBJECTS:aub_stream_all_hw>
    )
  endif()
  if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   $<TARGET_OBJECTS:${BUILTINS_SPIRV_LIB_NAME}>
    )
  endif()
  if(NOT ${DONT_USE_PREBUILT_KERNELS_L0})
    message(STATUS "Prebuilt kernels are linked to Level Zero.")
    target_sources(${TARGET_NAME_L0}
                   PRIVATE
                   $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
                   $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
                   $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
                   $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
    )
  endif()

  target_compile_definitions(${TARGET_NAME_L0}
                             PUBLIC
                             ZE_MAKEDLL
  )

  get_property(COMPUTE_RUNTIME_DEFINITIONS
               TARGET ${NEO_SHARED_RELEASE_LIB_NAME}
               PROPERTY COMPILE_DEFINITIONS
  )

  target_compile_definitions(${TARGET_NAME_L0}
                             PRIVATE
                             ${COMPUTE_RUNTIME_DEFINITIONS}
  )

  hide_subdir(api)
  hide_subdir(ddi)
  hide_subdir(experimental)
  hide_subdir(include)
  set(L0_RELEASE_LIB_NAME "${TARGET_NAME_L0}_lib")
  if(NOT NEO_SKIP_L0_UNIT_TESTS)
    if(DONT_CARE_OF_VIRTUALS)
      set(L0_MOCKABLE_LIB_NAME "${TARGET_NAME_L0}_lib")
    else()
      set(L0_MOCKABLE_LIB_NAME "${TARGET_NAME_L0}_mockable")
    endif()
  endif()

  function(generate_l0_lib LIB_NAME MOCKABLE)
    set(L0_STATIC_LIB_NAME ${LIB_NAME})
    add_library(${LIB_NAME} OBJECT
                ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    )
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/api "${NEO_BUILD_DIR}/${LIB_NAME}/api")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ddi "${NEO_BUILD_DIR}/${LIB_NAME}/ddi")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/source "${NEO_BUILD_DIR}/${LIB_NAME}/core/source")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/experimental "${NEO_BUILD_DIR}/${LIB_NAME}/experimental")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/include "${NEO_BUILD_DIR}/${LIB_NAME}/include")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/source "${NEO_BUILD_DIR}/${LIB_NAME}tools/source")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sysman/source "${NEO_BUILD_DIR}/${LIB_NAME}sysman/source")

    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/core/source/driver/driver_handle_imp.cpp
                                PROPERTIES
                                COMPILE_DEFINITIONS
                                "NEO_L0_VERSION_MAJOR=${NEO_L0_VERSION_MAJOR};NEO_L0_VERSION_MINOR=${NEO_L0_VERSION_MINOR}"
    )

    if(${MOCKABLE})
      get_property(COMPUTE_RUNTIME_DEFINITIONS
                   TARGET ${NEO_SHARED_MOCKABLE_LIB_NAME}
                   PROPERTY COMPILE_DEFINITIONS
      )
      target_compile_definitions(${LIB_NAME}
                                 PUBLIC MOCKABLE_VIRTUAL=virtual
                                 PUBLIC
                                 ${COMPUTE_RUNTIME_DEFINITIONS}
      )
    else()
      get_property(COMPUTE_RUNTIME_DEFINITIONS
                   TARGET ${NEO_SHARED_RELEASE_LIB_NAME}
                   PROPERTY COMPILE_DEFINITIONS
      )
      target_compile_definitions(${LIB_NAME}
                                 PUBLIC MOCKABLE_VIRTUAL=
                                 PUBLIC
                                 ${COMPUTE_RUNTIME_DEFINITIONS}
      )
    endif()

    set_property(TARGET ${LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
    set_target_properties(${LIB_NAME} PROPERTIES FOLDER ${TARGET_NAME_L0})

    target_include_directories(${LIB_NAME} PUBLIC
                               ${ENGINE_NODE_DIR}
                               ${NEO__GMM_INCLUDE_DIR}
                               ${CIF_BASE_DIR}
                               ${IGC_OCL_ADAPTOR_DIR}
                               ${NEO__IGC_INCLUDE_DIR}
                               ${KHRONOS_HEADERS_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(WIN32 OR NOT DISABLE_WDDM_LINUX)
      target_include_directories(${LIB_NAME} PUBLIC ${WDK_INCLUDE_PATHS})
    endif()

    if(WIN32)
      target_include_directories(${LIB_NAME} PUBLIC
                                 ${NEO_SHARED_DIRECTORY}/os_interface/windows
      )
    else()
      target_include_directories(${LIB_NAME} PUBLIC
                                 ${NEO_SHARED_DIRECTORY}/os_interface/linux
      )
    endif()

    create_project_source_tree(${LIB_NAME})
  endfunction()

  if(UNIX)
    option(L0_INSTALL_UDEV_RULES "Install udev rules. An attempt to automatically determine the proper location will be made if UDEV_RULES_DIR is not set." OFF)
    if(L0_INSTALL_UDEV_RULES)
      if(DEFINED UDEV_RULES_DIR)
        set(UDEV_RULES_DIR_FOUND TRUE)
      else()
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/UdevRulesDir.cmake)
      endif()
      if(NOT UDEV_RULES_DIR_FOUND)
        message(SEND_ERROR "udev rule install requested but no rules directory found")
      endif()
    endif()
  endif()

  if(DONT_CARE_OF_VIRTUALS)
    generate_l0_lib(${L0_RELEASE_LIB_NAME} TRUE)
  else()
    generate_l0_lib(${L0_RELEASE_LIB_NAME} FALSE)
    if(NOT NEO_SKIP_L0_UNIT_TESTS)
      generate_l0_lib(${L0_MOCKABLE_LIB_NAME} TRUE)
    endif()
  endif()

  append_sources_from_properties(L0_SHARED_LIB_SRCS L0_SRCS_DLL NEO_CORE_SRCS_LINK NEO_SRCS_ENABLE_CORE)
  target_sources(${TARGET_NAME_L0} PRIVATE $<TARGET_OBJECTS:${L0_RELEASE_LIB_NAME}> ${L0_SHARED_LIB_SRCS})

  target_link_libraries(${TARGET_NAME_L0} PRIVATE
                        ${NEO_SHARED_RELEASE_LIB_NAME}
                        ${NEO_EXTRA_LIBS} ${ASAN_LIBS} ${TSAN_LIBS}
  )

  if(UNIX)
    target_link_libraries(${TARGET_NAME_L0} PRIVATE ${GMM_LINK_NAME})
    if(NOT ENABLE_DYNAMIC_MEMORY_TRACKING)
      set_property(TARGET ${TARGET_NAME_L0}
                   APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/core/source/dll/linux/ze.exports"
      )
    endif()
  endif()

  create_source_tree(${TARGET_NAME_L0} ${L0_ROOT_DIR}/..)

  set_property(TARGET ${TARGET_NAME_L0} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

  set_target_properties(${TARGET_NAME_L0} PROPERTIES
                        FOLDER ${TARGET_NAME_L0}
                        VERSION "${PROJECT_VERSION}"
                        SOVERSION "${PROJECT_VERSION_MAJOR}"
  )

  if(UNIX)
    if(NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
      get_filename_component(lib_file_name $<TARGET_FILE:${TARGET_NAME_L0}> NAME_WE)
      set(symbols_file_name ${lib_file_name}.debug)
      set(debug_symbols_target_name "${STRIP_SYMBOLS_TARGET}_${TARGET_NAME_L0}")
      add_custom_target(${debug_symbols_target_name}
                        COMMAND sh -c "objcopy --only-keep-debug ${lib_file_name} ${symbols_file_name}"
                        COMMAND sh -c "strip -g ${lib_file_name}"
                        COMMAND sh -c "objcopy --add-gnu-debuglink=${symbols_file_name} ${lib_file_name}"
      )
      add_dependencies(${debug_symbols_target_name} ${TARGET_NAME_L0})
      add_dependencies(${STRIP_SYMBOLS_TARGET} ${debug_symbols_target_name})
      set_property(GLOBAL APPEND PROPERTY DEBUG_SYMBOL_FILES "${symbols_file_name}")
    endif()

    install(TARGETS ${TARGET_NAME_L0}
            LIBRARY
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${PROJECT_NAME}
            NAMELINK_SKIP
    )
  else()
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
            DESTINATION Release/lh64
            CONFIGURATIONS Release
    )
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
            DESTINATION Release-Internal/lh64
            CONFIGURATIONS ReleaseInternal
    )
    install(TARGETS ${TARGET_NAME_L0} RUNTIME
            DESTINATION Debug/lh64
            CONFIGURATIONS Debug
    )
  endif()
  if(NOT NEO_SKIP_L0_UNIT_TESTS)
    add_subdirectory_unique(core/test/common)
    add_subdirectory_unique(core/test/unit_tests)
    add_subdirectory_unique(core/test/aub_tests)
    add_subdirectory_unique(tools/test/unit_tests)
    add_subdirectory_unique(sysman/test/unit_tests)

  else()
    hide_subdir(core/test/common)
    hide_subdir(core/test/unit_tests)
    hide_subdir(core/test/aub_tests)
    hide_subdir(tools/test/unit_tests)
    hide_subdir(sysman/test/unit_tests)
    hide_subdir(experimental/test/unit_tests)
  endif()
  if(NOT NEO_SKIP_L0_BLACK_BOX_TESTS)
    add_subdirectory_unique(core/test/black_box_tests)
    add_subdirectory_unique(tools/test/black_box_tests)
  else()
    hide_subdir(core/test/black_box_tests)
    hide_subdir(tools/test/black_box_tests)
  endif()
  add_subdirectories()

  if(UNIX AND NEO_BUILD_L0_PACKAGE)
    message(STATUS "Building LevelZero package")

    set_property(GLOBAL APPEND PROPERTY NEO_L0_COMPONENTS_LIST ${PROJECT_NAME})

    set(L0_PACKAGE_VERSION_DEB "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
    set(L0_PACKAGE_VERSION_RPM "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/cpack.cmake)
      include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${BRANCH_TYPE}/cpack.cmake)
    endif()

  endif()

else()
  message(STATUS "Skipping level zero")
  set(BUILD_WITH_L0 FALSE PARENT_SCOPE)
endif()
