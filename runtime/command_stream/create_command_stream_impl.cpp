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
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto funcCreate = commandStreamReceiverFactory[executionEnvironment.getHardwareInfo()->platform.eRenderCoreFamily];
    if (funcCreate == nullptr) {
        return nullptr;
    }
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr < 0) {
        csr = CommandStreamReceiverType::CSR_HW;
    }
    switch (csr) {
    case CSR_HW:
        commandStreamReceiver = funcCreate(false, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_AUB:
        commandStreamReceiver = AUBCommandStreamReceiver::create("aubfile", true, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_TBX:
        commandStreamReceiver = TbxCommandStreamReceiver::create("", false, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_HW_WITH_AUB:
        commandStreamReceiver = funcCreate(true, executionEnvironment, rootDeviceIndex);
        break;
    case CSR_TBX_WITH_AUB:
        commandStreamReceiver = TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, rootDeviceIndex);
        break;
    default:
        break;
    }
    return commandStreamReceiver;
}

bool getDevicesImpl(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    bool result;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr < 0) {
        csr = CommandStreamReceiverType::CSR_HW;
    }
    switch (csr) {
    case CSR_HW:
        result = DeviceFactory::getDevices(numDevicesReturned, executionEnvironment);
        DEBUG_BREAK_IF(!result);
        return result;
    case CSR_AUB:
    case CSR_TBX:
    case CSR_TBX_WITH_AUB:
        return DeviceFactory::getDevicesForProductFamilyOverride(numDevicesReturned, executionEnvironment);
    case CSR_HW_WITH_AUB:
        return DeviceFactory::getDevices(numDevicesReturned, executionEnvironment);
    default:
        return false;
    }
}

} // namespace NEO
