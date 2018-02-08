# Copyright (c) 2018, Intel Corporation
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

if(GTPIN_HEADERS_DIR)
  set(GTPIN_INIT_FILE "gtpin/gtpin_init.cpp")
else()
  set(GTPIN_INIT_FILE "")
endif()

set(RUNTIME_SRCS_DLL_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_tbx_sockets.cpp

  ${CMAKE_SOURCE_DIR}/runtime/api/api.cpp
  ${CMAKE_SOURCE_DIR}/runtime/gmm_helper/resource_info.cpp
  ${CMAKE_SOURCE_DIR}/runtime/helpers/abort.cpp
  ${CMAKE_SOURCE_DIR}/runtime/helpers/debug_helpers.cpp
  ${CMAKE_SOURCE_DIR}/runtime/program/evaluate_unhandled_token.cpp

  ${DEF_FILE}
  ${GTPIN_INIT_FILE}
  ${HW_SRC_LINK}
  PARENT_SCOPE
)

set(RUNTIME_SRCS_DLL_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_neo_create.cpp
  PARENT_SCOPE
)

set(RUNTIME_SRCS_DLL_WINDOWS
  ${CMAKE_SOURCE_DIR}/runtime/os_interface/windows/wddm_create.cpp
  ${CMAKE_SOURCE_DIR}/runtime/gmm_helper/page_table_mngr.cpp
  ${CMAKE_SOURCE_DIR}/runtime/gmm_helper/gmm_memory.cpp
  PARENT_SCOPE
)
