/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct ApiSpecificConfig {
    enum ApiType { OCL,
                   L0 };
    static bool getHeapConfiguration();
    static bool getBindlessConfiguration();
    static ApiType getApiType();
};
} // namespace NEO