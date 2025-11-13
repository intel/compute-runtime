/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

bool &MockClDevice::createSingleDevice = MockDevice::createSingleDevice;

decltype(&createCommandStream) &MockClDevice::createCommandStreamReceiverFunc = MockDevice::createCommandStreamReceiverFunc;

MockClDevice::MockClDevice(MockDevice *pMockDevice)
    : ClDevice(*pMockDevice, platform(pMockDevice->executionEnvironment)), device(*pMockDevice), sharedDeviceInfo(device.deviceInfo),
      executionEnvironment(pMockDevice->executionEnvironment), allEngines(pMockDevice->allEngines) {
}

MockClExecutionEnvironment *MockClDevice::prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
    auto executionEnvironment = new MockClExecutionEnvironment();
    auto numRootDevices = debugManager.flags.CreateMultipleRootDevices.get() ? debugManager.flags.CreateMultipleRootDevices.get() : rootDeviceIndex + 1;
    executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
    pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(pHwInfo);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->calculateMaxOsContextCount();
    return executionEnvironment;
}

std::unique_ptr<CommandStreamReceiver> MockClDevice::createCommandStreamReceiver() const {
    return device.createCommandStreamReceiver();
}

std::unique_ptr<BuiltinDispatchInfoBuilder> MockClDevice::setBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, std::unique_ptr<BuiltinDispatchInfoBuilder> builder) {
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &operationBuilder = peekBuilders()[operationId];
    std::call_once(operationBuilder.second, [] {});
    operationBuilder.first.swap(builder);
    return builder;
}

void MockClDevice::setPciUuid(std::array<uint8_t, ProductHelper::uuidSize> &id) {
    memcpy_s(device.uuid.id.data(), ProductHelper::uuidSize, id.data(), ProductHelper::uuidSize);
    device.uuid.isValid = true;
}
