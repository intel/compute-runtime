/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/kernel_arg_descriptor.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable_dispatch.h"

#include <map>

namespace NEO {
class IndirectHeap;
} // namespace NEO

namespace L0 {
struct CommandList;
struct Event;
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily;
} // namespace L0

namespace L0 {
namespace ult {
template <typename Type>
struct WhiteBox;

struct VariableFixture : public MutableCommandListFixtureInit {
    constexpr static size_t kernelArgVariableSize = sizeof(void *);
    constexpr static size_t kernelDispatchVariableSize = 3 * sizeof(uint32_t);
    constexpr static size_t walkerBufferSize = 512;
    constexpr static size_t indirectBufferSize = 512;
    constexpr static CrossThreadDataOffset defaultBufferStatelessOffset = 64;
    constexpr static CrossThreadDataOffset defaultValueStatelessOffset = defaultBufferStatelessOffset + 16;
    constexpr static uint16_t defaultValueStatelessSize = 16;
    constexpr static CrossThreadDataOffset defaultSlmFirstOffset = defaultValueStatelessOffset + defaultValueStatelessSize;
    constexpr static CrossThreadDataOffset defaultSlmSecondOffset = defaultSlmFirstOffset + 8;
    constexpr static uint8_t defaultSlmArgumentAlignment = 4;
    constexpr static size_t totalSlmAlignment = 1024;

    void setUp(bool inOrder);
    void setUp() {
        setUp(false);
    }
    void tearDown();

    void createVariable(L0::MCL::VariableType varType, bool baseVariable, int32_t overrideStageCommit, int32_t overrideValueChunks);
    Variable *getVariable(L0::MCL::VariableType varType) {
        return (variablesMap[varType]).get();
    }
    void createVariableDispatch(bool useGroupCount, bool useGroupSize, bool useGlobalOffset, bool useSlm);
    void createMutableIndirectOffset();

    void reAllocateCrossThread(size_t newSize);
    void initCommandBufferWithWalker();

    template <typename FamilyType, typename WalkerType>
    void createMutableComputeWalker(uint64_t postSyncAddress) {
        this->walkerCmdSize = L0::MCL::MutableComputeWalkerHw<FamilyType>::getCommandSize();
        this->cpuWalkerBuffer = L0::MCL::MutableComputeWalkerHw<FamilyType>::createCommandBuffer();

        auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
        *reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer) = walkerTemplate;
        *reinterpret_cast<WalkerType *>(this->walkerBuffer) = walkerTemplate;
        if (postSyncAddress != 0) {
            auto walkerCmdGpu = reinterpret_cast<WalkerType *>(this->walkerBuffer);
            walkerCmdGpu->getPostSync().setDestinationAddress(postSyncAddress);

            auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
            walkerCmdCpu->getPostSync().setDestinationAddress(postSyncAddress);
        }

        mutableComputeWalker = std::make_unique<L0::MCL::MutableComputeWalkerHw<FamilyType>>(reinterpret_cast<void *>(this->walkerBuffer),
                                                                                             this->indirectOffset,
                                                                                             this->scratchOffset,
                                                                                             this->cpuWalkerBuffer,
                                                                                             this->stageCommitMode);

        this->inlineOffset = mutableComputeWalker->getInlineDataOffset();
        this->inlineSize = mutableComputeWalker->getInlineDataSize();
    }

    template <typename FamilyType>
    void createMutablePipeControl() {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        this->pipeControlBuffer = this->cmdBuffer->getSpace(sizeof(PIPE_CONTROL));
        *reinterpret_cast<PIPE_CONTROL *>(this->pipeControlBuffer) = FamilyType::cmdInitPipeControl;

        this->mutablePipeControl = std::make_unique<L0::MCL::MutablePipeControlHw<FamilyType>>(this->pipeControlBuffer);
    }

    template <typename FamilyType>
    void createMutableLoadRegisterImm(uint32_t registerAddress, size_t inOrderPatchListIndex) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        auto loadRegisterImmBuffer = this->cmdBuffer->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
        *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(loadRegisterImmBuffer) = FamilyType::cmdInitLoadRegisterImm;

        this->loadRegisterImmBuffers.push_back(loadRegisterImmBuffer);
        this->mutableLoadRegisterImms.push_back(std::make_unique<L0::MCL::MutableLoadRegisterImmHw<FamilyType>>(loadRegisterImmBuffer,
                                                                                                                registerAddress,
                                                                                                                inOrderPatchListIndex));
    }

    template <typename FamilyType>
    void createMutableSemaphoreWait(size_t offset, size_t inOrderPatchListIndex, L0::MCL::MutableSemaphoreWait::Type type, bool qwordDataIndirect) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        this->semaphoreWaitBuffer = this->cmdBuffer->getSpace(sizeof(MI_SEMAPHORE_WAIT));
        *reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer) = FamilyType::cmdInitMiSemaphoreWait;

        this->mutableSemaphoreWait = std::make_unique<L0::MCL::MutableSemaphoreWaitHw<FamilyType>>(this->semaphoreWaitBuffer,
                                                                                                   offset,
                                                                                                   inOrderPatchListIndex,
                                                                                                   type,
                                                                                                   qwordDataIndirect);
    }

    template <typename FamilyType>
    void createMutableStoreDataImm(size_t offset, bool workloadPartition) {
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        this->storeDataImmBuffer = this->cmdBuffer->getSpace(sizeof(MI_STORE_DATA_IMM));
        *reinterpret_cast<MI_STORE_DATA_IMM *>(this->storeDataImmBuffer) = FamilyType::cmdInitStoreDataImm;

        this->mutableStoreDataImm = std::make_unique<L0::MCL::MutableStoreDataImmHw<FamilyType>>(this->storeDataImmBuffer,
                                                                                                 offset,
                                                                                                 workloadPartition);
    }

    template <typename FamilyType>
    void createMutableStoreRegisterMem(size_t offset) {
        using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
        this->storeRegisterMemBuffer = this->cmdBuffer->getSpace(sizeof(MI_STORE_REGISTER_MEM));
        *reinterpret_cast<MI_STORE_REGISTER_MEM *>(this->storeRegisterMemBuffer) = FamilyType::cmdInitStoreRegisterMem;

        this->mutableStoreRegisterMem = std::make_unique<L0::MCL::MutableStoreRegisterMemHw<FamilyType>>(this->storeRegisterMemBuffer,
                                                                                                         offset);
    }

    struct Offsets {
        constexpr static L0::MCL::CrossThreadDataOffset patchSize = 12;
        constexpr static L0::MCL::CrossThreadDataOffset initialOffset = 16;

        L0::MCL::CrossThreadDataOffset globalWorkSize = initialOffset;
        L0::MCL::CrossThreadDataOffset localWorkSize = initialOffset + patchSize;
        L0::MCL::CrossThreadDataOffset localWorkSize2 = initialOffset + 2 * patchSize;
        L0::MCL::CrossThreadDataOffset enqLocalWorkSize = initialOffset + 3 * patchSize;
        L0::MCL::CrossThreadDataOffset numWorkGroups = initialOffset + 4 * patchSize;
        L0::MCL::CrossThreadDataOffset workDimensions = initialOffset + 5 * patchSize;
        L0::MCL::CrossThreadDataOffset globalWorkOffset = initialOffset + 6 * patchSize;
    } offsets;

    ::L0::MCL::MutableKernelDispatchParameters dispatchParams;

    NEO::ArgDescriptor kernelArgPtr = {NEO::ArgDescriptor::argTPointer};
    NEO::ArgDescriptor kernelArgValue = {NEO::ArgDescriptor::argTValue};
    NEO::ArgDescriptor kernelArgSlmFirst = {NEO::ArgDescriptor::argTPointer};
    NEO::ArgDescriptor kernelArgSlmSecond = {NEO::ArgDescriptor::argTPointer};

    std::map<L0::MCL::VariableType, std::unique_ptr<Variable>> variablesMap;
    std::vector<void *> loadRegisterImmBuffers;
    std::vector<std::unique_ptr<L0::MCL::MutableLoadRegisterImm>> mutableLoadRegisterImms;

    std::unique_ptr<Variable> variable;
    std::unique_ptr<VariableDispatch> variableDispatch;
    std::unique_ptr<L0::MCL::KernelDispatch> kernelDispatch;
    std::unique_ptr<L0::MCL::MutableIndirectData> indirectData;
    std::unique_ptr<uint8_t, std::function<void(void *)>> perThreadData;
    std::unique_ptr<uint8_t, std::function<void(void *)>> crossThreadData;
    std::unique_ptr<L0::MCL::MutableComputeWalker> mutableComputeWalker;
    std::unique_ptr<L0::MCL::MutablePipeControl> mutablePipeControl;
    std::unique_ptr<L0::MCL::MutableSemaphoreWait> mutableSemaphoreWait;
    std::unique_ptr<L0::MCL::MutableStoreDataImm> mutableStoreDataImm;
    std::unique_ptr<L0::MCL::MutableStoreRegisterMem> mutableStoreRegisterMem;

    NEO::LinearStream *cmdBuffer = nullptr;
    NEO::IndirectHeap *ioh = nullptr;

    size_t perThreadDataSize = 0;
    size_t crossThreadDataSize = 256;
    size_t walkerCmdSize = 0;
    size_t inlineOffset = 0;
    size_t inlineSize = 0;

    void *walkerBuffer = nullptr;
    void *cpuWalkerBuffer = nullptr;
    void *cmdListBufferCpuBase = nullptr;
    void *cmdListIndirectCpuBase = nullptr;
    void *pipeControlBuffer = nullptr;
    void *semaphoreWaitBuffer = nullptr;
    void *storeDataImmBuffer = nullptr;
    void *storeRegisterMemBuffer = nullptr;

    uint32_t initialGroupCount[3] = {1, 1, 1};
    uint32_t initialGroupSize[3] = {1, 1, 1};
    uint32_t initialGlobalOffset[3] = {0, 0, 0};

    uint32_t grfSize = 32;
    uint32_t partitionCount = 1;
    uint32_t slmSize = 0;
    uint32_t slmOffset = 0;
    uint32_t slmInlineSize = 0;

    uint8_t indirectOffset = 0;
    uint8_t scratchOffset = 8;

    bool calculateRegion = false;
    bool inlineData = false;
    bool usePerThread = false;
    bool stageCommitMode = true;
    bool qwordIndirect = false;
    bool inOrder = false;
};

struct MockCommandList;

struct VariableInOrderFixture : public VariableFixture {
    void setUp();
    void attachCbEvent(Event *event, L0::ult::MockCommandList *cmdList);
    void attachCbEvent(Event *event) {
        attachCbEvent(event, this->mockBaseCmdList);
    }

    template <typename FamilyType>
    NEO::InOrderPatchCommandsContainer<FamilyType> &prepareInOrderWaitCommands(L0::CommandList *cmdList, bool disablePatching, bool addToPatchList) {
        auto mockCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(cmdList);
        size_t inOrderPatchIndex = addToPatchList ? mockCmdListHw->inOrderPatchCmds.size() : std::numeric_limits<size_t>::max();
        if (this->qwordIndirect) {
            createMutableLoadRegisterImm<FamilyType>(0x2600, inOrderPatchIndex);
            createMutableLoadRegisterImm<FamilyType>(0x2604, inOrderPatchIndex);
            if (addToPatchList) {
                mockCmdListHw->addCmdForPatching(&mockCmdListHw->inOrderExecInfo, this->loadRegisterImmBuffers[0], this->loadRegisterImmBuffers[1], 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::lri64b);
                if (disablePatching) {
                    mockCmdListHw->disablePatching(inOrderPatchIndex);
                }
                inOrderPatchIndex++;
            }
        }

        createMutableSemaphoreWait<FamilyType>(this->semWaitOffset, inOrderPatchIndex, L0::MCL::MutableSemaphoreWait::Type::cbEventWait, this->qwordIndirect);
        if (addToPatchList) {
            mockCmdListHw->addCmdForPatching(&mockCmdListHw->inOrderExecInfo, this->semaphoreWaitBuffer, nullptr, 1, NEO::InOrderPatchCommandHelpers::PatchCmdType::semaphore);
            if (disablePatching) {
                mockCmdListHw->disablePatching(inOrderPatchIndex);
            }
        }

        return mockCmdListHw->inOrderPatchCmds;
    }

    uint64_t cmdListInOrderCounterValue = 0;

    L0::ult::MockCommandList *mockBaseCmdList = nullptr;
    size_t semWaitOffset = 0x10;

    uint32_t cmdListInOrderAllocationOffset = 0;
};

} // namespace ult
} // namespace L0
