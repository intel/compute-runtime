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

if(NOT DISABLED_GTPIN_SUPPORT)
  set(RUNTIME_SRCS_GTPIN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_callbacks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_hw_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_hw_helper.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_init.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_notify.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_defs.h
  )

  if(WIN32)
    set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}\nGTPin_Init")
  endif()

else()
  set(RUNTIME_SRCS_GTPIN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_callback_stubs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_notify.h
  )
endif()

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GTPIN})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_GTPIN ${RUNTIME_SRCS_GTPIN})
set(MSVC_DEF_ADDITIONAL_EXPORTS "${MSVC_DEF_ADDITIONAL_EXPORTS}" PARENT_SCOPE)
