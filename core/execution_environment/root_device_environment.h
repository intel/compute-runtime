/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/common_types.h"

namespace NEO {

class AubCenter;

struct RootDeviceEnvironment {
    RootDeviceEnvironment();
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    RootDeviceEnvironment(RootDeviceEnvironment &&);
    ~RootDeviceEnvironment();
    std::unique_ptr<AubCenter> aubCenter;
    CsrContainer commandStreamReceivers;
};
} // namespace NEO
