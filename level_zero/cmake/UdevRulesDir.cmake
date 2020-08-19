#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(UDEV_RULES_DIR_FOUND FALSE)

foreach(rules_dir IN ITEMS "/lib/udev/rules.d" "/usr/lib/udev/rules.d")
  if(IS_DIRECTORY ${rules_dir})
    set(UDEV_RULES_DIR ${rules_dir} CACHE PATH "Install path for udev rules")
    set(UDEV_RULES_DIR_FOUND TRUE)
    break()
  endif()
endforeach()
