/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

#include <cassert>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
bool getDevicesResult = true;

bool overrideCommandStreamReceiverCreation = false;
bool overrideDeviceWithDefaultHardwareInfo = true;

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto hwInfo = executionEnvironment.getHardwareInfo();

    if (overrideCommandStreamReceiverCreation) {
        return createCommandStreamImpl(executionEnvironment, rootDeviceIndex);
    }

    auto funcCreate = commandStreamReceiverFactory[IGFX_MAX_CORE + hwInfo->platform.eRenderCoreFamily];
    if (funcCreate) {
        return funcCreate(false, executionEnvironment, rootDeviceIndex);
    }
    return nullptr;
}

bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (overrideDeviceWithDefaultHardwareInfo) {
        numDevicesReturned = numPlatformDevices;
        executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numDevicesReturned));
        executionEnvironment.calculateMaxOsContextCount();
        return getDevicesResult;
    }

    return getDevicesImpl(numDevicesReturned, executionEnvironment);
}
} // namespace NEO
