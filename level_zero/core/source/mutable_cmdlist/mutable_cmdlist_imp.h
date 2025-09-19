/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_group.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_variable_descriptor.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/label.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NEO {
struct KernelDescriptor;
class GraphicsAllocation;
using ResidencyContainer = std::vector<GraphicsAllocation *>;
} // namespace NEO

namespace L0 {
struct CommandList;
struct Context;
struct Event;
struct Kernel;
} // namespace L0

namespace L0::MCL {
struct Variable;
struct VariableDispatch;
struct Label;
struct KernelData;
struct KernelDispatch;

struct TempMem {
    struct Allocation {
        size_t size;
        void *memPtr;
        NEO::GraphicsAllocation *alloc;
    } allocation;

    size_t eleCount = 0;
    std::vector<Variable *> variables;
};

struct StateBaseAddressOffsets {
    CommandBufferOffset gsba = undefined<CommandBufferOffset>; // General State Base Address
    CommandBufferOffset isba = undefined<CommandBufferOffset>; // InStruction Base Address
    CommandBufferOffset ssba = undefined<CommandBufferOffset>; // Surface State Base Address
};

struct MclAllocations {
    std::unique_ptr<NEO::GraphicsAllocation> sshAlloc;
    std::unique_ptr<NEO::GraphicsAllocation> ihAlloc;
    std::unique_ptr<NEO::GraphicsAllocation> constsAlloc;
    std::unique_ptr<NEO::GraphicsAllocation> varsAlloc;
    std::unique_ptr<char[]> stringsAlloc;
};

using KernelMutationsContainer = std::vector<AppendKernelMutation>;
using EventMutationsContainer = std::vector<AppendEventMutation>;

using CooperativeKernelDispatchContainer = std::unordered_set<VariableDispatch *>;

struct AllocationReference {
    NEO::GraphicsAllocation *allocation = nullptr;
    uint32_t refCount = 0;
};

struct MutableResidencyAllocations {
    void addAllocation(NEO::GraphicsAllocation *allocation);
    void removeAllocation(NEO::GraphicsAllocation *allocation);
    void populateInputResidencyContainer(NEO::ResidencyContainer &cmdListResidency, bool baseCmdListClosed);
    void cleanResidencyContainer();
    void reserveSpace(size_t size) {
        addedAllocations.reserve(size);
    }

  protected:
    std::vector<AllocationReference> addedAllocations;
    size_t immutableResidencySize = 0;
};

struct MutableCommandListImp : public MutableCommandList {
    MutableCommandListImp(L0::CommandList *base) {
        this->base = base;
    }
    ~MutableCommandListImp() override;
    ze_command_list_handle_t toHandle() override;
    CommandList *getBase() override;

    ze_result_t getVariable(const InterfaceVariableDescriptor *varDesc, Variable **outVariable) override;
    ze_result_t getVariablesList(uint32_t *outVarCount, Variable **outVariables) override;
    ze_result_t getLabel(const InterfaceLabelDescriptor *labelDesc, Label **outLabel) override;

    ze_result_t tempMemSetElementCount(size_t elementCount) override;
    ze_result_t tempMemGetSize(size_t *tempMemSize) override;
    ze_result_t tempMemSet(const void *pTempMem) override;

    ze_result_t getNativeBinary(uint8_t *pBinary, size_t *pBinarySize, const uint8_t *pModule, size_t moduleSize) override;
    ze_result_t loadFromBinary(const uint8_t *pBinary, const size_t binarySize) override;

    ze_result_t getNextCommandId(const ze_mutable_command_id_exp_desc_t *desc, uint32_t numKernels, ze_kernel_handle_t *phKernels, uint64_t *pCommandId) override;
    ze_result_t updateMutableCommandsExp(const ze_mutable_commands_exp_desc_t *desc) override;
    ze_result_t updateMutableCommandSignalEventExp(uint64_t commandId, ze_event_handle_t signalEvent) override;
    ze_result_t updateMutableCommandWaitEventsExp(uint64_t commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;
    ze_result_t updateMutableCommandKernelsExp(uint32_t numKernels, uint64_t *pCommandId, ze_kernel_handle_t *phKernels) override;

    void removeFromResidencyContainer(NEO::GraphicsAllocation *allocation) override {
        if (allocation == nullptr) {
            return;
        }
        mutableAllocations.removeAllocation(allocation);
        this->finalizeCommandListResidency = true;
    }
    void addToResidencyContainer(NEO::GraphicsAllocation *allocation) override {
        if (allocation == nullptr) {
            return;
        }
        mutableAllocations.addAllocation(allocation);
        this->finalizeCommandListResidency = true;
    }
    void processResidencyContainer(bool baseCmdListClosed) override;
    void addVariableToCommitList(Variable *variable) override {
        stageCommitVariables.emplace_back(variable);
    }
    void toggleCommandListUpdated() override {
        this->updatedCommandList = true;
    }

    static constexpr bool kernelInstructionMutationEnabled(ze_mutable_command_exp_flags_t flags) {
        return ((flags & ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION) == ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    }

  protected:
    ze_result_t parseDispatchedKernel(L0::Kernel *kernel, MutableComputeWalker *mutableComputeWalker,
                                      size_t extraHeapSize, NEO::GraphicsAllocation *syncBuffer, NEO::GraphicsAllocation *regionBarrier);
    ze_result_t addVariableDispatch(const NEO::KernelDescriptor &kernelDescriptor, KernelDispatch &kernelDispatch, Variable *groupSize, Variable *groupCount, Variable *globalOffset,
                                    Variable *lastSlmArgumentVariable, MutableComputeWalker *mutableComputeWalker, const MutableKernelDispatchParameters &dispatchParams);

    void createNativeBinary(ArrayRef<const uint8_t> module);
    KernelData *getKernelData(L0::Kernel *kernel);

    KernelVariableDescriptor *getVariableDescriptorContainer(AppendKernelMutation &selectedAppend) {
        if (selectedAppend.kernelGroup != nullptr) {
            return &selectedAppend.kernelGroup->getCurrentMutableKernel()->getKernelVariables();
        } else {
            return &selectedAppend.variables;
        }
    }

    virtual void updateKernelMemoryPrefetch(const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, const CommandToPatch &cmdToPatch, uint64_t cmdId) = 0;

    MutableResidencyAllocations mutableAllocations;

    L0::CommandList *base;
    MclAllocations allocs;

    std::vector<uint8_t> nativeBinary;

    std::vector<std::unique_ptr<Label>> labelStorage;
    std::unordered_map<std::string, Label *> labelMap;

    std::vector<std::unique_ptr<Variable>> variableStorage;
    std::unordered_map<std::string, Variable *> variableMap;
    TempMem tempMem;

    std::vector<std::unique_ptr<KernelData>> kernelData;
    std::vector<std::unique_ptr<KernelDispatch>> dispatches;
    std::vector<StateBaseAddressOffsets> sbaVec;

    std::vector<std::unique_ptr<MutableComputeWalker>> mutableWalkerCmds;
    std::vector<std::unique_ptr<MutablePipeControl>> mutablePipeControlCmds;
    std::vector<std::unique_ptr<MutableStoreRegisterMem>> mutableStoreRegMemCmds;
    std::vector<std::unique_ptr<MutableSemaphoreWait>> mutableSemaphoreWaitCmds;
    std::vector<std::unique_ptr<MutableStoreDataImm>> mutableStoreDataImmCmds;
    std::vector<std::unique_ptr<MutableLoadRegisterImm>> mutableLoadRegisterImmCmds;
    std::vector<std::unique_ptr<MutableKernelGroup>> mutableKernelGroups;
    std::vector<Variable *> stageCommitVariables;

    CommandToPatchContainer appendCmdsToPatch;
    KernelMutationsContainer kernelMutations;
    EventMutationsContainer eventMutations;
    CooperativeKernelDispatchContainer cooperativeKernelVariableDispatches;

    MutableComputeWalker *appendKernelMutableComputeWalker = nullptr;

    uint64_t nextCommandId = 0;
    size_t iohAlignment = 0;

    uint32_t maxPerThreadDataSize = 0;
    uint32_t inlineDataSize = 0;
    ze_mutable_command_exp_flags_t nextMutationFlags = 0;

    bool nextAppendKernelMutable = false;
    bool baseCmdListClosed = false;
    bool enableReservePerThreadForLocalId = false;
    bool hasStageCommitVariables = false;
    bool updatedCommandList = false;
    bool finalizeCommandListResidency = true;
};
} // namespace L0::MCL
