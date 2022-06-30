#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TBX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_proto.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_sockets_imp.h
)
set_property(GLOBAL PROPERTY NEO_CORE_TBX ${NEO_CORE_TBX})
