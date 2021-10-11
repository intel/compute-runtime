/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {
class ExecutionEnvironment;
extern CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment,
                                                      uint32_t rootDeviceIndex,
                                                      const DeviceBitfield deviceBitfield);
extern bool prepareDeviceEnvironmentsImpl(ExecutionEnvironment &executionEnvironment);
extern bool prepareDeviceEnvironmentImpl(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex);
} // namespace NEO
