/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/helpers/api_handle_helper.h"
#include <level_zero/ze_api.h>

#include <memory>

namespace NEO {
class GraphicsAllocation;
class InOrderExecInfo;
enum class EngineGroupType : uint32_t;
enum class MiPredicateType : uint32_t;
} // namespace NEO
namespace L0 {
struct Device;
struct CommandList;
struct Kernel;
struct Event;
struct CmdListKernelLaunchParams;

namespace MCL {
struct InterfaceLabelDescriptor;
struct InterfaceVariableDescriptor;
struct Label;
struct MutableComputeWalker;
struct Variable;

enum MclAluReg : uint32_t {
    mclAluRegGpr0 = 0,
    mclAluRegGpr0_1 = 1,
    mclAluRegGpr1 = 2,
    mclAluRegGpr1_1 = 3,
    mclAluRegGpr2 = 4,
    mclAluRegGpr2_1 = 5,
    mclAluRegGpr3 = 6,
    mclAluRegGpr3_1 = 7,
    mclAluRegGpr4 = 8,
    mclAluRegGpr4_1 = 9,
    mclAluRegGpr5 = 10,
    mclAluRegGpr5_1 = 11,
    mclAluRegGpr6 = 12,
    mclAluRegGpr6_1 = 13,
    mclAluRegGpr7 = 14,
    mclAluRegGpr7_1 = 15,
    mclAluRegGpr8 = 16,
    mclAluRegGpr8_1 = 17,
    mclAluRegGpr9 = 18,
    mclAluRegGpr9_1 = 19,
    mclAluRegGpr10 = 20,
    mclAluRegGpr10_1 = 21,
    mclAluRegGpr11 = 22,
    mclAluRegGpr11_1 = 23,
    mclAluRegGpr12 = 24,
    mclAluRegGpr12_1 = 25,
    mclAluRegGpr13 = 26,
    mclAluRegGpr13_1 = 27,
    mclAluRegGpr14 = 28,
    mclAluRegGpr14_1 = 29,
    mclAluRegGpr15 = 30,
    mclAluRegGpr15_1 = 31,
    mclAluRegGprMax = 32,
    mclAluRegPredicate1 = 33,
    mclAluRegRegMax = 36,
    mclAluRegConst0 = 37,
    mclAluRegConst1 = 38,
    mclAluRegNone = 39,
    mclAluRegPredicate2 = 40,
    mclAluRegPredicateResult = 41,
    mclAluRegMax = 42
};

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

struct InterfaceOperandDescriptor {
    enum Flags : uint32_t {
        usesVariable = BIT(0),
        jumpOnClear = BIT(1)
    };

    void *memory = nullptr;
    size_t offset = 0;
    uint32_t size = 0;
    uint32_t flags = 0;
};

struct MutableCommandList {
    template <typename Type>
    struct Allocator {
        static MutableCommandList *allocate(uint32_t numIddsPerBlock) { return new Type(numIddsPerBlock); }
    };

    static CommandList *create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                               ze_command_list_flags_t flags, ze_result_t &resultValue, bool useInternalEngineType);
    virtual ze_result_t initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) = 0;
    virtual ~MutableCommandList() = 0;

    static MutableCommandList *fromHandle(ze_command_list_handle_t handle);
    virtual ze_command_list_handle_t toHandle() = 0;
    virtual CommandList *getBase() = 0;

    virtual ze_result_t appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                           const ze_group_count_t &threadGroupDimensions,
                                           ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents,
                                           CmdListKernelLaunchParams &launchParams) = 0;
    virtual ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                                     const ze_group_count_t &threadGroupDimensions,
                                                     Event *event,
                                                     CmdListKernelLaunchParams &launchParams) = 0;
    virtual ze_result_t appendVariableLaunchKernel(Kernel *kernel,
                                                   Variable *groupCount,
                                                   Event *signalEvent,
                                                   uint32_t numWaitEvents,
                                                   ze_event_handle_t *waitEvents) = 0;
    virtual ze_result_t close() = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t appendSetPredicate(NEO::MiPredicateType predicateType) = 0;
    virtual ze_result_t appendJump(Label *label, const InterfaceOperandDescriptor *condition) = 0;
    virtual ze_result_t appendMILoadRegVariable(MclAluReg reg, Variable *variable) = 0;
    virtual ze_result_t appendMIStoreRegVariable(MclAluReg reg, Variable *variable) = 0;

    virtual ze_result_t tempMemSetElementCount(size_t elementCount) = 0;
    virtual ze_result_t tempMemGetSize(size_t *tempMemSize) = 0;
    virtual ze_result_t tempMemSet(const void *pTempMem) = 0;

    virtual ze_result_t getVariable(const InterfaceVariableDescriptor *varDesc, Variable **outVariable) = 0;
    virtual ze_result_t getLabel(const InterfaceLabelDescriptor *labelDesc, Label **outLabel) = 0;
    virtual ze_result_t getVariablesList(uint32_t *outVarCount, Variable **outVariables) = 0;

    virtual ze_result_t getNativeBinary(uint8_t *outBinary, size_t *outBinarySize, const uint8_t *module, size_t moduleSize) = 0;
    virtual ze_result_t loadFromBinary(const uint8_t *binary, const size_t binarySize) = 0;

    virtual ze_result_t getNextCommandId(const ze_mutable_command_id_exp_desc_t *desc, uint32_t numKernels, ze_kernel_handle_t *phKernels, uint64_t *pCommandId) = 0;
    virtual ze_result_t updateMutableCommandsExp(const ze_mutable_commands_exp_desc_t *desc) = 0;
    virtual ze_result_t updateMutableCommandSignalEventExp(uint64_t commandId, ze_event_handle_t signalEvent) = 0;
    virtual ze_result_t updateMutableCommandWaitEventsExp(uint64_t commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t updateMutableCommandKernelsExp(uint32_t numKernels, uint64_t *pCommandId, ze_kernel_handle_t *phKernels) = 0;

    virtual void removeFromResidencyContainer(NEO::GraphicsAllocation *allocation) = 0;
    virtual void addToResidencyContainer(NEO::GraphicsAllocation *allocation) = 0;
    virtual void processResidencyContainer(bool baseCmdListClosed) = 0;

    virtual void setBufferSurfaceState(void *address, NEO::GraphicsAllocation *alloc, Variable *variable) = 0;

    virtual MutableComputeWalker *getCommandWalker(size_t offsetToWalkerCommand, uint8_t indirectOffset, uint8_t scratchOffset) = 0;

    virtual void switchCounterBasedEvents(uint64_t inOrderExecBaseSignalValue, uint32_t inOrderAllocationOffset, Event *newEvent) = 0;

    virtual bool isCbEventBoundToCmdList(Event *event) const = 0;
    virtual NEO::GraphicsAllocation *getDeviceCounterAllocForResidency(NEO::GraphicsAllocation *counterDeviceAlloc) = 0;
    virtual bool isQwordInOrderCounter() const = 0;

    virtual void updateInOrderExecInfo(size_t inOrderPatchIndex, std::shared_ptr<NEO::InOrderExecInfo> *inOrderExecInfo, bool disablePatchingFlag) = 0;
    virtual void disablePatching(size_t inOrderPatchIndex) = 0;
    virtual void enablePatching(size_t inOrderPatchIndex) = 0;

    virtual void updateScratchAddress(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) = 0;
    virtual void updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) = 0;
    virtual uint64_t getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const = 0;
    virtual void updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset) = 0;
    virtual size_t createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset) = 0;
    virtual void fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset) = 0;
    virtual void addVariableToCommitList(Variable *variable) = 0;

    virtual void toggleCommandListUpdated() = 0;
};

using MutableCommandListAllocFn = MutableCommandList *(*)(uint32_t);
extern MutableCommandListAllocFn mutableCommandListFactory[];

template <uint32_t productFamily, typename CommandListType>
struct MutableCommandListPopulateFactory {
    MutableCommandListPopulateFactory() {
        mutableCommandListFactory[productFamily] = MutableCommandList::Allocator<CommandListType>::allocate;
    }
};
} // namespace MCL
} // namespace L0
