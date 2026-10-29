/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/helpers/options.h"
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/source/os_interface/mapped_file.h"
#include "shared/source/utilities/mockable_virtual_defaults.h"

#include "aubstream/aub_manager.h"

#include <mutex>

namespace NEO {
struct RootDeviceEnvironment;
class TbxInProcessSimulator;

class AubCenter {
  public:
    AubCenter(const RootDeviceEnvironment &rootDeviceEnvironment, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);

    AubCenter();
    virtual ~AubCenter();

    void initPhysicalAddressAllocator(PhysicalAddressAllocator *pPhysicalAddressAllocator) {
        physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(pPhysicalAddressAllocator);
    }

    PhysicalAddressAllocator *getPhysicalAddressAllocator() const {
        return physicalAddressAllocator.get();
    }

    AubSubCaptureCommon *getSubCaptureCommon() const {
        return subCaptureCommon.get();
    }

    aub_stream::AubManager *getAubManager() const {
        return aubManager.get();
    }

    MOCKABLE_VIRTUAL std::unique_lock<std::mutex> obtainPageTablesLock() {
        return std::unique_lock<std::mutex>(pageTablesMutex);
    }

    static uint32_t getAubStreamMode(const std::string &aubFileName, CommandStreamReceiverType csrType);
    static uint32_t getSharedMemoryAubStreamMode(uint32_t aubStreamMode, bool translationTableMode);
    bool isAllocationMemorySharedWithSimulator() const;

    void addImplicitArgsInfoToAubComments(uint32_t implicitArgsVersion);

  protected:
    bool enableTbxSharedMemory(const RootDeviceEnvironment &rootDeviceEnvironment, bool inProcessSimulatorRequested, aub_stream::AubManagerOptions &options);
    bool mapTbxSharedMemory(const std::string &systemMemoryFile, const std::string &localMemoryFile, aub_stream::SharedMemoryInfo &sharedMemoryInfo);
    bool isTbxSharedMemorySizeSufficient(bool translationTableMode, const aub_stream::AubManagerOptions &options) const;
    void unmapTbxSharedMemory(aub_stream::SharedMemoryInfo &sharedMemoryInfo);

    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<MappedFile> sharedSystemMemory;
    std::unique_ptr<MappedFile> sharedLocalMemory;
    std::unique_ptr<TbxInProcessSimulator> inProcessSimulator;

    std::unique_ptr<AubSubCaptureCommon> subCaptureCommon;
    std::unique_ptr<aub_stream::AubManager> aubManager;
    std::mutex pageTablesMutex;
    uint32_t aubStreamMode = 0;
    uint32_t stepping = 0;
    std::once_flag addImplicitArgsInfoOnceFlag;
};
} // namespace NEO
