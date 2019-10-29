/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"

namespace NEO {
class ExecutionEnvironment;
extern CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
extern bool getDevicesImpl(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace NEO
