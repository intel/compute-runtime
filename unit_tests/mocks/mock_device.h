/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace OCLRT {
class OSTime;
class MemoryManager;
class MockMemoryManager;

class MockDevice : public Device {
  public:
    using Device::commandStreamReceiver;
    using Device::createDeviceImpl;
    using Device::initializeCaps;
    using Device::sourceLevelDebugger;

    void setOSTime(OSTime *osTime);
    void setDriverInfo(DriverInfo *driverInfo);
    bool hasDriverInfo();

    bool getCpuTime(uint64_t *timeStamp) { return true; };
    void *peekSlmWindowStartAddress() const {
        return this->slmWindowStartAddress;
    }
    MockDevice(const HardwareInfo &hwInfo);

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

    void injectMemoryManager(MockMemoryManager *);

    void setPerfCounters(PerformanceCounters *perfCounters) {
        performanceCounters = std::unique_ptr<PerformanceCounters>(perfCounters);
    }
    void setMemoryManager(MemoryManager *memoryManager);

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(getCommandStreamReceiver());
    }

    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);

    GraphicsAllocation *getTagAllocation() { return tagAllocation; }

    void setSourceLevelDebuggerActive(bool active) {
        this->deviceInfo.sourceLevelDebuggerActive = active;
    }
    template <typename T>
    static T *createWithMemoryManager(const HardwareInfo *pHwInfo,
                                      MemoryManager *memManager) {
        pHwInfo = getDeviceInitHwInfo(pHwInfo);
        T *device = new T(*pHwInfo);
        device->connectToExecutionEnvironment(new ExecutionEnvironment);
        if (memManager) {
            device->setMemoryManager(memManager);
        }
        if (false == createDeviceImpl(pHwInfo, *device)) {
            delete device;
            return nullptr;
        }
        return device;
    }
    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo) {
        return Device::create<T>(pHwInfo, new ExecutionEnvironment);
    }

    void allocatePreemptionAllocationIfNotPresent() {
        if (this->preemptionAllocation == nullptr) {
            if (preemptionMode == PreemptionMode::MidThread || isSourceLevelDebuggerActive()) {
                size_t requiredSize = hwInfo.capabilityTable.requiredPreemptionSurfaceSize;
                size_t alignment = 256 * MemoryConstants::kiloByte;
                bool uncacheable = getWaTable()->waCSRUncachable;
                this->preemptionAllocation = memoryManager->allocateGraphicsMemory(requiredSize, alignment, false, uncacheable);

                commandStreamReceiver->setPreemptionCsrAllocation(preemptionAllocation);
            }
        }
    }

  private:
    bool forceWhitelistedRegs = false;
    WhitelistedRegisters mockWhitelistedRegs = {0};
    WorkaroundTable mockWaTable = {};
};

class FailMemoryManager : public MockMemoryManager {
  public:
    FailMemoryManager();
    FailMemoryManager(int32_t fail);
    virtual ~FailMemoryManager() override {
        if (agnostic) {
            for (auto alloc : allocations) {
                agnostic->freeGraphicsMemory(alloc);
            }
            delete agnostic;
        }
    };
    GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override {
        if (fail <= 0) {
            return nullptr;
        }
        fail--;
        GraphicsAllocation *alloc = agnostic->allocateGraphicsMemory(size, alignment, forcePin, uncacheable);
        allocations.push_back(alloc);
        return alloc;
    };
    GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr) override {
        return nullptr;
    };
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, void *ptr, AllocationOrigin allocationOrigin) override {
        return nullptr;
    };
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) override {
        return nullptr;
    };
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override {
        return nullptr;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override{};
    void *lockResource(GraphicsAllocation *gfxAllocation) override { return nullptr; };
    void unlockResource(GraphicsAllocation *gfxAllocation) override{};

    MemoryManager::AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override {
        return AllocationStatus::Error;
    };
    void cleanOsHandles(OsHandleStorage &handleStorage) override{};

    uint64_t getSystemSharedMemory() override {
        return 0;
    };

    uint64_t getMaxApplicationAddress() override {
        return MemoryConstants::max32BitAppAddress;
    };

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
        return nullptr;
    }
    int32_t fail;
    OsAgnosticMemoryManager *agnostic;
    std::vector<GraphicsAllocation *> allocations;
};

class FailDevice : public Device {
  public:
    FailDevice(const HardwareInfo &hwInfo)
        : Device(hwInfo) {
        memoryManager = new FailMemoryManager;
    }
};

class FailDeviceAfterOne : public Device {
  public:
    FailDeviceAfterOne(const HardwareInfo &hwInfo)
        : Device(hwInfo) {
        memoryManager = new FailMemoryManager(1);
    }
};

class MockAlignedMallocManagerDevice : public MockDevice {
  public:
    MockAlignedMallocManagerDevice(const HardwareInfo &hwInfo);
};

template <typename T = SourceLevelDebugger>
class MockDeviceWithSourceLevelDebugger : public MockDevice {
  public:
    MockDeviceWithSourceLevelDebugger(const HardwareInfo &hwInfo) : MockDevice(hwInfo) {
        T *sourceLevelDebuggerCreated = new T(nullptr);
        sourceLevelDebugger.reset(sourceLevelDebuggerCreated);
    }
};

} // namespace OCLRT
