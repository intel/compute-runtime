/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <memory>

namespace NEO {

class HwDeviceId;
struct RootDeviceEnvironment;

bool initDrmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex,
                        RootDeviceEnvironment *rootDeviceEnv);

} // namespace NEO
