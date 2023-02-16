/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

namespace NEO {
INSTANTIATE_TEST_CASE_P(
    OclocFatbinaryMtlTests,
    OclocFatbinaryPerProductTests,
    ::testing::Combine(
        ::testing::Values("xe-lpg"),
        ::testing::Values(IGFX_METEORLAKE)));

} // namespace NEO