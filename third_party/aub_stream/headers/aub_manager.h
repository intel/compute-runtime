/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "page_info.h"
#include "shared_mem_info.h"
#include "physical_allocation_info.h"

namespace aub_stream {

struct AllocationParams;
struct HardwareContext;

struct AubManagerOptions {
    uint32_t productFamily{};
    uint32_t devicesCount{};
    uint64_t memoryBankSize{};
    uint32_t stepping{};
    bool localMemorySupported{};
    uint32_t mode{};
    uint64_t gpuAddressSpace{};
    SharedMemoryInfo sharedMemoryInfo{};
    bool throwOnError{};
};

class AubManager {
  public:
    virtual ~AubManager() = default;

    virtual HardwareContext *createHardwareContext(uint32_t device, uint32_t engine, uint32_t flags) = 0;

    virtual void open(const std::string &aubFileName) = 0;
    virtual void close() = 0;
    virtual bool isOpen() = 0;
    virtual const std::string getFileName() = 0;
    virtual void pause(bool onoff) = 0;

    virtual void addComment(const char *message) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) = 0;
    virtual void writePageTableEntries(uint64_t gfxAddress, size_t size, uint32_t memoryBanks, int hint,
                                       std::vector<PageInfo> &lastLevelPages, size_t pageSize) = 0;

    virtual void writePhysicalMemoryPages(const void *memory, std::vector<PageInfo> &pages, size_t size, int hint) = 0;
    virtual void freeMemory(uint64_t gfxAddress, size_t size) = 0;

    static AubManager *create(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, uint32_t stepping,
                              bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace);

    virtual void writeMemory2(AllocationParams allocationParams) = 0;
    static AubManager *create(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, uint32_t stepping,
                              bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace, SharedMemoryInfo sharedMemoryInfo);

    static AubManager *create(const struct AubManagerOptions &options);

    virtual bool reservePhysicalMemory(AllocationParams allocationParams, PhysicalAllocationInfo &physicalAllocInfo) = 0;
    virtual bool mapGpuVa(uint64_t gfxAddress, size_t size, PhysicalAllocationInfo physicalAllocInfo) = 0;
};

} // namespace aub_stream
