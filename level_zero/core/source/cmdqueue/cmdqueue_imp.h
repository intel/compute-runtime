/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"

#include <vector>

namespace NEO {
class LinearStream;
class GraphicsAllocation;
class MemoryManager;

} // namespace NEO

namespace L0 {
struct CommandList;
struct Kernel;
struct CommandQueueImp : public CommandQueue {
    class CommandBufferManager {
      public:
        enum BUFFER_ALLOCATION : uint32_t {
            FIRST = 0,
            SECOND,
            COUNT
        };

        ze_result_t initialize(Device *device, size_t sizeRequested);
        void destroy(NEO::MemoryManager *memoryManager);
        void switchBuffers(NEO::CommandStreamReceiver *csr);

        NEO::GraphicsAllocation *getCurrentBufferAllocation() {
            return buffers[bufferUse];
        }

        void setCurrentFlushStamp(NEO::FlushStamp flushStamp) {
            flushId[bufferUse] = flushStamp;
        }

      private:
        NEO::GraphicsAllocation *buffers[BUFFER_ALLOCATION::COUNT];
        NEO::FlushStamp flushId[BUFFER_ALLOCATION::COUNT];
        BUFFER_ALLOCATION bufferUse = BUFFER_ALLOCATION::FIRST;
    };
    static constexpr size_t defaultQueueCmdBufferSize = 128 * MemoryConstants::kiloByte;
    static constexpr size_t minCmdBufferPtrAlign = 8;
    static constexpr size_t totalCmdBufferSize =
        defaultQueueCmdBufferSize +
        MemoryConstants::cacheLineSize +
        NEO::CSRequirements::csOverfetchSize;

    CommandQueueImp() = delete;
    CommandQueueImp(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
        : device(device), csr(csr), desc(*desc) {
    }

    ze_result_t destroy() override;

    ze_result_t synchronize(uint64_t timeout) override;

    ze_result_t initialize(bool copyOnly, bool isInternal);

    Device *getDevice() { return device; }

    uint32_t getTaskCount() { return taskCount; }

    NEO::CommandStreamReceiver *getCsr() { return csr; }

    void reserveLinearStreamSize(size_t size);
    ze_command_queue_mode_t getSynchronousMode();
    virtual void dispatchTaskCountWrite(NEO::LinearStream &commandStream, bool flushDataCache) = 0;

  protected:
    MOCKABLE_VIRTUAL void submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr);

    ze_result_t synchronizeByPollingForTaskCount(uint64_t timeout);

    void printFunctionsPrintfOutput();

    Device *device = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    const ze_command_queue_desc_t desc;
    NEO::LinearStream *commandStream = nullptr;
    std::atomic<uint32_t> taskCount{0};
    std::vector<Kernel *> printfFunctionContainer;
    bool gpgpuEnabled = false;
    CommandBufferManager buffers;
    NEO::ResidencyContainer residencyContainer;
    NEO::HeapContainer heapContainer;
};

} // namespace L0
