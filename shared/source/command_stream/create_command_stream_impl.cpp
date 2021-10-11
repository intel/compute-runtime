/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/device_factory.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment,
                                               uint32_t rootDeviceIndex,
                                               const DeviceBitfield deviceBitfield) {
    auto funcCreate = commandStreamReceiverFactory[executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->platform.eRenderCoreFamily];
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
        commandStreamReceiver = funcCreate(false, executionEnvironment, rootDeviceIndex, deviceBitfield);
        break;
    case CSR_AUB:
        commandStreamReceiver = AUBCommandStreamReceiver::create(ApiSpecificConfig::getName(), true, executionEnvironment, rootDeviceIndex, deviceBitfield);
        break;
    case CSR_TBX:
        commandStreamReceiver = TbxCommandStreamReceiver::create("", false, executionEnvironment, rootDeviceIndex, deviceBitfield);
        break;
    case CSR_HW_WITH_AUB:
        commandStreamReceiver = funcCreate(true, executionEnvironment, rootDeviceIndex, deviceBitfield);
        break;
    case CSR_TBX_WITH_AUB:
        commandStreamReceiver = TbxCommandStreamReceiver::create(ApiSpecificConfig::getName(), true, executionEnvironment, rootDeviceIndex, deviceBitfield);
        break;
    default:
        break;
    }
    return commandStreamReceiver;
}

bool prepareDeviceEnvironmentsImpl(ExecutionEnvironment &executionEnvironment) {
    if (DeviceFactory::isHwModeSelected()) {
        return DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    }
    return DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
}

bool prepareDeviceEnvironmentImpl(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex) {
    if (DeviceFactory::isHwModeSelected()) {
        return DeviceFactory::prepareDeviceEnvironment(executionEnvironment, osPciPath, rootDeviceIndex);
    }
    return false;
}

} // namespace NEO
