/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"

namespace NEO {
INSTANTIATE_TEST_SUITE_P(
    OclocFatbinaryMtlTests,
    OclocFatbinaryPerProductTests,
    ::testing::Combine(
        ::testing::Values("xe-lpg"),
        ::testing::Values(IGFX_METEORLAKE)));

} // namespace NEO