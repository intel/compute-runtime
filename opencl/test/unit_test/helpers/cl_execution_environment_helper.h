/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
class ExecutionEnvironment;
struct HardwareInfo;
ExecutionEnvironment *getClExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments);
} // namespace NEO
