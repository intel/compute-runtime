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

set(SCHEDULER_INCLUDE scheduler/${NEO_ARCH})
set(SCHEDULER_FILENAME scheduler)

set(IGDRCL_SRCS_tests_scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_kernel_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    PARENT_SCOPE
)

foreach(GEN_NUM RANGE 0 ${MAX_GEN} 1)
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_NUM} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      set(RUNTIME_GENERATED_SCHEDULER_GEN${GEN_NUM}
        ${SCHEDULER_INCLUDE}/${PLATFORM_IT_LOWER}/${SCHEDULER_FILENAME}_${PLATFORM_IT_LOWER}.cpp
        PARENT_SCOPE
      )
    endforeach(PLATFORM_IT)
  endif(${GENX_HAS_PLATFORMS})
endforeach(GEN_NUM)
