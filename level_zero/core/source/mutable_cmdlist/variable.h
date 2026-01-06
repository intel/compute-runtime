/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/core/source/mutable_cmdlist/variable_handle.h"

#include <memory>
#include <string>

namespace L0::MCL::Program::Decoder {
struct VarInfo;
}
namespace NEO {
struct ArgDescriptor;
struct RootDeviceEnvironment;
class GraphicsAllocation;
class InOrderExecInfo;
} // namespace NEO

namespace L0 {
struct Event;
} // namespace L0

namespace L0::MCL {
struct MutableCommandList;
struct MutableComputeWalker;
struct MutablePipeControl;
struct MutableLoadRegisterImm;
struct MutableSemaphoreWait;
struct MutableStoreDataImm;
struct MutableStoreRegisterMem;
struct VariableDispatch;

struct InterfaceVariableDescriptor {
    const char *name = nullptr;
    size_t size = 0;
    bool isTemporary = false;
    bool isScalable = false;
    bool isConstSize = false;
    bool isStageCommit = false;
    bool immediateValueChunks = false;
    bool api = false;
};

enum VariableType : uint8_t {
    none,
    buffer,
    value,
    groupSize,
    groupCount,
    signalEvent,
    waitEvent,
    globalOffset,
    slmBuffer
};

struct VariableDescriptor {
    enum State : uint8_t {
        declared,
        defined,
        initialized
    };

    std::string name = "";

    size_t eleSize = 0U;
    size_t size = 0U;
    OffsetT offset = 0U;

    GpuAddress bufferGpuAddress = undefined<GpuAddress>;
    NEO::GraphicsAllocation *bufferAlloc = nullptr;
    const void *argValue = reinterpret_cast<void *>(undefined<size_t>);

    VariableType type = VariableType::none;
    State state = State::declared;

    bool isTemporary = false;
    bool isScalable = false;
    bool isStageCommit = false;
    bool commitRequired = false;
    bool immediateValueChunks = false;
    bool apiVariable = false;
};

struct Variable : public VariableHandle {
    static constexpr uint32_t directFlag = 0x1U;
    Variable() = delete;
    Variable(MutableCommandList *mcl);

    static Variable *create(ze_command_list_handle_t hCmdList, const InterfaceVariableDescriptor *ifaceVarDesc);
    static Variable *fromHandle(VariableHandle *handle) { return static_cast<Variable *>(handle); }
    static Variable *createFromInfo(ze_command_list_handle_t hCmdList, Program::Decoder::VarInfo &varInfo);
    inline VariableHandle *toHandle() { return this; }

    // ------API------------
    ze_result_t setValue(size_t size, uint32_t flags, const void *argVal);
    ze_result_t setAsKernelArg(ze_kernel_handle_t hKernel, uint32_t argIndex);
    ze_result_t setAsKernelGroupSize(ze_kernel_handle_t hKernel);
    ze_result_t setAsSignalEvent(Event *event, MutableComputeWalker *walkerCmd, MutablePipeControl *postSyncCmd);
    ze_result_t setAsWaitEvent(Event *event);
    //  --------------------

    // ------Usages---------
    ze_result_t addKernelArgUsage(const NEO::ArgDescriptor &arg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset, SurfaceStateHeapOffset sshOffset,
                                  uint32_t slmArgSize, uint32_t slmArgOffsetValue,
                                  CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData);
    ze_result_t addCsUsage(CommandBufferOffset csOffset, CommandBufferOffset csFullOffset);

    inline void addDispatch(VariableDispatch *vd) { usedInDispatch.push_back(vd); }
    inline const auto &getDispatches() const { return usedInDispatch; }

    void setBufferUsages(BufferUsages &&bufferUsages) { this->bufferUsages = std::move(bufferUsages); }
    inline const auto &getBufferUsages() const { return bufferUsages; }
    auto &getBufferUsages() { return bufferUsages; }

    void setValueUsages(ValueUsages &&valueUsages) { this->valueUsages = std::move(valueUsages); }
    inline const auto &getValueUsages() const { return valueUsages; }
    auto &getValueUsages() { return valueUsages; }
    // ----------------------

    VariableType getType() const { return desc.type; }
    bool isType(VariableType type);

    inline VariableDescriptor &getDesc() { return desc; }
    inline const VariableDescriptor &getDesc() const { return desc; }
    inline std::vector<MutableStoreRegisterMem *> &getStoreRegMemList() {
        return eventValue.storeRegMemCmds;
    }
    inline std::vector<MutableSemaphoreWait *> &getSemWaitList() {
        return eventValue.semWaitCmds;
    }
    inline std::vector<MutableStoreDataImm *> &getStoreDataImmList() {
        return eventValue.storeDataImmCmds;
    }
    inline std::vector<MutableLoadRegisterImm *> &getLoadRegImmList() {
        return eventValue.loadRegImmCmds;
    }

    void updateMutableComputeWalker(MutableComputeWalker *walkerCmd) {
        if (eventValue.walkerCmd != nullptr) {
            eventValue.walkerCmd = walkerCmd;
        }
    }

    inline void resetBufferVariable() {
        desc.bufferAlloc = nullptr;
        desc.bufferGpuAddress = undefined<GpuAddress>;
        desc.state = VariableDescriptor::State::defined;
        desc.argValue = reinterpret_cast<void *>(undefined<size_t>);
    }
    inline void resetSlmVariable() {
        slmValue.slmOffsetValue = undefined<SlmOffset>;
        slmValue.slmSize = undefined<SlmOffset>;
    }
    inline void resetGroupCountVariable() {
        kernelDispatch.groupCount[0] = 0;
        kernelDispatch.groupCount[1] = 0;
        kernelDispatch.groupCount[2] = 0;
    }
    inline void resetGroupSizeVariable() {
        kernelDispatch.groupSize[0] = 0;
        kernelDispatch.groupSize[1] = 0;
        kernelDispatch.groupSize[2] = 0;
    }
    inline void resetGlobalOffsetVariable() {
        kernelDispatch.globalOffset[0] = undefined<uint32_t>;
        kernelDispatch.globalOffset[1] = undefined<uint32_t>;
        kernelDispatch.globalOffset[2] = undefined<uint32_t>;
    }

    void commitVariable();
    void updateAllocationResidency(NEO::GraphicsAllocation *oldAllocation, NEO::GraphicsAllocation *newAllocation);

    void updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress);
    size_t createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress);
    void fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset, uint64_t &gpuAddress);

    bool isCooperativeVariable() const;
    inline VariableDispatch *getInitialVariableDispatch() const {
        if (usedInDispatch.empty()) {
            return nullptr;
        }
        return usedInDispatch[0];
    }

    void setNextSlmVariable(Variable *nextSlmVariable) {
        slmValue.nextSlmVariable = nextSlmVariable;
    }
    void setNextSlmVariableOffset(SlmOffset nextSlmOffset);
    void processVariableDispatchForSlm();
    uint32_t getAlignedSlmSize(uint32_t slmSize);

    void setGroupCountProperty(const uint32_t *groupCount) {
        kernelDispatch.groupCount[0] = groupCount[0];
        kernelDispatch.groupCount[1] = groupCount[1];
        kernelDispatch.groupCount[2] = groupCount[2];
    }

    void setGroupSizeProperty(const uint32_t *groupSize) {
        kernelDispatch.groupSize[0] = groupSize[0];
        kernelDispatch.groupSize[1] = groupSize[1];
        kernelDispatch.groupSize[2] = groupSize[2];
    }

    void setGlobalOffsetProperty(const uint32_t *globalOffset) {
        kernelDispatch.globalOffset[0] = globalOffset[0];
        kernelDispatch.globalOffset[1] = globalOffset[1];
        kernelDispatch.globalOffset[2] = globalOffset[2];
    }
    inline MutableCommandList *getCmdList() const {
        return cmdList;
    }

  protected:
    struct ImmediateValueChunkProperties {
        size_t size = 0;
        size_t sourceOffset = 0;

        size_t heapStatelessUsageIndex = std::numeric_limits<size_t>::max();
        size_t commandBufferUsageIndex = std::numeric_limits<size_t>::max();
    };

    struct EventValueProperties {
        Event *event = nullptr;
        NEO::GraphicsAllocation *eventPoolAllocation = nullptr;
        NEO::GraphicsAllocation *cbEventDeviceCounterAllocation = nullptr;
        uint64_t inOrderExecBaseSignalValue = 0;

        MutableComputeWalker *walkerCmd = nullptr;
        MutablePipeControl *postSyncCmd = nullptr;
        std::vector<MutableLoadRegisterImm *> loadRegImmCmds;
        std::vector<MutableSemaphoreWait *> semWaitCmds;
        std::vector<MutableStoreDataImm *> storeDataImmCmds;
        std::vector<MutableStoreRegisterMem *> storeRegMemCmds;

        uint32_t kernelCount = 0;
        uint32_t packetCount = 0;
        uint32_t waitPackets = 0;
        uint32_t inOrderAllocationOffset = 0;

        bool counterBasedEvent = false;
        bool inOrderIncrementEvent = false;
        bool noopState = false;
        bool isCbEventBoundToCmdList = false;
        bool hasStandaloneProfilingNode = false;
    };

    struct SlmValueProperties {
        Variable *nextSlmVariable = nullptr;
        SlmOffset slmSize = 0;
        SlmOffset slmOffsetValue = 0;
        uint8_t slmAlignment = 0;
    };

    struct KernelDispatchProperties {
        uint32_t groupCount[3] = {0, 0, 0};
        uint32_t groupSize[3] = {0, 0, 0};
        uint32_t globalOffset[3] = {undefined<uint32_t>, undefined<uint32_t>, undefined<uint32_t>};
    };

    void setDescExperimentalValues(const InterfaceVariableDescriptor *ifaceVarDesc);
    bool hasKernelArgCorrectType(const NEO::ArgDescriptor &arg);
    void mutateStatefulBufferArg(GpuAddress bufferGpuAddress, NEO::GraphicsAllocation *bufferAllocation);

    ze_result_t setBufferVariable(size_t size, const void *argVal);
    ze_result_t setValueVariable(size_t size, const void *argVal);
    ze_result_t setValueVariableInChunks(size_t size, const void *argVal);
    ze_result_t setValueVariableContinuous(size_t size, const void *argVal);
    ze_result_t setGroupSizeVariable(size_t size, const void *argVal);
    ze_result_t setGroupCountVariable(size_t size, const void *argVal);
    ze_result_t setSignalEventVariable(size_t size, const void *argVal);
    ze_result_t setWaitEventVariable(size_t size, const void *argVal);
    ze_result_t setGlobalOffsetVariable(size_t size, const void *argVal);
    ze_result_t setSlmBufferVariable(size_t size, const void *argVal);

    ze_result_t addKernelArgUsageImmediateAsChunk(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                  CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData);
    ze_result_t addKernelArgUsageImmediateAsContinuous(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                       CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData);
    ze_result_t addKernelArgUsageStatefulBuffer(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, SurfaceStateHeapOffset sshOffset);
    void handleFlags(uint32_t flags);
    ze_result_t selectImmediateSetValueHandler(size_t size, const void *argVal);
    ze_result_t selectImmediateAddKernelArgUsageHandler(const NEO::ArgDescriptor &kernelArg, IndirectObjectHeapOffset iohOffset, IndirectObjectHeapOffset iohFullOffset,
                                                        CommandBufferOffset walkerCmdOffset, MutableComputeWalker *mutableComputeWalker, bool inlineData);

    enum CbWaitEventOperationType {
        set,
        noop,
        restore
    };
    void setCbWaitEventUpdateOperation(CbWaitEventOperationType operation, uint64_t waitAddress, std::shared_ptr<NEO::InOrderExecInfo> *eventInOrderInfo);
    void addCommitVariableToBaseCmdList();
    void setCommitVariable() {
        if (desc.isStageCommit) {
            desc.commitRequired = true;
        }
        addCommitVariableToBaseCmdList();
    }

    VariableDescriptor desc;
    std::vector<VariableDispatch *> usedInDispatch;
    std::vector<ImmediateValueChunkProperties> immediateValueChunks;
    BufferUsages bufferUsages;
    ValueUsages valueUsages;

    EventValueProperties eventValue;
    SlmValueProperties slmValue;
    KernelDispatchProperties kernelDispatch;
    MutableCommandList *cmdList;
};
} // namespace L0::MCL
