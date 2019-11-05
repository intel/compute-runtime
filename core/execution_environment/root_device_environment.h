/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

namespace NEO {

class AubCenter;

struct RootDeviceEnvironment {
    RootDeviceEnvironment();
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    RootDeviceEnvironment(RootDeviceEnvironment &&);
    ~RootDeviceEnvironment();
    std::unique_ptr<AubCenter> aubCenter;
};
} // namespace NEO
