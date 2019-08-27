/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/root_device.h"

#include "runtime/device/sub_device.h"

namespace NEO {

RootDevice::~RootDevice() = default;
RootDevice::RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : Device(executionEnvironment, deviceIndex) {}

} // namespace NEO
