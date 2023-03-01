/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
#include <vector>

namespace NEO {
class HwDeviceId;
} // namespace NEO

namespace L0 {
namespace Sysman {
std::unique_ptr<NEO::HwDeviceId> createSysmanHwDeviceId(std::unique_ptr<NEO::HwDeviceId> &hwDeviceId);
} // namespace Sysman
} // namespace L0