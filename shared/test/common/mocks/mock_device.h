/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/root_device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"

#include "gtest/gtest.h"

namespace NEO {
class CommandStreamReceiver;
class DriverInfo;

template <typename GfxFamily>
class UltCommandStreamReceiver;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

struct MockSubDevice : public SubDevice {
    using Device::allEngines;
    using Device::createEngines;
    using Device::secondaryEngines;
    using SubDevice::getDeviceBitfield;
    using SubDevice::getGlobalMemorySize;
    using SubDevice::SubDevice;

    ~MockSubDevice() override {
        EXPECT_EQ(nullptr, this->getDebugSurface());
    }

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override;
    static decltype(&createCommandStream) createCommandStreamReceiverFunc;

    bool failOnCreateEngine = false;
    bool pollForCompletionCalled = false;

    bool createEngine(EngineTypeUsage engineTypeUsage) override;
    void pollForCompletion() override {
        pollForCompletionCalled = true;
        Device::pollForCompletion();
    }
};

class MockDevice : public RootDevice {
  public:
    using Device::addEngineToEngineGroup;
    using Device::allEngines;
    using Device::allocateDebugSurface;
    using Device::bufferPoolCount;
    using Device::commandStreamReceivers;
    using Device::createDeviceInternals;
    using Device::createEngine;
    using Device::createSubDevices;
    using Device::deviceBitfield;
    using Device::deviceInfo;
    using Device::executionEnvironment;
    using Device::generateUuidFromPciBusInfo;
    using Device::getGlobalMemorySize;
    using Device::initializeCaps;
    using Device::initializePeerAccessForDevices;
    using Device::initUsmReuseLimits;
    using Device::maxBufferPoolCount;
    using Device::microsecondResolution;
    using Device::preemptionMode;
    using Device::regularEngineGroups;
    using Device::rootCsrCreated;
    using Device::rtDispatchGlobalsInfos;
    using Device::rtMemoryBackedBuffer;
    using Device::secondaryCsrs;
    using Device::secondaryEngines;
    using Device::usmMemAllocPool;
    using Device::uuid;
    using RootDevice::createEngines;
    using RootDevice::defaultEngineIndex;
    using RootDevice::getDeviceBitfield;
    using RootDevice::initializeRootCommandStreamReceiver;
    using RootDevice::numSubDevices;
    using RootDevice::subdevices;

    void setOSTime(OSTime *osTime);

    static bool createSingleDevice;
    bool createDeviceImpl() override;

    bool getCpuTime(uint64_t *timeStamp) { return true; };
    MockDevice();
    MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex);

    void setPreemptionMode(PreemptionMode mode) {
        preemptionMode = mode;
    }

    void injectMemoryManager(MemoryManager *);

    void setPerfCounters(std::unique_ptr<PerformanceCounters> perfCounters) {
        performanceCounters = std::move(perfCounters);
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

    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(pHwInfo);
        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

        if (!executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface) {
            executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
        }

        executionEnvironment->calculateMaxOsContextCount();
        T *device = new T(executionEnvironment, rootDeviceIndex);
        return createDeviceInternals(device);
    }

    static ExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex);

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        auto executionEnvironment = prepareExecutionEnvironment(pHwInfo, rootDeviceIndex);
        return createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }

    SubDevice *createSubDevice(uint32_t subDeviceIndex) override {
        return Device::create<MockSubDevice>(executionEnvironment, subDeviceIndex, *this);
    }

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override;

    bool verifyAdapterLuid() override;

    void finalizeRayTracing();

    ReleaseHelper *getReleaseHelper() const override;
    AILConfiguration *getAilConfigurationHelper() const override;

    void setRTDispatchGlobalsForceAllocation() {
        rtDispatchGlobalsForceAllocation = true;
    }

    bool isAnyDirectSubmissionEnabledImpl(bool light) const override {
        return anyDirectSubmissionEnabledReturnValue;
    }

    void stopDirectSubmissionAndWaitForCompletion() override {
        stopDirectSubmissionCalled = true;
        Device::stopDirectSubmissionAndWaitForCompletion();
    }

    void pollForCompletion() override {
        pollForCompletionCalled = true;
        Device::pollForCompletion();
    }

    uint64_t getGlobalMemorySize(uint32_t deviceBitfield) const override {
        if (callBaseGetGlobalMemorySize) {
            return Device::getGlobalMemorySize(deviceBitfield);
        }
        return getGlobalMemorySizeReturn;
    }

    EngineControl *getSecondaryEngineCsr(EngineTypeUsage engineTypeUsage, std::optional<int> priorityLevel, bool allocateInterrupt) override;

    static ExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo);
    static decltype(&createCommandStream) createCommandStreamReceiverFunc;

    bool anyDirectSubmissionEnabledReturnValue = false;
    bool callBaseGetMaxParameterSizeFromIGC = false;
    bool callBaseVerifyAdapterLuid = true;
    bool verifyAdapterLuidReturnValue = true;
    size_t maxParameterSizeFromIGC = 0u;
    bool rtDispatchGlobalsForceAllocation = true;
    bool stopDirectSubmissionCalled = false;
    bool pollForCompletionCalled = false;
    ReleaseHelper *mockReleaseHelper = nullptr;
    AILConfiguration *mockAilConfigurationHelper = nullptr;
    uint64_t getGlobalMemorySizeReturn = 0u;
    bool callBaseGetGlobalMemorySize = true;
    bool disableSecondaryEngines = false;
};

template <>
inline Device *MockDevice::createWithNewExecutionEnvironment<Device>(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(pHwInfo);
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
