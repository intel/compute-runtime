/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/create_command_stream.h"

#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include <cassert>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                           uint32_t rootDeviceIndex,
                                           const DeviceBitfield deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    if (ultHwConfig.useHwCsr) {
        return createCommandStreamImpl(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    auto funcCreate = commandStreamReceiverFactory[IGFX_MAX_CORE + hwInfo->platform.eRenderCoreFamily];
    if (funcCreate) {
        return funcCreate(false, executionEnvironment, rootDeviceIndex, deviceBitfield);
    }
    return nullptr;
}

bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment) {
    auto retVal = true;
    if (executionEnvironment.rootDeviceEnvironments.size() == 0) {
        executionEnvironment.prepareRootDeviceEnvironments(1u);
    }
    auto currentHwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    if (currentHwInfo->platform.eProductFamily == IGFX_UNKNOWN && currentHwInfo->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE) {
        executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    }

    if (ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc) {
        uint32_t numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get() != 0 ? DebugManager.flags.CreateMultipleRootDevices.get() : 1u;
        UltDeviceFactory::prepareDeviceEnvironments(executionEnvironment, numRootDevices);
        retVal = ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult;
    } else {
        retVal = prepareDeviceEnvironmentsImpl(executionEnvironment);
    }

    for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    }

    return retVal;
}

bool prepareDeviceEnvironment(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex) {
    auto retVal = true;
    executionEnvironment.prepareRootDeviceEnvironment(rootDeviceIndex);
    auto currentHwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    if (currentHwInfo->platform.eProductFamily == IGFX_UNKNOWN && currentHwInfo->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
    }
    if (ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc) {
        uint32_t numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get() != 0 ? DebugManager.flags.CreateMultipleRootDevices.get() : 1u;
        UltDeviceFactory::prepareDeviceEnvironments(executionEnvironment, numRootDevices);
        retVal = ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult;
    } else {
        retVal = prepareDeviceEnvironmentImpl(executionEnvironment, osPciPath, rootDeviceIndex);
    }

    for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    }

    return retVal;
}
const HardwareInfo *getDefaultHwInfo() {
    return defaultHwInfo.get();
}

} // namespace NEO
