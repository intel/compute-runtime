/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment) {
    auto funcCreate = commandStreamReceiverFactory[pHwInfo->pPlatform->eRenderCoreFamily];
    if (funcCreate == nullptr) {
        return nullptr;
    }
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr) {
        switch (csr) {
        case CSR_AUB:
            commandStreamReceiver = AUBCommandStreamReceiver::create(*pHwInfo, "aubfile", true, executionEnvironment);
            break;
        case CSR_TBX:
            commandStreamReceiver = TbxCommandStreamReceiver::create(*pHwInfo, false, executionEnvironment);
            break;
        case CSR_HW_WITH_AUB:
            commandStreamReceiver = funcCreate(*pHwInfo, true, executionEnvironment);
            break;
        case CSR_TBX_WITH_AUB:
            commandStreamReceiver = TbxCommandStreamReceiver::create(*pHwInfo, true, executionEnvironment);
            break;
        default:
            break;
        }
    } else {
        commandStreamReceiver = funcCreate(*pHwInfo, false, executionEnvironment);
    }
    return commandStreamReceiver;
}

bool getDevicesImpl(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    bool result;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
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
