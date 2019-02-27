/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/hw_info.h"

namespace OCLRT {
class ExecutionEnvironment;
extern CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment);
extern bool getDevicesImpl(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT
