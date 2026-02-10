/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/performance_counters.h"
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

bool MockClDevice::createEngines() { return device.createEngines(); }
void MockClDevice::setOSTime(OSTime *osTime) { device.setOSTime(osTime); }
bool MockClDevice::getCpuTime(uint64_t *timeStamp) { return device.getCpuTime(timeStamp); }
void MockClDevice::setPreemptionMode(PreemptionMode mode) { device.setPreemptionMode(mode); }
void MockClDevice::injectMemoryManager(MemoryManager *pMemoryManager) { device.injectMemoryManager(pMemoryManager); }
void MockClDevice::setPerfCounters(std::unique_ptr<PerformanceCounters> perfCounters) { device.setPerfCounters(std::move(perfCounters)); }
const char *MockClDevice::getProductAbbrev() const { return device.getProductAbbrev(); }
CommandStreamReceiver &MockClDevice::getGpgpuCommandStreamReceiver() const { return device.getGpgpuCommandStreamReceiver(); }
void MockClDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr) { device.resetCommandStreamReceiver(newCsr); }
void MockClDevice::resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) { device.resetCommandStreamReceiver(newCsr, engineIndex); }

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

SubDevice *MockClDevice::createSubDevice(uint32_t subDeviceIndex) { return device.createSubDevice(subDeviceIndex); }

std::unique_ptr<CommandStreamReceiver> MockClDevice::createCommandStreamReceiver() const {
    return device.createCommandStreamReceiver();
}

BuiltIns *MockClDevice::getBuiltIns() const { return getDevice().getBuiltIns(); }

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
