/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device/device.h"
#include "runtime/helpers/hw_helper.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_allocation_properties.h"

namespace NEO {
class OSTime;
class FailMemoryManager;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment);

class MockDevice : public Device {
  public:
    using Device::engines;
    using Device::executionEnvironment;
    using Device::initializeCaps;

    void setOSTime(OSTime *osTime);
    void setDriverInfo(DriverInfo *driverInfo);
    bool hasDriverInfo();

    bool getCpuTime(uint64_t *timeStamp) { return true; };
    void *peekSlmWindowStartAddress() const {
        return this->slmWindowStartAddress;
    }
    MockDevice(const HardwareInfo &hwInfo);
    MockDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);

    DeviceInfo *getDeviceInfoToModify() {
        return &this->deviceInfo;
    }

    void initializeCaps() override {
        Device::initializeCaps();
    }

    void setPreemptionMode(PreemptionMode mode) {
        preemptionMode = mode;
    }

    const WhitelistedRegisters &getWhitelistedRegisters() override {
        if (forceWhitelistedRegs) {
            return mockWhitelistedRegs;
        }
        return Device::getWhitelistedRegisters();
    }

    const WorkaroundTable *getWaTable() const override { return &mockWaTable; }

    void setForceWhitelistedRegs(bool force, WhitelistedRegisters *mockRegs = nullptr) {
        forceWhitelistedRegs = force;
        if (mockRegs) {
            mockWhitelistedRegs = *mockRegs;
        }
    }

    void injectMemoryManager(MemoryManager *);

    void setPerfCounters(PerformanceCounters *perfCounters) {
        performanceCounters = std::unique_ptr<PerformanceCounters>(perfCounters);
    }

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(*engines[defaultEngineIndex].commandStreamReceiver);
    }

    CommandStreamReceiver &getCommandStreamReceiver() const { return *engines[defaultEngineIndex].commandStreamReceiver; }

    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex);

    void setSourceLevelDebuggerActive(bool active) {
        this->deviceInfo.sourceLevelDebuggerActive = active;
    }

    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) {
        pHwInfo = getDeviceInitHwInfo(pHwInfo);
        executionEnvironment->setHwInfo(pHwInfo);
        T *device = new T(*pHwInfo, executionEnvironment, deviceIndex);
        executionEnvironment->memoryManager = std::move(device->mockMemoryManager);
        return createDeviceInternals(pHwInfo, device);
    }

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo) {
        ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->setHwInfo(pHwInfo);
        return createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, 0u);
    }

    void allocatePreemptionAllocationIfNotPresent() {
        if (this->preemptionAllocation == nullptr) {
            if (preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) {
                this->preemptionAllocation = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(getAllocationPropertiesForPreemption());
                this->engines[defaultEngineIndex].commandStreamReceiver->setPreemptionCsrAllocation(preemptionAllocation);
            }
        }
    }
    std::unique_ptr<MemoryManager> mockMemoryManager;

  private:
    bool forceWhitelistedRegs = false;
    WhitelistedRegisters mockWhitelistedRegs = {0};
    WorkaroundTable mockWaTable = {};
};

template <>
inline Device *MockDevice::createWithNewExecutionEnvironment<Device>(const HardwareInfo *pHwInfo) {
    auto executionEnvironment = new ExecutionEnvironment();
    MockAubCenterFixture::setMockAubCenter(executionEnvironment);
    executionEnvironment->setHwInfo(*platformDevices);
    executionEnvironment->initializeMemoryManager();
    return Device::create<Device>(pHwInfo, executionEnvironment, 0u);
}

class FailDevice : public MockDevice {
  public:
    FailDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class FailDeviceAfterOne : public MockDevice {
  public:
    FailDeviceAfterOne(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class MockAlignedMallocManagerDevice : public MockDevice {
  public:
    MockAlignedMallocManagerDevice(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};
} // namespace NEO
