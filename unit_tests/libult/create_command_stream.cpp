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
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    auto hwInfo = executionEnvironment.getHardwareInfo();
    auto offset = !overrideCommandStreamReceiverCreation ? IGFX_MAX_CORE : 0;
    if (offset != 0) {
        auto funcCreate = commandStreamReceiverFactory[offset + hwInfo->platform.eRenderCoreFamily];
        if (funcCreate) {
            commandStreamReceiver = funcCreate(false, executionEnvironment);
        }
    } else {
        commandStreamReceiver = createCommandStreamImpl(executionEnvironment);
    }
    return commandStreamReceiver;
}

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (overrideDeviceWithDefaultHardwareInfo) {
        numDevicesReturned = numPlatformDevices;
        return getDevicesResult;
    }

    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}
} // namespace NEO
