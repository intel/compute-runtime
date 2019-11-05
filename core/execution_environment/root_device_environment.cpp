/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"

#include "runtime/aub/aub_center.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment() = default;
RootDeviceEnvironment::RootDeviceEnvironment(RootDeviceEnvironment &&) = default;
RootDeviceEnvironment::~RootDeviceEnvironment() = default;
} // namespace NEO
