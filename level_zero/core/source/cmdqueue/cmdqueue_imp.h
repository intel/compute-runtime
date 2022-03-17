/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/submission_status.h"
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
        void destroy(Device *device);
        void switchBuffers(NEO::CommandStreamReceiver *csr);

        NEO::GraphicsAllocation *getCurrentBufferAllocation() {
            return buffers[bufferUse];
        }

        void setCurrentFlushStamp(uint32_t taskCount, NEO::FlushStamp flushStamp) {
            flushId[bufferUse] = std::make_pair(taskCount, flushStamp);
        }
        std::pair<uint32_t, NEO::FlushStamp> &getCurrentFlushStamp() {
            return flushId[bufferUse];
        }

      private:
        NEO::GraphicsAllocation *buffers[BUFFER_ALLOCATION::COUNT];
        std::pair<uint32_t, NEO::FlushStamp> flushId[BUFFER_ALLOCATION::COUNT];
        BUFFER_ALLOCATION bufferUse = BUFFER_ALLOCATION::FIRST;
    };
    static constexpr size_t defaultQueueCmdBufferSize = 128 * MemoryConstants::kiloByte;
    static constexpr size_t minCmdBufferPtrAlign = 8;
    static constexpr size_t totalCmdBufferSize =
        defaultQueueCmdBufferSize +
        MemoryConstants::cacheLineSize +
        NEO::CSRequirements::csOverfetchSize;

    CommandQueueImp() = delete;
    CommandQueueImp(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc);

    ze_result_t destroy() override;

    ze_result_t synchronize(uint64_t timeout) override;

    ze_result_t initialize(bool copyOnly, bool isInternal);

    Device *getDevice() { return device; }

    uint32_t getTaskCount() { return taskCount; }

    NEO::CommandStreamReceiver *getCsr() { return csr; }

    void reserveLinearStreamSize(size_t size);
    ze_command_queue_mode_t getSynchronousMode() const;
    virtual bool getPreemptionCmdProgramming() = 0;

  protected:
    MOCKABLE_VIRTUAL NEO::SubmissionStatus submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr,
                                                             bool isCooperative);

    ze_result_t synchronizeByPollingForTaskCount(uint64_t timeout);

    void printFunctionsPrintfOutput();

    void postSyncOperations();

    CommandBufferManager buffers;
    NEO::HeapContainer heapContainer;
    ze_command_queue_desc_t desc;
    std::vector<Kernel *> printfFunctionContainer;

    Device *device = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    NEO::LinearStream *commandStream = nullptr;

    std::atomic<uint32_t> taskCount{0};

    bool gpgpuEnabled = false;
    bool useKmdWaitFunction = false;
};

} // namespace L0
