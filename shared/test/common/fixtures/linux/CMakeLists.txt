#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_libult_common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/device_id_fixture.h)
endif()

