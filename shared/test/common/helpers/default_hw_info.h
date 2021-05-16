/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <memory>

namespace NEO {
struct HardwareInfo;
extern std::unique_ptr<HardwareInfo> defaultHwInfo;
} // namespace NEO
