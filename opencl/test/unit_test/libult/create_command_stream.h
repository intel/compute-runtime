/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {
class CommandStreamReceiver;
class ExecutionEnvironment;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
extern bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);
} // namespace NEO
