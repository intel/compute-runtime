/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct UltHwConfig {
    bool mockedGetDevicesFuncResult = true;
    bool useHwCsr = false;
    bool useMockedGetDevicesFunc = true;
    bool forceOsAgnosticMemoryManager = true;
};

extern UltHwConfig ultHwConfig;
} // namespace NEO
