/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment) {
    auto funcCreate = commandStreamReceiverFactory[executionEnvironment.getHardwareInfo()->pPlatform->eRenderCoreFamily];
    if (funcCreate == nullptr) {
        return nullptr;
    }
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    int32_t csr = CommandStreamReceiverType::CSR_HW;
    if (DebugManager.flags.SetCommandStreamReceiver.get() >= 0) {
        csr = DebugManager.flags.SetCommandStreamReceiver.get();
    }
    if (csr) {
        switch (csr) {
        case CSR_AUB:
            commandStreamReceiver = AUBCommandStreamReceiver::create("aubfile", true, executionEnvironment);
            break;
        case CSR_TBX:
            commandStreamReceiver = TbxCommandStreamReceiver::create("", false, executionEnvironment);
            break;
        case CSR_HW_WITH_AUB:
            commandStreamReceiver = funcCreate(true, executionEnvironment);
            break;
        case CSR_TBX_WITH_AUB:
            commandStreamReceiver = TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment);
            break;
        default:
            break;
        }
    } else {
        commandStreamReceiver = funcCreate(false, executionEnvironment);
    }
    return commandStreamReceiver;
}

bool getDevicesImpl(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    bool result;
    int32_t csr = CommandStreamReceiverType::CSR_HW;
    if (DebugManager.flags.SetCommandStreamReceiver.get() >= 0) {
        csr = DebugManager.flags.SetCommandStreamReceiver.get();
    }
    if (csr) {
        switch (csr) {
        case CSR_AUB:
        case CSR_TBX: {
        case CSR_TBX_WITH_AUB:
            return DeviceFactory::getDevicesForProductFamilyOverride(hwInfo, numDevicesReturned, executionEnvironment);
        }
        case CSR_HW_WITH_AUB:
            return DeviceFactory::getDevices(hwInfo, numDevicesReturned, executionEnvironment);
        default:
            return false;
        }
    }
    result = DeviceFactory::getDevices(hwInfo, numDevicesReturned, executionEnvironment);
    DEBUG_BREAK_IF(result && (hwInfo == nullptr));
    return result;
}

} // namespace OCLRT
