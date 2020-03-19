/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/memory_manager/memory_constants.h"

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

        void initialize(Device *device, size_t sizeRequested);
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

    CommandQueueImp(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
        : device(device), csr(csr), desc(*desc) {
        std::atomic_init(&commandQueuePerThreadScratchSize, 0u);
    }

    ze_result_t destroy() override;

    ze_result_t synchronize(uint32_t timeout) override;

    void initialize();

    Device *getDevice() { return device; }

    uint32_t getTaskCount() { return taskCount; }

    NEO::CommandStreamReceiver *getCsr() { return csr; }

    void reserveLinearStreamSize(size_t size);
    ze_command_queue_mode_t getSynchronousMode();
    virtual void dispatchTaskCountWrite(NEO::LinearStream &commandStream, bool flushDataCache) = 0;

  protected:
    void submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr);

    ze_result_t synchronizeByPollingForTaskCount(uint32_t timeout);

    void printFunctionsPrintfOutput();

    Device *device = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    const ze_command_queue_desc_t desc;
    NEO::LinearStream *commandStream = nullptr;
    uint32_t taskCount = 0;
    std::vector<Kernel *> printfFunctionContainer;
    bool gsbaInit = false;
    bool frontEndInit = false;
    bool gpgpuEnabled = false;
    CommandBufferManager buffers;
};

} // namespace L0
