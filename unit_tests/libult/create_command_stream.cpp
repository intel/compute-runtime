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

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
bool getDevicesResult = true;

bool overrideCommandStreamReceiverCreation = false;
bool overrideDeviceWithDefaultHardwareInfo = true;

CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment) {
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    assert(nullptr != pHwInfo->pPlatform);
    auto offset = !overrideCommandStreamReceiverCreation ? IGFX_MAX_CORE : 0;
    if (offset != 0) {
        auto funcCreate = commandStreamReceiverFactory[offset + pHwInfo->pPlatform->eRenderCoreFamily];
        if (funcCreate) {
            commandStreamReceiver = funcCreate(*pHwInfo, false, executionEnvironment);
        }
    } else {
        commandStreamReceiver = createCommandStreamImpl(pHwInfo, executionEnvironment);
    }
    return commandStreamReceiver;
}

bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (overrideDeviceWithDefaultHardwareInfo) {
        *hwInfo = const_cast<HardwareInfo *>(*platformDevices);
        executionEnvironment.setHwInfo(*hwInfo);
        numDevicesReturned = numPlatformDevices;
        return getDevicesResult;
    }

    return getDevicesImpl(hwInfo, numDevicesReturned, executionEnvironment);
}
} // namespace OCLRT
