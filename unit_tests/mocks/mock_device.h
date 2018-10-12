/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device/device.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

namespace OCLRT {
class OSTime;
class FailMemoryManager;

extern CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment);

class MockDevice : public Device {
  public:
    using Device::createDeviceImpl;
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
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(getCommandStreamReceiver());
    }

    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);

    GraphicsAllocation *getTagAllocation() { return this->getCommandStreamReceiver().getTagAllocation(); }

    void setSourceLevelDebuggerActive(bool active) {
        this->deviceInfo.sourceLevelDebuggerActive = active;
    }

    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) {
        pHwInfo = getDeviceInitHwInfo(pHwInfo);
        T *device = new T(*pHwInfo, executionEnvironment, deviceIndex);
        executionEnvironment->memoryManager = std::move(device->mockMemoryManager);
        return createDeviceInternals(pHwInfo, device);
    }

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo) {
        return createWithExecutionEnvironment<T>(pHwInfo, new ExecutionEnvironment(), 0u);
    }

    void allocatePreemptionAllocationIfNotPresent() {
        if (this->preemptionAllocation == nullptr) {
            if (preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) {
                size_t requiredSize = hwInfo.capabilityTable.requiredPreemptionSurfaceSize;
                size_t alignment = 256 * MemoryConstants::kiloByte;
                bool uncacheable = getWaTable()->waCSRUncachable;
                this->preemptionAllocation = executionEnvironment->memoryManager->allocateGraphicsMemory(requiredSize, alignment, false, uncacheable);
                this->commandStreamReceiver->setPreemptionCsrAllocation(preemptionAllocation);
            }
        }
    }
    std::unique_ptr<MemoryManager> mockMemoryManager;

    void setHWCapsLocalMemorySupported(bool localMemorySupported);

  private:
    bool forceWhitelistedRegs = false;
    WhitelistedRegisters mockWhitelistedRegs = {0};
    WorkaroundTable mockWaTable = {};
};

template <>
inline Device *MockDevice::createWithNewExecutionEnvironment<Device>(const HardwareInfo *pHwInfo) {
    return Device::create<Device>(pHwInfo, new ExecutionEnvironment, 0u);
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

} // namespace OCLRT
