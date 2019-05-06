/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"

namespace NEO {
extern bool overrideCommandStreamReceiverCreation;
extern bool overrideDeviceWithDefaultHardwareInfo;
extern bool overrideMemoryManagerCreation;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment);
extern bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace NEO
