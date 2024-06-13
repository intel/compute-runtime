#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(ocloc_cmd_prefix ocloc)
else()
  if(DEFINED NEO__IGC_LIBRARY_PATH)
    set(ocloc_cmd_prefix ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${NEO__IGC_LIBRARY_PATH}:$<TARGET_FILE_DIR:ocloc_lib>" $<TARGET_FILE:ocloc>)
  else()
    set(ocloc_cmd_prefix ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$<TARGET_FILE_DIR:ocloc_lib>" $<TARGET_FILE:ocloc>)
  endif()
endif()

if(NEO_USE_CL_CACHE)
  set(ocloc_cmd_prefix ${ocloc_cmd_prefix} -allow_caching)
endif()
