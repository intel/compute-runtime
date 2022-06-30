#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  # get WDK location and version to use
  if(NOT WDK_DIR)
    if(IS_DIRECTORY "${NEO_SOURCE_DIR}/../wdk")
      get_filename_component(WDK_DIR "../wdk" ABSOLUTE)
    endif()
  endif()
  if(WDK_DIR)
    if(IS_DIRECTORY "${WDK_DIR}/Win15")
      get_filename_component(WDK_DIR "${WDK_DIR}/Win15" ABSOLUTE)
    endif()
  endif()
  message(STATUS "WDK Directory: ${WDK_DIR}")

  if(NOT WDK_VERSION)
    if(WDK_DIR)
      # Get WDK version from ${WDK_DIR}/WDKVersion.txt
      file(READ "${WDK_DIR}/WDKVersion.txt" WindowsTargetPlatformVersion)
      string(REPLACE " " ";" WindowsTargetPlatformVersion ${WindowsTargetPlatformVersion})
      list(LENGTH WindowsTargetPlatformVersion versionListLength)
      if(NOT versionListLength EQUAL 3)
        if(WIN32)
          message(ERROR "Error reading content of WDKVersion.txt file")
        endif()
      else()
        list(GET WindowsTargetPlatformVersion 2 WindowsTargetPlatformVersion)
      endif()
    else()
      if(WIN32)
        message(ERROR "WDK not available")
      endif()
    endif()
  else()
    set(WindowsTargetPlatformVersion ${WDK_VERSION})
  endif()
  message(STATUS "WDK Version is ${WindowsTargetPlatformVersion}")
endif()

if(NOT DISABLE_WDDM_LINUX)
  get_filename_component(LIBDXG_PATH "${NEO_SOURCE_DIR}/third_party/libdxg" ABSOLUTE)
  set(D3DKMTHK_INCLUDE_PATHS "${LIBDXG_PATH}/include/")
endif()

if(WIN32)
  if(${WindowsTargetPlatformVersion} VERSION_LESS "10.0.18328.0")
    set(CONST_FROM_WDK_10_0_18328_0)
  else()
    set(CONST_FROM_WDK_10_0_18328_0 "CONST")
  endif()
  add_compile_options(-DCONST_FROM_WDK_10_0_18328_0=${CONST_FROM_WDK_10_0_18328_0})
  set(WDK_INCLUDE_PATHS "")
  list(APPEND WDK_INCLUDE_PATHS
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/um"
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/shared"
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/km"
  )
  message(STATUS "WDK include paths: ${WDK_INCLUDE_PATHS}")
elseif(NOT DISABLE_WDDM_LINUX)
  add_compile_options(-DCONST_FROM_WDK_10_0_18328_0=CONST)
  add_compile_options(-DWDDM_LINUX=1)
  set(WDK_INCLUDE_PATHS "")
  get_filename_component(DX_HEADERS_PATH "${NEO_SOURCE_DIR}/third_party/DirectX-Headers" ABSOLUTE)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/wsl)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/wsl/stubs)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/directx)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/dxguids)
  list(APPEND WDK_INCLUDE_PATHS ${D3DKMTHK_INCLUDE_PATHS})
  message(STATUS "WDK include paths: ${WDK_INCLUDE_PATHS}")
endif()

if(WIN32)
  link_directories("${WDK_DIR}/Lib/${WindowsTargetPlatformVersion}/um/${NEO_ARCH}/")
endif()
