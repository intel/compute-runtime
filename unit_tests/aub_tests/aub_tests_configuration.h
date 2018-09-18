/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

struct AubTestsConfig {
    bool testCanonicalAddress;
};

template <typename GfxFamily>
AubTestsConfig GetAubTestsConfig();
