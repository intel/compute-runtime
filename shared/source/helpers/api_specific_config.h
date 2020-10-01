/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct ApiSpecificConfig {
    static bool getHeapConfiguration();
    static bool getBindelssConfiguration();
};
} // namespace NEO