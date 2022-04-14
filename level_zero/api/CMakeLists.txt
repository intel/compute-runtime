#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_subdirectory(core)
add_subdirectory(driver_experimental/public)
add_subdirectory(driver_experimental${BRANCH_DIR_SUFFIX})
add_subdirectory(driver_experimental/tracing)
add_subdirectory(tools)
add_subdirectory(sysman)
add_subdirectory(extensions${BRANCH_DIR_SUFFIX})
add_subdirectory(extensions/public)

set(L0_API "")
append_sources_from_properties(L0_API
                               L0_SRCS_API
                               L0_PUBLIC_DRIVER_EXPERIMENTAL_EXTENSIONS_API
                               L0_EXTENDED_DRIVER_EXPERIMENTAL_EXTENSIONS_API
                               L0_EXPERIMENTAL_TRACING_API
                               L0_SYSMAN_API
                               L0_TOOLS_API
                               L0_EXTENSIONS_SRCS_API
                               L0_PUBLIC_EXTENSIONS_SRCS_API
)

set_property(GLOBAL PROPERTY L0_API ${L0_API})
