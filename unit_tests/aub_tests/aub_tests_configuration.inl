/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aub_tests/aub_tests_configuration.h"

template <typename GfxFamily>
AubTestsConfig GetAubTestsConfig() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = true;
    return aubTestsConfig;
}