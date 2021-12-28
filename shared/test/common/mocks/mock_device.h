/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/root_device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

namespace NEO {
class CommandStreamReceiver;
class DriverInfo;
class OSTime;

template <typename GfxFamily>
class UltCommandStreamReceiver;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

struct MockSubDevice : public SubDevice {
    using Device::allEngines;
    using Device::createEngines;
    using Device::engineInstancedType;
    using SubDevice::engineInstanced;
    using SubDevice::getDeviceBitfield;
    using SubDevice::getGlobalMemorySize;
    using SubDevice::SubDevice;

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
        return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
    }
    static decltype(&createCommandStream) createCommandStreamReceiverFunc;

    bool failOnCreateEngine = false;
    bool createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) override;
};

class MockDevice : public RootDevice {
  public:
    using Device::addEngineToEngineGroup;
    using Device::allEngines;
    using Device::commandStreamReceivers;
    using Device::createDeviceInternals;
    using Device::createEngine;
    using Device::createSubDevices;
    using Device::deviceBitfield;
    using Device::deviceInfo;
    using Device::engineInstanced;
    using Device::engineInstancedType;
    using Device::executionEnvironment;
    using Device::getGlobalMemorySize;
    using Device::initializeCaps;
    using Device::isDebuggerActive;
    using Device::regularEngineGroups;
    using Device::rootCsrCreated;
    using Device::rtMemoryBackedBuffer;
    using RootDevice::createEngines;
    using RootDevice::defaultEngineIndex;
    using RootDevice::getDeviceBitfield;
    using RootDevice::initializeRootCommandStreamReceiver;
    using RootDevice::numSubDevices;
    using RootDevice::subdevices;

    void setOSTime(OSTime *osTime);
    void setDriverInfo(DriverInfo *driverInfo);

    static bool createSingleDevice;
    bool createDeviceImpl() override;

    bool getCpuTime(uint64_t *timeStamp) { return true; };
    MockDevice();
    MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex);

    void setPreemptionMode(PreemptionMode mode) {
        preemptionMode = mode;
    }

    void injectMemoryManager(MemoryManager *);

    void setPerfCounters(PerformanceCounters *perfCounters) {
        if (perfCounters) {
            performanceCounters = std::unique_ptr<PerformanceCounters>(perfCounters);
        } else {
            performanceCounters.release();
        }
    }

    size_t getMaxParameterSizeFromIGC() const override {
        if (callBaseGetMaxParameterSizeFromIGC) {
            return Device::getMaxParameterSizeFromIGC();
        }
        return maxParameterSizeFromIGC;
    }

    const char *getProductAbbrev() const;

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(*allEngines[defaultEngineIndex].commandStreamReceiver);
    }

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiverFromIndex(uint32_t index) {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(*allEngines[index].commandStreamReceiver);
    }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const { return *allEngines[defaultEngineIndex].commandStreamReceiver; }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex);

    void setDebuggerActive(bool active) {
        this->deviceInfo.debuggerActive = active;
    }

    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(pHwInfo);
        T *device = new T(executionEnvironment, rootDeviceIndex);
        return createDeviceInternals(device);
    }

    static ExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
        ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
        auto numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get() ? DebugManager.flags.CreateMultipleRootDevices.get() : rootDeviceIndex + 1;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(pHwInfo);
        }
        executionEnvironment->calculateMaxOsContextCount();
        return executionEnvironment;
    }

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        auto executionEnvironment = prepareExecutionEnvironment(pHwInfo, rootDeviceIndex);
        return createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }

    SubDevice *createSubDevice(uint32_t subDeviceIndex) override {
        return Device::create<MockSubDevice>(executionEnvironment, subDeviceIndex, *this);
    }

    SubDevice *createEngineInstancedSubDevice(uint32_t subDeviceIndex, aub_stream::EngineType engineType) override {
        return Device::create<MockSubDevice>(executionEnvironment, subDeviceIndex, *this, engineType);
    }

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
        return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
    }

    bool isDebuggerActive() const override {
        if (isDebuggerActiveParentCall) {
            return Device::isDebuggerActive();
        }
        return isDebuggerActiveReturn;
    }

    void allocateRTDispatchGlobals(uint32_t maxBvhLevels) override {
        if (rtDispatchGlobalsForceAllocation == true) {
            rtDispatchGlobals[maxBvhLevels] = new MockGraphicsAllocation();
        } else {
            Device::allocateRTDispatchGlobals(maxBvhLevels);
        }
    }

    void setRTDispatchGlobalsForceAllocation() {
        rtDispatchGlobalsForceAllocation = true;
    }

    static decltype(&createCommandStream) createCommandStreamReceiverFunc;

    bool isDebuggerActiveParentCall = true;
    bool isDebuggerActiveReturn = false;
    bool rtDispatchGlobalsForceAllocation = false;
    bool callBaseGetMaxParameterSizeFromIGC = false;
    size_t maxParameterSizeFromIGC = 0u;
};

template <>
inline Device *MockDevice::createWithNewExecutionEnvironment<Device>(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
    auto executionEnvironment = new ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto hwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hwInfo);

    MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->initializeMemoryManager();
    return Device::create<RootDevice>(executionEnvironment, 0u);
}

class FailDevice : public MockDevice {
  public:
    FailDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class FailDeviceAfterOne : public MockDevice {
  public:
    FailDeviceAfterOne(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class MockAlignedMallocManagerDevice : public MockDevice {
  public:
    MockAlignedMallocManagerDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

struct EnvironmentWithCsrWrapper {
    template <typename CsrType>
    void setCsrType() {
        createSubDeviceCsrFuncBackup = EnvironmentWithCsrWrapper::createCommandStreamReceiver<CsrType>;
        createRootDeviceCsrFuncBackup = EnvironmentWithCsrWrapper::createCommandStreamReceiver<CsrType>;
    }

    template <typename CsrType>
    static CommandStreamReceiver *createCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                                              uint32_t rootDeviceIndex,
                                                              const DeviceBitfield deviceBitfield) {
        return new CsrType(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    VariableBackup<decltype(MockSubDevice::createCommandStreamReceiverFunc)> createSubDeviceCsrFuncBackup{&MockSubDevice::createCommandStreamReceiverFunc};
    VariableBackup<decltype(MockDevice::createCommandStreamReceiverFunc)> createRootDeviceCsrFuncBackup{&MockDevice::createCommandStreamReceiverFunc};
};
} // namespace NEO
