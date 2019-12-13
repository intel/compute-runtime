#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_tests PRIVATE ${NEO_SOURCE_DIR}/unit_tests/core_unit_tests_files.cmake)

append_sources_from_properties(NEO_CORE_UNIT_TESTS_SOURCES
  NEO_CORE_COMMAND_CONTAINER_TESTS
  NEO_CORE_DEBUG_SETTINGS_TESTS
  NEO_CORE_INDIRECT_HEAP_TESTS
)

set_property(GLOBAL PROPERTY NEO_CORE_UNIT_TESTS_SOURCES ${NEO_CORE_UNIT_TESTS_SOURCES})

target_sources(igdrcl_tests PRIVATE ${NEO_CORE_UNIT_TESTS_SOURCES})
