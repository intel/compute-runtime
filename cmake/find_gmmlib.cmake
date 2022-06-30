#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# GmmLib detection
if(TARGET igfx_gmmumd_dll)
  set(GMM_TARGET_NAME "igfx_gmmumd_dll")
  set(GMM_LINK_NAME $<TARGET_PROPERTY:${GMM_TARGET_NAME},OUTPUT_NAME>)
  set(NEO__GMM_LIBRARY_PATH $<TARGET_FILE_DIR:${GMM_TARGET_NAME}>)
  set(NEO__GMM_INCLUDE_DIR $<TARGET_PROPERTY:${GMM_TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
else()
  if(DEFINED GMM_DIR)
    get_filename_component(GMM_DIR "${GMM_DIR}" ABSOLUTE)
  else()
    get_filename_component(GMM_DIR_tmp "${NEO_SOURCE_DIR}/../gmmlib" ABSOLUTE)
    if(IS_DIRECTORY "${GMM_DIR_tmp}")
      set(GMM_DIR "${GMM_DIR_tmp}")
    endif()
  endif()

  if(UNIX)
    if(DEFINED GMM_DIR)
      if(IS_DIRECTORY "${GMM_DIR}/lib/pkgconfig/")
        set(__tmp_LIBDIR "lib")
      elseif(IS_DIRECTORY "${GMM_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
        set(__tmp_LIBDIR ${CMAKE_INSTALL_LIBDIR})
      endif()
    endif()

    find_package(PkgConfig)
    if(DEFINED __tmp_LIBDIR)
      set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
      set(ENV{PKG_CONFIG_PATH} "${GMM_DIR}/${__tmp_LIBDIR}/pkgconfig/")
    endif()
    pkg_check_modules(NEO__GMM igdgmm)
    if(DEFINED __tmp_LIBDIR)
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
    endif()

    if(NEO__GMM_FOUND)
      if(DEFINED __tmp_LIBDIR)
        string(REPLACE "${NEO__GMM_INCLUDEDIR}" "${GMM_DIR}/include/igdgmm" NEO__GMM_INCLUDE_DIRS "${NEO__GMM_INCLUDE_DIRS}")
        string(REPLACE "${NEO__GMM_LIBDIR}" "${GMM_DIR}/${__tmp_LIBDIR}" NEO__GMM_LIBDIR "${NEO__GMM_LIBDIR}")
        set(NEO__GMM_LIBRARY_PATH "${NEO__GMM_LIBDIR}")
      endif()

      set(GMM_TARGET_NAME "igfx_gmmumd_dll")
      set(GMM_LINK_NAME ${NEO__GMM_LIBRARIES})

      set(NEO__GMM_INCLUDE_DIR ${NEO__GMM_INCLUDE_DIRS})
      message(STATUS "GmmLib include dirs: ${NEO__GMM_INCLUDE_DIR}")
    else()
      message(FATAL_ERROR "GmmLib not found!")
    endif()
    if(DEFINED __tmp_LIBDIR)
      unset(__tmp_LIBDIR)
    endif()
  else()
    if(EXISTS "${GMM_DIR}/CMakeLists.txt")
      message(STATUS "GmmLib source dir is: ${GMM_DIR}")
      add_subdirectory_unique("${GMM_DIR}" "${NEO_BUILD_DIR}/gmmlib")

      if(NOT DEFINED GMM_TARGET_NAME)
        set(GMM_TARGET_NAME "igfx_gmmumd_dll")
      endif()

      set(NEO__GMM_INCLUDE_DIR $<TARGET_PROPERTY:${GMM_TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
      set(NEO__GMM_LIBRARY_PATH $<TARGET_FILE_DIR:${GMM_TARGET_NAME}>)
      set(GMM_LINK_NAME $<TARGET_PROPERTY:${GMM_TARGET_NAME},OUTPUT_NAME>)
    else()
      message(FATAL_ERROR "GmmLib not found!")
    endif()
  endif()
endif()

macro(copy_gmm_dll_for target)
  if(NOT UNIX)
    add_custom_command(
                       TARGET ${target}
                       POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${GMM_TARGET_NAME}> $<TARGET_FILE_DIR:${target}>
    )
  endif()
endmacro()

link_directories(${NEO__GMM_LIBRARY_PATH})

add_definitions(-DGMM_OCL)
