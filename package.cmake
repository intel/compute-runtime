#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(CPACK_PACKAGE_ARCHITECTURE "x86_64")
else()
  set(CPACK_PACKAGE_ARCHITECTURE "x86")
endif()
set(CPACK_PACKAGE_RELOCATABLE FALSE)
set(CPACK_PACKAGE_NAME "intel")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intel(R) Graphics Compute Runtime")
set(CPACK_PACKAGE_VENDOR "Intel")
if(NEO_BUILD_L0_PACKAGE)
  set(CPACK_PACKAGE_VERSION_MAJOR ${NEO_L0_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${NEO_L0_VERSION_MINOR})
else()
  set(CPACK_PACKAGE_VERSION_MAJOR ${NEO_OCL_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${NEO_OCL_VERSION_MINOR})
endif()
set(CPACK_PACKAGE_VERSION_PATCH ${NEO_VERSION_BUILD})

if(UNIX)
  set(package_input_dir ${NEO_BINARY_DIR}/packageinput)
  set(package_output_dir ${NEO_BINARY_DIR}/packages)

  if(NEO_BUILD_OCL_PACKAGE AND NEO_BUILD_L0_PACKAGE)
    message(FATAL_ERROR "OpenCL and LevelZero packages cannot be created simultaneously")
  endif()

  if(NOT DEFINED NEO_OCL_VERSION_MAJOR)
    set(NEO_OCL_VERSION_MAJOR 1)
  endif()
  if(NOT DEFINED NEO_OCL_VERSION_MINOR)
    set(NEO_OCL_VERSION_MINOR 0)
  endif()
  if(NOT DEFINED NEO_VERSION_BUILD)
    set(NEO_VERSION_BUILD 0)
  endif()

  include("os_release_info.cmake")

  get_os_release_info(os_name os_version os_codename)
  if(NOT DEFINED OCL_ICD_VENDORDIR)
    if("${os_name}" STREQUAL "clear-linux-os")
      # clear-linux-os distribution avoids /etc for distribution defaults.
      set(OCL_ICD_VENDORDIR "/usr/share/defaults/etc/OpenCL/vendors")
    else()
      set(OCL_ICD_VENDORDIR "/etc/OpenCL/vendors")
    endif()
  endif()

  if(NEO_BUILD_WITH_OCL)
    set(NEO_OCL_ICD_FILE_NAME "intel${NEO__SO_NAME_SUFFIX}.icd")

    get_target_property(OCL_RUNTIME_LIB_NAME igdrcl_dll OUTPUT_NAME)
    install(
            CODE "file( WRITE ${NEO_BINARY_DIR}/${NEO_OCL_ICD_FILE_NAME} \"${CMAKE_INSTALL_FULL_LIBDIR}/intel-opencl/${CMAKE_SHARED_LIBRARY_PREFIX}${OCL_RUNTIME_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\n\" )"
            CODE "file( WRITE ${NEO_BINARY_DIR}/tmp/postinst \"/sbin/ldconfig\n\" )"
            CODE "file( WRITE ${NEO_BINARY_DIR}/tmp/postrm \"/sbin/ldconfig\n\" )"
            CODE "file( COPY ${NEO_BINARY_DIR}/tmp/postinst DESTINATION ${NEO_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )"
            CODE "file( COPY ${NEO_BINARY_DIR}/tmp/postrm DESTINATION ${NEO_BINARY_DIR} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )"
            COMPONENT opencl
    )
    install(FILES ${NEO_BINARY_DIR}/${NEO_OCL_ICD_FILE_NAME} DESTINATION ${OCL_ICD_VENDORDIR} COMPONENT opencl)
  endif()

  if(NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
    get_property(DEBUG_SYMBOL_FILES GLOBAL PROPERTY DEBUG_SYMBOL_FILES)
    install(FILES ${DEBUG_SYMBOL_FILES}
            DESTINATION ${DEBUG_SYMBOL_INSTALL_DIR}
            COMPONENT opencl-debuginfo
    )
    get_property(IGDRCL_SYMBOL_FILE GLOBAL PROPERTY IGDRCL_SYMBOL_FILE)
    install(FILES ${IGDRCL_SYMBOL_FILE}
            DESTINATION ${DEBUG_SYMBOL_INSTALL_DIR}/intel-opencl
            COMPONENT opencl-debuginfo
    )
  endif()

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
  if(NEO_BUILD_OCL_PACKAGE)
    get_property(CPACK_COMPONENTS_ALL GLOBAL PROPERTY NEO_OCL_COMPONENTS_LIST)
  endif()
  if(NEO_BUILD_L0_PACKAGE)
    get_property(CPACK_COMPONENTS_ALL GLOBAL PROPERTY NEO_L0_COMPONENTS_LIST)
  endif()
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
      /etc/ld.so.conf.d
      /usr/local
      /usr/local/lib64
      /usr/local/bin
  )

  if(CMAKE_VERSION VERSION_GREATER 3.6 OR CMAKE_VERSION VERSION_EQUAL 3.6)
    set(CPACK_DEBIAN_OPENCL_FILE_NAME "intel-opencl_${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_OCLOC_FILE_NAME "intel-ocloc_${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_LEVEL-ZERO-GPU_FILE_NAME "libze-intel-gpu1_${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    set(CPACK_DEBIAN_OPENCL-DEBUGINFO_FILE_NAME "intel-opencl-debuginfo_${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-1~${os_codename}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

    set(CPACK_RPM_OPENCL_FILE_NAME "intel-opencl-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    set(CPACK_RPM_OCLOC_FILE_NAME "intel-ocloc-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    set(CPACK_RPM_LEVEL-ZERO-GPU_FILE_NAME "libze-intel-gpu1-${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    set(CPACK_RPM_OPENCL-DEBUGINFO_FILE_NAME "intel-opencl-debuginfo-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")

    set(CPACK_ARCHIVE_OPENCL_FILE_NAME "intel-opencl-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${os_codename}-${CPACK_PACKAGE_ARCHITECTURE}")
    set(CPACK_ARCHIVE_OCLOC_FILE_NAME "intel-ocloc-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${os_codename}-${CPACK_PACKAGE_ARCHITECTURE}")
    set(CPACK_ARCHIVE_LEVEL-ZERO-GPU_FILE_NAME "libze-intel-gpu1-${NEO_L0_VERSION_MAJOR}.${NEO_L0_VERSION_MINOR}.${NEO_VERSION_BUILD}-${os_codename}_${CPACK_PACKAGE_ARCHITECTURE}")
    set(CPACK_ARCHIVE_OPENCL-DEBUGINFO_FILE_NAME "intel-opencl-debuginfo-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${os_codename}-${CPACK_PACKAGE_ARCHITECTURE}")
  else()
    if(CPACK_GENERATOR STREQUAL "DEB")
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime_${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    elseif(CPACK_GENERATOR STREQUAL "RPM")
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
    else()
      set(CPACK_PACKAGE_FILE_NAME "intel-compute-runtime-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
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
    string(REPLACE "." ";" NEO__IGC_VERSION_list ${NEO__IGC_VERSION})
    list(GET NEO__IGC_VERSION_list 0 NEO__IGC_VERSION_MAJOR)
    list(GET NEO__IGC_VERSION_list 1 NEO__IGC_VERSION_MINOR)
    math(EXPR NEO__IGC_VERSION_MINOR_UPPER "${NEO__IGC_VERSION_MINOR} + 3")
    set(NEO__IGC_VERSION_LOWER "${NEO__IGC_VERSION_MAJOR}.${NEO__IGC_VERSION_MINOR}")
    set(NEO__IGC_VERSION_UPPER "${NEO__IGC_VERSION_MAJOR}.${NEO__IGC_VERSION_MINOR_UPPER}")

    list(APPEND _external_package_dependencies_debian "intel-igc-opencl-2 (>=${NEO__IGC_VERSION_LOWER}), intel-igc-opencl-2 (<<${NEO__IGC_VERSION_UPPER})")
    list(APPEND _external_package_dependencies_rpm "intel-igc-opencl-2 >= ${NEO__IGC_VERSION_LOWER}, intel-igc-opencl-2 < ${NEO__IGC_VERSION_UPPER}")
    list(APPEND _igc_package_dependencies_debian "intel-igc-opencl-2 (>=${NEO__IGC_VERSION_LOWER}), intel-igc-opencl-2 (<<${NEO__IGC_VERSION_UPPER})")
    list(APPEND _igc_package_dependencies_rpm "intel-igc-opencl-2 >= ${NEO__IGC_VERSION_LOWER}, intel-igc-opencl-2 < ${NEO__IGC_VERSION_UPPER}")
  else()
    list(APPEND _external_package_dependencies_debian "intel-igc-opencl-2")
    list(APPEND _external_package_dependencies_rpm "intel-igc-opencl-2")
    list(APPEND _igc_package_dependencies_debian "intel-igc-opencl-2")
    list(APPEND _igc_package_dependencies_rpm "intel-igc-opencl-2")
  endif()

  set(_external_package_dependencies_debian_level_zero_gpu "${_external_package_dependencies_debian}")
  set(_external_package_optionals_debian_level_zero_gpu "${_external_package_optionals_debian}")
  set(_external_package_dependencies_rpm_level_zero_gpu "${_external_package_dependencies_rpm}")
  set(_external_package_optionals_rpm_level_zero_gpu "${_external_package_optionals_rpm}")

  list(APPEND _external_package_optionals_debian_level_zero_gpu "level-zero")
  list(APPEND _external_package_optionals_rpm_level_zero_gpu "level-zero")

  if(PC_LIBXML_FOUND)
    list(APPEND _external_package_optionals_debian_level_zero_gpu "libxml2")
    list(APPEND _external_package_optionals_rpm_level_zero_gpu "libxml2")
  endif()

  string(REPLACE ";" ", " CPACK_DEBIAN_OPENCL_PACKAGE_DEPENDS "${_external_package_dependencies_debian}")
  string(REPLACE ";" ", " CPACK_DEBIAN_OCLOC_PACKAGE_DEPENDS "${_igc_package_dependencies_debian}")
  string(REPLACE ";" ", " CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_DEPENDS "${_external_package_dependencies_debian_level_zero_gpu}")
  string(REPLACE ";" ", " CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_RECOMMENDS "${_external_package_optionals_debian_level_zero_gpu}")
  string(REPLACE ";" ", " CPACK_RPM_OPENCL_PACKAGE_REQUIRES "${_external_package_dependencies_rpm}")
  string(REPLACE ";" ", " CPACK_RPM_OCLOC_PACKAGE_REQUIRES "${_igc_package_dependencies_rpm}")
  string(REPLACE ";" ", " CPACK_RPM_LEVEL-ZERO-GPU_PACKAGE_REQUIRES "${_external_package_dependencies_rpm_level_zero_gpu}")
  string(REPLACE ";" ", " CPACK_RPM_LEVEL-ZERO-GPU_PACKAGE_SUGGESTS "${_external_package_optionals_rpm_level_zero_gpu}")

  set(CPACK_DEBIAN_LEVEL-ZERO-GPU_PACKAGE_SUGGESTS "level-zero")

  set(CPACK_PROPERTIES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package_config.cmake")
  set(CPACK_LD_LIBRARY_PATH "${NEO__GMM_LIBRARY_PATH}")
  include(CPack)

  get_directory_property(__HAS_PARENT PARENT_DIRECTORY)
  if(__HAS_PARENT)
    set(NEO__COMPONENT_NAME "opencl" PARENT_SCOPE)
  endif()
elseif(WIN32)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
  set(CPACK_COMPONENTS_ALL ocloc)

  set(CPACK_ARCHIVE_OCLOC_FILE_NAME "ocloc-${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}-${CPACK_PACKAGE_ARCHITECTURE}")
  include(CPack)
endif()
