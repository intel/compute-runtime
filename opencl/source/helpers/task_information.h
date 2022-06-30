/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/utilities/iflist.h"

#include "opencl/source/helpers/properties_helper.h"

#include <memory>
#include <vector>

namespace NEO {
class CommandQueue;
class CommandStreamReceiver;
class InternalAllocationStorage;
class Kernel;
class MemObj;
class Surface;
class PrintfHandler;
class HwTimeStamps;
class TimestampPacketContainer;
template <class T>
class TagNode;

enum MapOperationType {
    MAP,
    UNMAP
};

struct KernelOperation {
  protected:
    struct ResourceCleaner {
        ResourceCleaner() = delete;
        ResourceCleaner(InternalAllocationStorage *storageForAllocations) : storageForAllocations(storageForAllocations){};

        template <typename ObjectT>
        void operator()(ObjectT *object);

        InternalAllocationStorage *storageForAllocations = nullptr;
    } resourceCleaner{nullptr};

    using LinearStreamUniquePtrT = std::unique_ptr<LinearStream, ResourceCleaner>;
    using IndirectHeapUniquePtrT = std::unique_ptr<IndirectHeap, ResourceCleaner>;

  public:
    KernelOperation() = delete;
    KernelOperation(LinearStream *commandStream, InternalAllocationStorage &storageForAllocations) {
        resourceCleaner.storageForAllocations = &storageForAllocations;
        this->commandStream = LinearStreamUniquePtrT(commandStream, resourceCleaner);
    }

    void setHeaps(IndirectHeap *dsh, IndirectHeap *ioh, IndirectHeap *ssh) {
        this->dsh = IndirectHeapUniquePtrT(dsh, resourceCleaner);
        this->ioh = IndirectHeapUniquePtrT(ioh, resourceCleaner);
        this->ssh = IndirectHeapUniquePtrT(ssh, resourceCleaner);
    }

    ~KernelOperation() {
        if (ioh.get() == dsh.get()) {
            ioh.release();
        }
    }

    LinearStreamUniquePtrT commandStream{nullptr, resourceCleaner};
    IndirectHeapUniquePtrT dsh{nullptr, resourceCleaner};
    IndirectHeapUniquePtrT ioh{nullptr, resourceCleaner};
    IndirectHeapUniquePtrT ssh{nullptr, resourceCleaner};

    CommandStreamReceiver *bcsCsr = nullptr;
    BlitPropertiesContainer blitPropertiesContainer;
    bool blitEnqueue = false;
    size_t surfaceStateHeapSizeEM = 0;
};

class Command : public IFNode<Command> {
  public:
    // returns command's taskCount obtained from completion stamp
    //   as acquired from command stream receiver
    virtual CompletionStamp &submit(uint32_t taskLevel, bool terminated) = 0;

    Command() = delete;
    Command(CommandQueue &commandQueue);
    Command(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation);

    virtual ~Command();
    virtual LinearStream *getCommandStream() {
        return nullptr;
    }
    void setTimestampPacketNode(TimestampPacketContainer &current, TimestampPacketDependencies &&dependencies);
    void setEventsRequest(EventsRequest &eventsRequest);
    void makeTimestampPacketsResident(CommandStreamReceiver &commandStreamReceiver);

    TagNodeBase *timestamp = nullptr;
    CompletionStamp completionStamp = {};

  protected:
    bool terminated = false;
    CommandQueue &commandQueue;
    std::unique_ptr<KernelOperation> kernelOperation;
    std::unique_ptr<TimestampPacketContainer> currentTimestampPacketNodes;
    std::unique_ptr<TimestampPacketDependencies> timestampPacketDependencies;
    EventsRequest eventsRequest = {0, nullptr, nullptr};
    std::vector<cl_event> eventsWaitlist;
};

class CommandMapUnmap : public Command {
  public:
    CommandMapUnmap(MapOperationType operationType, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                    CommandQueue &commandQueue);
    ~CommandMapUnmap() override = default;
    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;

  private:
    MemObj &memObj;
    MemObjSizeArray copySize;
    MemObjOffsetArray copyOffset;
    bool readOnly;
    MapOperationType operationType;
};

class CommandComputeKernel : public Command {
  public:
    CommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> surfaces,
                         bool flushDC, bool usesSLM, uint32_t commandType, std::unique_ptr<PrintfHandler> &&printfHandler,
                         PreemptionMode preemptionMode, Kernel *kernel, uint32_t kernelCount);

    ~CommandComputeKernel() override;

    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;

    LinearStream *getCommandStream() override { return kernelOperation->commandStream.get(); }
    Kernel *peekKernel() const { return kernel; }
    PrintfHandler *peekPrintfHandler() const { return printfHandler.get(); }

  protected:
    std::vector<Surface *> surfaces;
    bool flushDC;
    bool slmUsed;
    uint32_t commandType;
    std::unique_ptr<PrintfHandler> printfHandler;
    Kernel *kernel;
    uint32_t kernelCount;
    PreemptionMode preemptionMode;
};

class CommandWithoutKernel : public Command {
  public:
    using Command::Command;
    CompletionStamp &submit(uint32_t taskLevel, bool terminated) override;
    bool dispatchBlitOperation();
};
} // namespace NEO
