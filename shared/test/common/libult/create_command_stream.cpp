/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/create_command_stream.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/tests_configuration.h"

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                           uint32_t rootDeviceIndex,
                                           const DeviceBitfield deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    if (ultHwConfig.useHwCsr) {
        return createCommandStreamImpl(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    if (ultHwConfig.aubTestName != nullptr) {
        if (testMode == TestMode::aubTestsWithTbx) {
            return TbxCommandStreamReceiver::create(ultHwConfig.aubTestName, true, executionEnvironment, rootDeviceIndex, deviceBitfield);
        } else {
            return AUBCommandStreamReceiver::create(ultHwConfig.aubTestName, true, executionEnvironment, rootDeviceIndex, deviceBitfield);
        }
    }

    auto funcCreate = commandStreamReceiverFactory[IGFX_MAX_CORE + hwInfo->platform.eRenderCoreFamily];
    if (funcCreate) {
        return funcCreate(false, executionEnvironment, rootDeviceIndex, deviceBitfield);
    }
    return nullptr;
}

bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment) {
    auto retVal = true;
    if (ultHwConfig.sourceExecutionEnvironment && ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc) {
        if (executionEnvironment.rootDeviceEnvironments.size() < ultHwConfig.sourceExecutionEnvironment->rootDeviceEnvironments.size()) {
            executionEnvironment.rootDeviceEnvironments.resize(ultHwConfig.sourceExecutionEnvironment->rootDeviceEnvironments.size());
        }
        for (uint32_t i = 0; i < ultHwConfig.sourceExecutionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment.rootDeviceEnvironments[i] = std::make_unique<RootDeviceEnvironment>(executionEnvironment);
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment.rootDeviceEnvironments[i]->initGmm();
            executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();

            executionEnvironment.rootDeviceEnvironments[i]->getMutableHardwareInfo()->platform = ultHwConfig.sourceExecutionEnvironment->rootDeviceEnvironments[i]->getHardwareInfo()->platform;
            executionEnvironment.rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable = ultHwConfig.sourceExecutionEnvironment->rootDeviceEnvironments[i]->getHardwareInfo()->capabilityTable;
        }
        return ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult;
    }
    if (executionEnvironment.rootDeviceEnvironments.size() == 0) {
        executionEnvironment.prepareRootDeviceEnvironments(1u);
    }
    auto currentHwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    if (currentHwInfo->platform.eProductFamily == IGFX_UNKNOWN && currentHwInfo->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE) {
        executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    }

    if (ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc) {
        uint32_t numRootDevices = debugManager.flags.CreateMultipleRootDevices.get() != 0 ? debugManager.flags.CreateMultipleRootDevices.get() : 1u;
        retVal = UltDeviceFactory::prepareDeviceEnvironments(executionEnvironment, numRootDevices);
        retVal &= ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult;
    } else {
        retVal = prepareDeviceEnvironmentsImpl(executionEnvironment);
    }

    if (retVal) {
        for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initGmm();
        }
    }

    return retVal;
}

bool prepareDeviceEnvironment(ExecutionEnvironment &executionEnvironment, std::string &osPciPath, const uint32_t rootDeviceIndex) {
    auto retVal = true;
    executionEnvironment.prepareRootDeviceEnvironment(rootDeviceIndex);
    auto currentHwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    if (currentHwInfo->platform.eProductFamily == IGFX_UNKNOWN && currentHwInfo->platform.eRenderCoreFamily == IGFX_UNKNOWN_CORE) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    }
    if (ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc) {
        uint32_t numRootDevices = debugManager.flags.CreateMultipleRootDevices.get() != 0 ? debugManager.flags.CreateMultipleRootDevices.get() : 1u;
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
