/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <cstring>
#include <utility>
#include <vector>

namespace NEO {
namespace PagaFaultManagerTestConfig {
extern bool disabled;
}
} // namespace NEO

namespace L0 {
namespace ult {

enum class AubMemoryType {
    hostUsm,
    deviceUsm,
    sharedUsm,
    system,
    importedSystem
};

struct MemoryAubFixture : public AUBFixtureL0 {
    void tearDown() {
        for (auto &[ptr, type] : this->localAllocations) {
            if (type == AubMemoryType::sharedUsm) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemFree(this->context->toHandle(), ptr));
            } else {
                if (type == AubMemoryType::importedSystem) {
                    EXPECT_EQ(ZE_RESULT_SUCCESS, this->driverHandle->releaseImportedPointer(ptr));
                }
                alignedFree(ptr);
            }
        }
        this->localAllocations.clear();
        AUBFixtureL0::tearDown();
    }

    bool isPageFaultManagerAvailable() const {
        return !NEO::PagaFaultManagerTestConfig::disabled && this->neoDevice->getMemoryManager()->getPageFaultManager() != nullptr;
    }

    void *allocate(AubMemoryType type, size_t size) {
        void *ptr = nullptr;
        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        switch (type) {
        case AubMemoryType::hostUsm:
            return this->allocateHostMemory(size, 0u);
        case AubMemoryType::deviceUsm:
            return this->allocateDeviceMemory(size, 0u);
        case AubMemoryType::sharedUsm:
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocShared(this->context->toHandle(), &deviceDesc, &hostDesc, size, 0u, this->device->toHandle(), &ptr));
            break;
        case AubMemoryType::system:
            ptr = alignedMalloc(size, MemoryConstants::pageSize);
            break;
        case AubMemoryType::importedSystem:
            ptr = alignedMalloc(size, MemoryConstants::pageSize);
            EXPECT_EQ(ZE_RESULT_SUCCESS, this->driverHandle->importExternalPointer(ptr, size));
            break;
        }
        EXPECT_NE(nullptr, ptr);
        this->localAllocations.emplace_back(ptr, type);
        return ptr;
    }

    void appendCopy(void *dst, const void *src, size_t size) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(this->commandList->toHandle(), dst, src, size, nullptr, 0, nullptr));
    }

    void appendBarrier() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(this->commandList->toHandle(), nullptr, 0, nullptr));
    }

    void initialize(AubMemoryType type, void *dst, const void *src, size_t size) {
        if (type == AubMemoryType::deviceUsm) {
            this->appendCopy(dst, src, size);
            this->appendBarrier();
        } else {
            memcpy(dst, src, size);
        }
    }

    void *getGpuPointer(void *ptr) {
        NEO::SvmAllocationData *allocData = nullptr;
        if (this->driverHandle->findAllocationDataForRange(ptr, 1, allocData)) {
            return ptr;
        }
        auto importedAllocation = this->driverHandle->findHostPointerAllocation(ptr, 1, this->rootDeviceIndex);
        if (importedAllocation) {
            return toGpuPointer(*importedAllocation, ptr);
        }
        for (auto &hostPtrEntry : this->commandList->getHostPtrMap()) {
            auto allocation = hostPtrEntry.second;
            auto cpuBase = reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer());
            auto address = reinterpret_cast<uintptr_t>(ptr);
            if (address >= cpuBase && address < cpuBase + allocation->getUnderlyingBufferSize()) {
                return toGpuPointer(*allocation, ptr);
            }
        }
        ADD_FAILURE() << "no allocation found for " << ptr;
        return ptr;
    }

    static void *toGpuPointer(NEO::GraphicsAllocation &allocation, const void *ptr) {
        return reinterpret_cast<void *>(allocation.getGpuAddress() + ptrDiff(ptr, allocation.getUnderlyingBuffer()));
    }

    std::vector<std::pair<void *, AubMemoryType>> localAllocations;
};

template <typename ParamType>
struct MemoryAubParamTest : public MemoryAubFixture,
                            public ::testing::TestWithParam<ParamType> {
    void SetUp() override {
        MemoryAubFixture::setUp();
    }

    void TearDown() override {
        MemoryAubFixture::tearDown();
    }
};

} // namespace ult
} // namespace L0
