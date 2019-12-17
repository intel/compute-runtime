#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(extension bat)
else()
  set(extension sh)
endif()

add_custom_target(format_files
  ${NEO_SOURCE_DIR}/scripts/format/format.${extension} ${NEO_SOURCE_DIR}
  WORKING_DIRECTORY ${NEO_SOURCE_DIR}
  COMMENT "Formatting changed files"
)

set_target_properties(format_files PROPERTIES
  EXCLUDE_FROM_DEFAULT_BUILD TRUE
  EXCLUDE_FROM_ALL TRUE
)
