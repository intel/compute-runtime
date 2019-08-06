/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

#include <cassert>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
bool getDevicesResult = true;

bool overrideCommandStreamReceiverCreation = false;
bool overrideDeviceWithDefaultHardwareInfo = true;

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment) {
    auto hwInfo = executionEnvironment.getHardwareInfo();

    if (overrideCommandStreamReceiverCreation) {
        return createCommandStreamImpl(executionEnvironment);
    }

    auto funcCreate = commandStreamReceiverFactory[IGFX_MAX_CORE + hwInfo->platform.eRenderCoreFamily];
    if (funcCreate) {
        return funcCreate(false, executionEnvironment);
    }
    return nullptr;
}

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (overrideDeviceWithDefaultHardwareInfo) {
        numDevicesReturned = numPlatformDevices;
        return getDevicesResult;
    }

    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}
} // namespace NEO
