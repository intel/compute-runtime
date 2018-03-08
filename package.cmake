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

if(UNIX)
  set(package_input_dir ${IGDRCL_BINARY_DIR}/packageinput)
  set(package_output_dir ${IGDRCL_BINARY_DIR}/packages)

  if(NOT NEO_VERSION_MAJOR)
    set(NEO_VERSION_MAJOR 1)
  endif()
  if(NOT NEO_VERSION_MINOR)
    set(NEO_VERSION_MINOR 0)
  endif()
  if(NOT NEO_VERSION_BUILD)
    set(NEO_VERSION_BUILD 0)
  endif()

  set(NEO_BINARY_INSTALL_DIR /opt/intel/opencl)
  set(CMAKE_INSTALL_PREFIX ${NEO_BINARY_INSTALL_DIR})

  install(FILES
    ${IGDRCL_BINARY_DIR}/bin/libigdrcl.so
    ${IGDRCL_BINARY_DIR}/bin/libigdccl.so
    ${IGDRCL_BINARY_DIR}/bin/libigdfcl.so
    ${IGDRCL_BINARY_DIR}/bin/libiga64.so
    ${IGDRCL_BINARY_DIR}/bin/lib${COMMON_CLANG_LIBRARY_NAME}.so
    DESTINATION ${NEO_BINARY_INSTALL_DIR}
    COMPONENT igdrcl
  )

  set(OCL_ICD_RUNTIME_NAME libigdrcl.so)
  install(
    CODE "file( WRITE  ${IGDRCL_BINARY_DIR}/libintelopencl.conf \"/opt/intel/opencl\n\" )"
    CODE "file( WRITE  ${IGDRCL_BINARY_DIR}/intel.icd \"/opt/intel/opencl/${OCL_ICD_RUNTIME_NAME}\n\" )"
    CODE "file( WRITE  ${IGDRCL_BINARY_DIR}/postinst \"echo /opt/intel/opencl >> /etc/ld.so.conf\n\" )"
    CODE "file( APPEND ${IGDRCL_BINARY_DIR}/postinst \"/sbin/ldconfig\n\" )"
    CODE "file( WRITE  ${IGDRCL_BINARY_DIR}/postrm \"sed -i '/\\\\/opt\\\\/intel\\\\/opencl.*$/d' /etc/ld.so.conf\n\" )"
    CODE "file( APPEND ${IGDRCL_BINARY_DIR}/postrm \"/sbin/ldconfig\n\" )"
    COMPONENT igdrcl
  )
  install(FILES ${IGDRCL_BINARY_DIR}/libintelopencl.conf DESTINATION /etc/ld.so.conf.d COMPONENT igdrcl)
  install(FILES ${IGDRCL_BINARY_DIR}/intel.icd DESTINATION /etc/OpenCL/vendors/ COMPONENT igdrcl)

  # Add Khronos ICD loader - if available
  if(NOT ICD_LIB_DIR)
    # Try to find ICD in upper level directory
    if(EXISTS ${IGDRCL_SOURCE_DIR}/../OpenCL-ICD-Loader/build/lib/libOpenCL.so)
      set(ICD_LIB_DIR ${IGDRCL_SOURCE_DIR}/../OpenCL-ICD-Loader/build/lib)
      message(STATUS "Taking ICD library from ${ICD_LIB_DIR}")
    else()
      get_filename_component(IGDRCL_PARENT_DIR ${IGDRCL_SOURCE_DIR} DIRECTORY)
    endif()
  endif()

  if(ICD_LIB_DIR)
    get_filename_component(ICD_LIB_DIR ${ICD_LIB_DIR} ABSOLUTE)
    set(ICD_LIB_NAME "libOpenCL.so*")
    install(
      CODE "if(NOT((EXISTS ${ICD_LIB_DIR}/libOpenCL.so) OR (IS_SYMLINK ${ICD_LIB_DIR}/libOpenCL.so)))\n execute_process( COMMAND ln -s ${NEO_BINARY_INSTALL_DIR}/libOpenCL.so.1 ${ICD_LIB_DIR}/libOpenCL.so)\n endif()\n"
      CODE "file( GLOB _NeoIcdLibFiles \"${ICD_LIB_DIR}/${ICD_LIB_NAME}\" )"
      CODE "if(NOT _NeoIcdLibFiles)\n message(FATAL_ERROR \"${ICD_LIB_NAME} cannot be found in ${ICD_LIB_DIR}\")\nendif()"
      CODE "file( INSTALL \${_NeoIcdLibFiles} DESTINATION \"${NEO_BINARY_INSTALL_DIR}\" )"
      COMPONENT igdrcl
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
  set(CPACK_PACKAGE_RELOCATABLE FALSE)
  set(CPACK_PACKAGE_NAME "intel-opencl")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intel OpenCL GPU driver")
  set(CPACK_PACKAGE_VENDOR "Intel")
  set(CPACK_PACKAGE_VERSION_MAJOR ${NEO_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${NEO_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${NEO_VERSION_BUILD})
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "postinst;postrm")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
  set(CPACK_RPM_COMPRESSION_TYPE "xz")
  set(CPACK_RPM_PACKAGE_DESCRIPTION "Intel OpenCL GPU driver")
  set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${IGDRCL_BINARY_DIR}/postinst")
  set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${IGDRCL_BINARY_DIR}/postrm")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "/opt/intel/opencl")
  set(CPACK_PACKAGE_CONTACT "Intel Corporation")
  set(CPACK_PACKAGE_FILE_NAME "intel-opencl-${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}-${NEO_VERSION_BUILD}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
  set(CPACK_COMPONENTS_ALL igdrcl)

  include(CPack)

endif(UNIX)
