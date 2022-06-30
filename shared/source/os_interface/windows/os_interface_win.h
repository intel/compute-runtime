/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {

class HwDeviceId;
struct RootDeviceEnvironment;

bool initWddmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, RootDeviceEnvironment *rootDeviceEnv);

} // namespace NEO
