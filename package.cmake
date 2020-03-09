#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(package_input_dir ${NEO_BINARY_DIR}/packageinput)
  set(package_output_dir ${NEO_BINARY_DIR}/packages)

  if(NOT DEFINED NEO_VERSION_MAJOR)
    set(NEO_VERSION_MAJOR 1)
  endif()
  if(NOT DEFINED NEO_VERSION_MINOR)
    set(NEO_VERSION_MINOR 0)
  endif()
  if(NOT DEFINED NEO_VERSION_BUILD)
    set(NEO_VERSION_BUILD 0)
  endif()

  include("os_release_info.cmake")

  get_os_release_info(os_name os_version os_codename)

  if("${os_name}" STREQUAL "clear-linux-os")
    # clear-linux-os distribution avoids /etc for distribution defaults.
    set(_dir_etc "/usr/share/defaults/etc")
  else()
    set(_dir_etc "/etc")
  endif()

  set(OCL_ICD_RUNTIME_NAME ${CMAKE_SHARED_LIBRARY_PREFIX}${NEO_DLL_NAME_BASE}${CMAKE_SHARED_LIBRARY_SUFFIX})
  install(
    CODE "file( WRITE ${NEO_BINARY_DIR}/intel.icd \"${CMAKE_INSTALL_FULL_LIBDIR}/intel-opencl/${OCL_ICD_RUNTIME_NAME}\n\" )"
    CODE "file( WRITE ${NEO_BINARY_DIR}/tmp/postinst \"/sbin/ldconfig\n\" )"
    CODE "file( WRITE ${NEO_BINARY_DIR}/tmp/postrm \"/sbin/ldconfig\n\" )"
    CODE "file( COPY ${NEO_BINARY_DIR}/tmp/postinst DESTINATION ${NEO_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )"
    CODE "file( COPY ${NEO_BINARY_DIR}/tmp/postrm DESTINATION ${NEO_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )"
    COMPONENT opencl
  )
  install(FILES ${NEO_BINARY_DIR}/intel.icd DESTINATION ${_dir_etc}/OpenCL/vendors/ COMPONENT opencl)

  if(NEO_CPACK_GENERATOR)
    set(CPACK_GENERATOR "${NEO_CPACK_GENERATOR}")
  else()
    # If generators list was not define build native package for current distro
    if(EXISTS "/etc/debian_version")
      set(CPACK_GENERATOR "DEB")
    elseif(EXISTS "/etc/redhat-release")
      set(CPACK_GENERATOR "RPM")
    else()
      set(CPACK_GENERATOR "TXZ")
    endif()
  endif()

  set(CPACK_SET_DESTDIR TRUE)
  set(CPACK_PACKAGE_ARCHITECTURE "x86_64")
  set(CPACK_PACKAGE_RELOCATABLE FALSE)
  set(CPACK_PACKAGE_NAME "intel")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intel(R) Graphics Compute Runtime")
  set(CPACK_PACKAGE_VENDOR "Intel")
  set(CPACK_PACKAGE_VERSION_MAJOR ${NEO_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${NEO_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${NEO_VERSION_BUILD})
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "postinst;postrm")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://01.org/compute-runtime")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_RPM_COMPRESSION_TYPE "xz")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
  set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
  set(CPACK_RPM_PACKAGE_DESCRIPTION "Intel OpenCL GPU driver")
  set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_RELEASE 1)
  set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
  set(CPACK_RPM_PACKAGE_URL "http://01.org/compute-runtime")
  set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${NEO_BINARY_DIR}/postinst")
  set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${NEO_BINARY_DIR}/postrm")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CMAKE_INSTALL_PREFIX})
  set(CPACK_PACKAGE_CONTACT "Intel Corporation")
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
  get_property(CPACK_COMPONENTS_ALL GLOBAL PROPERTY NEO_COMPONENTS_LIST)
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /etc/ld.so.conf.d
    /usr/local
    /usr/local/lib64
    /usr/local/bin
  )

  if(CMAKE_VERSION VERSION_GREATER 3.6 OR CMAKE_VERSION VERSION_EQUAL 3.6)
    set(CPACK_DEBIAN_OPENCL_FILE_NAME "intel-opencl_${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_OCLOC_FILE_NAME "intel-opencl-ocloc_${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_LEVEL-ZERO-GPU_FILE_NAME  "intel-level-zero-gpu_${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

    set(CPACK_RPM_OPENCL_FILE_NAME "intel-opencl-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    set(CPACK_RPM_OCLOC_FILE_NAME "intel-opencl-ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    set(CPACK_RPM_LEVEL-ZERO-GPU_FILE_NAME "intel-level-zero-gpu-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")

    set(CPACK_ARCHIVE_OPENCL_FILE_NAME "intel-opencl-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
    set(CPACK_ARCHIVE_OCLOC_FILE_NAME "intel-opencl-ocloc-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
    set(CPACK_ARCHIVE_LEVEL-ZERO-GPU_FILE_NAME "intel-level-zero-gpu-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
  else()
    if(CPACK_GENERATOR STREQUAL "DEB")
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime_${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    elseif(CPACK_GENERATOR STREQUAL "RPM")
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    else()
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
    endif()
  endif()

  if(NEO__GMM_FOUND)
    list(APPEND _external_package_dependencies_debian "intel-gmmlib(=${NEO__GMM_VERSION})")
    list(APPEND _external_package_dependencies_rpm "intel-gmmlib = ${NEO__GMM_VERSION}")
  else()
    list(APPEND _external_package_dependencies_debian "intel-gmmlib")
    list(APPEND _external_package_dependencies_rpm "intel-gmmlib")
  endif()

  if(NEO__IGC_FOUND)
    list(APPEND _external_package_dependencies_debian "intel-igc-opencl(=${NEO__IGC_VERSION})")
    list(APPEND _external_package_dependencies_rpm "intel-igc-opencl = ${NEO__IGC_VERSION}")
    list(APPEND _igc_package_dependencies_debian "intel-igc-opencl(=${NEO__IGC_VERSION})")
    list(APPEND _igc_package_dependencies_rpm "intel-igc-opencl = ${NEO__IGC_VERSION}")
  else()
    list(APPEND _external_package_dependencies_debian "intel-igc-opencl")
    list(APPEND _external_package_dependencies_rpm "intel-igc-opencl")
    list(APPEND _igc_package_dependencies_debian "intel-igc-opencl")
    list(APPEND _igc_package_dependencies_rpm "intel-igc-opencl")
  endif()

  string(REPLACE ";" ", " CPACK_DEBIAN_OPENCL_PACKAGE_DEPENDS "${_external_package_dependencies_debian}")
  string(REPLACE ";" ", " CPACK_DEBIAN_OCLOC_PACKAGE_DEPENDS "${_igc_package_dependencies_debian}")
  string(REPLACE ";" ", " CPACK_DEBIAN_INTEL-LEVEL-ZERO-GPU_PACKAGE_DEPENDS "${_external_package_dependencies_deb}")
  string(REPLACE ";" ", " CPACK_RPM_OPENCL_PACKAGE_REQUIRES "${_external_package_dependencies_rpm}")
  string(REPLACE ";" ", " CPACK_RPM_OCLOC_PACKAGE_REQUIRES "${_igc_package_dependencies_rpm}")
  string(REPLACE ";" ", " CPACK_RPM_INTEL-LEVEL-ZERO-GPU_PACKAGE_REQUIRES "${_external_package_dependencies_rpm}")

  set(CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_RECOMMENDS "level-zero")
  set(CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_SUGGESTS "level-zero")
  set(CPACK_RPM_LEVEL-ZERO-GPU_PACKAGE_SUGGESTS "level-zero")

  set(CPACK_PROPERTIES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package_config.cmake")
  set(CPACK_LD_LIBRARY_PATH "${NEO__GMM_LIBRARY_PATH}")
  include(CPack)

  get_directory_property(__HAS_PARENT PARENT_DIRECTORY)
  if(__HAS_PARENT)
    set(NEO__COMPONENT_NAME "opencl" PARENT_SCOPE)
  endif()
endif(UNIX)
