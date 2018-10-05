/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/linear_stream.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/utilities/iflist.h"
#include "runtime/helpers/completion_stamp.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/properties_helper.h"

#include <memory>
#include <vector>

namespace OCLRT {
class CommandQueue;
class CommandStreamReceiver;
class Kernel;
class MemObj;
class Surface;
class PrintfHandler;
struct HwTimeStamps;
class MemoryManager;
class TimestampPacketContainer;

enum MapOperationType {
    MAP,
    UNMAP
};

class Command : public IFNode<Command> {
  public:
    // returns command's taskCount obtained from completion stamp
    //   as acquired from command stream receiver
    virtual CompletionStamp &submit(uint32_t taskLevel, bool terminated) = 0;

    virtual ~Command() = default;
    virtual LinearStream *getCommandStream() {
        return nullptr;
    }
    HwTimeStamps *timestamp = nullptr;
    CompletionStamp completionStamp = {};
};

class CommandMapUnmap : public Command {
  public:
    CommandMapUnmap(MapOperationType op, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                    CommandStreamReceiver &csr, CommandQueue &cmdQ);
    ~CommandMapUnmap() override;
    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;

  private:
    MemObj &memObj;
    MemObjSizeArray copySize;
    MemObjOffsetArray copyOffset;
    bool readOnly;
    CommandStreamReceiver &csr;
    CommandQueue &cmdQ;
    MapOperationType op;
};

struct KernelOperation {
    KernelOperation(std::unique_ptr<LinearStream> commandStream, std::unique_ptr<IndirectHeap> dsh, std::unique_ptr<IndirectHeap> ioh, std::unique_ptr<IndirectHeap> ssh,
                    MemoryManager &memoryManager)
        : commandStream(std::move(commandStream)), dsh(std::move(dsh)),
          ioh(std::move(ioh)), ssh(std::move(ssh)),
          surfaceStateHeapSizeEM(0), doNotFreeISH(false), memoryManager(memoryManager) {
    }

    ~KernelOperation();

    std::unique_ptr<LinearStream> commandStream;
    std::unique_ptr<IndirectHeap> dsh;
    std::unique_ptr<IndirectHeap> ioh;
    std::unique_ptr<IndirectHeap> ssh;

    size_t surfaceStateHeapSizeEM;
    bool doNotFreeISH;
    MemoryManager &memoryManager;
};

class CommandComputeKernel : public Command {
  public:
    CommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> kernelResources, std::vector<Surface *> &surfaces,
                         bool flushDC, bool usesSLM, bool ndRangeKernel, std::unique_ptr<PrintfHandler> printfHandler,
                         PreemptionMode preemptionMode, Kernel *kernel = nullptr, uint32_t kernelCount = 0);

    ~CommandComputeKernel() override;

    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;

    LinearStream *getCommandStream() override { return kernelOperation->commandStream.get(); }

    void setTimestampPacketNode(TimestampPacketContainer &current, TimestampPacketContainer &previous);
    void setEventsRequest(EventsRequest &eventsRequest) { this->eventsRequest = eventsRequest; }

  private:
    CommandQueue &commandQueue;
    std::unique_ptr<KernelOperation> kernelOperation;
    std::vector<Surface *> surfaces;
    bool flushDC;
    bool slmUsed;
    bool NDRangeKernel;
    std::unique_ptr<PrintfHandler> printfHandler;
    Kernel *kernel;
    uint32_t kernelCount;
    PreemptionMode preemptionMode;
    std::unique_ptr<TimestampPacketContainer> currentTimestampPacketNodes;
    std::unique_ptr<TimestampPacketContainer> previousTimestampPacketNodes;
    EventsRequest eventsRequest = {0, nullptr, nullptr};
};

class CommandMarker : public Command {
  public:
    CommandMarker(CommandQueue &cmdQ, CommandStreamReceiver &csr, uint32_t clCommandType, uint32_t commandSize)
        : cmdQ(cmdQ), csr(csr), clCommandType(clCommandType), commandSize(commandSize) {
    }

    void setTimestampPacketsForPipeControlWrite(TimestampPacketContainer &inputNodes);
    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;

  private:
    std::unique_ptr<TimestampPacketContainer> timestampPacketsForPipeControlWrite;
    CommandQueue &cmdQ;
    CommandStreamReceiver &csr;
    uint32_t clCommandType;
    uint32_t commandSize;
};
} // namespace OCLRT
