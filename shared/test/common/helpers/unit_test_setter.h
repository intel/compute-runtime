/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct RootDeviceEnvironment;

struct UnitTestSetter {
    static void setCcsExposure(RootDeviceEnvironment &rootDeviceEnvironment);
};

} // namespace NEO
