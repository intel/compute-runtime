/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"

namespace OCLRT {
extern bool overrideCommandStreamReceiverCreation;
extern bool overrideDeviceWithDefaultHardwareInfo;
extern bool overrideMemoryManagerCreation;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment);
extern bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT
