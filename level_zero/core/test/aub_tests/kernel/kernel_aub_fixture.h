/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <cstring>

namespace L0 {
namespace ult {

struct KernelAubFixtureL0 : public AUBFixtureL0 {
    void *allocateDeviceMemoryWithData(const void *data, size_t size) {
        auto staging = this->allocateHostMemory(size, MemoryConstants::pageSize);
        auto deviceMemory = this->allocateDeviceMemory(size, MemoryConstants::pageSize);
        if (staging == nullptr || deviceMemory == nullptr) {
            return nullptr;
        }
        memcpy(staging, data, size);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(this->commandList->toHandle(), deviceMemory, staging, size, nullptr, 0, nullptr));
        return deviceMemory;
    }

    void *allocateDeviceMemoryWithPattern(const void *pattern, size_t patternSize, size_t size) {
        auto deviceMemory = this->allocateDeviceMemory(size, MemoryConstants::pageSize);
        if (deviceMemory != nullptr) {
            this->appendFill(deviceMemory, pattern, patternSize, size);
        }
        return deviceMemory;
    }

    void appendFill(void *ptr, const void *pattern, size_t patternSize, size_t size) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(this->commandList->toHandle(), ptr, pattern, patternSize, size, nullptr, 0, nullptr));
    }

    NEO::GraphicsAllocation *getAllocation(const void *usmPtr) {
        auto allocData = this->driverHandle->svmAllocsManager->getSVMAlloc(usmPtr);
        EXPECT_NE(nullptr, allocData);
        if (allocData == nullptr) {
            return nullptr;
        }
        auto allocation = allocData->gpuAllocations.getGraphicsAllocation(this->rootDeviceIndex);
        EXPECT_NE(nullptr, allocation);
        return allocation;
    }

    void *getGpuPointer(const void *usmPtr) {
        auto allocation = getAllocation(usmPtr);
        if (allocation == nullptr) {
            return nullptr;
        }
        auto offset = ptrDiff(usmPtr, allocation->getUnderlyingBuffer());
        return reinterpret_cast<void *>(allocation->getGpuAddress() + offset);
    }

    template <typename FamilyType>
    void writeHostMemory(const void *usmPtr) {
        auto allocation = getAllocation(usmPtr);
        if (allocation == nullptr) {
            return;
        }
        auto simulatedCsr = getSimulatedCsr<FamilyType>();
        simulatedCsr->initializeEngine();
        simulatedCsr->writeMemory(*allocation);
    }

    void appendLaunch(ze_kernel_handle_t kernel, const uint32_t groupSize[3], const uint32_t groupCount[3]) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, groupSize[0], groupSize[1], groupSize[2]));
        ze_group_count_t dispatchTraits = {groupCount[0], groupCount[1], groupCount[2]};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(this->commandList->toHandle(), kernel, &dispatchTraits, nullptr, 0, nullptr));
    }

    NEO::LinearStream &getCommandListStream() {
        return *this->commandList->getCmdContainer().getCommandStream();
    }

    NEO::LinearStream &getQueueStream() {
        return static_cast<WhiteBox<::L0::CommandQueue> *>(this->pCmdq)->commandStream;
    }

    static L0::Kernel *toKernel(ze_kernel_handle_t kernel) {
        return L0::Kernel::fromHandle(kernel);
    }

    static uint32_t getExpectedEmitLocalId(const NEO::KernelDescriptor &kernelDescriptor) {
        const auto &localId = kernelDescriptor.kernelAttributes.localId;
        uint32_t expectedEmitLocal = 0;
        for (uint32_t dim = 0; dim < 3; dim++) {
            if (localId[dim]) {
                expectedEmitLocal |= (1 << dim);
            }
        }
        return expectedEmitLocal;
    }
};

} // namespace ult
} // namespace L0
