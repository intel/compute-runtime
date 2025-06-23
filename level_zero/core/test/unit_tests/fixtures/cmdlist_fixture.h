/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace L0 {
namespace ult {
class CommandListFixture : public DeviceFixture {
  public:
    CommandListFixture();
    ~CommandListFixture() override;
    void setUp();
    void tearDown();

    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<WhiteBox<L0::Event>> event;
};

class DirectSubmissionCommandListFixture : public CommandListFixture {
  public:
    DirectSubmissionCommandListFixture();
    ~DirectSubmissionCommandListFixture() override;
    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;
};

struct MultiTileCommandListFixtureInit : public SingleRootMultiSubDeviceFixture {
    void setUp();
    void setUpParams(bool createImmediate, bool createInternal, bool createCopy, int32_t primaryBuffer);
    inline void tearDown() {
        SingleRootMultiSubDeviceFixture::tearDown();
    }

    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
};

template <bool createImmediate, bool createInternal, bool createCopy, int32_t primaryBuffer>
struct MultiTileCommandListFixture : public MultiTileCommandListFixtureInit {
    void setUp() {
        MultiTileCommandListFixtureInit::setUp();
        MultiTileCommandListFixtureInit::setUpParams(createImmediate, createInternal, createCopy, primaryBuffer);
    }

    void tearDown() {
        MultiTileCommandListFixtureInit::tearDown();
    }
};

template <typename FamilyType>
void validateTimestampRegisters(GenCmdList &cmdList,
                                GenCmdList::iterator &startIt,
                                uint32_t firstLoadRegisterRegSrcAddress,
                                uint64_t firstStoreRegMemAddress,
                                uint32_t secondLoadRegisterRegSrcAddress,
                                uint64_t secondStoreRegMemAddress,
                                bool workloadPartition,
                                bool useMask);

struct ModuleMutableCommandListFixture : public ModuleImmutableDataFixture {
    void setUp() {
        setUp(0u);
    }
    void setUp(uint32_t revision);
    void tearDown();
    void setUpImpl();

    uint32_t getMocs(bool l3On);

    std::unique_ptr<MockImmutableData> mockKernelImmData;
    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList;
    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandListImmediate;
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    std::unique_ptr<VariableBackup<HardwareInfo>> backupHwInfo;
    WhiteBox<L0::CommandQueue> *commandQueue;
    size_t expectedSbaCmds = 0;
    NEO::EngineGroupType engineGroupType;

    bool dshRequired = false;

    DebugManagerStateRestore restorer;
};

struct FrontEndCommandListFixtureInit : public ModuleMutableCommandListFixture {
    void setUp() {
        setUp(0);
    }
    void setUp(int32_t dispatchCmdBufferPrimary);
};

template <int32_t dispatchCmdBufferPrimary>
struct FrontEndCommandListFixture : public FrontEndCommandListFixtureInit {
    void setUp() {
        FrontEndCommandListFixtureInit::setUp(dispatchCmdBufferPrimary);
    }
};

struct CmdListPipelineSelectStateFixture : public ModuleMutableCommandListFixture {
    void setUp();
    void tearDown();

    template <typename FamilyType>
    void testBody();

    template <typename FamilyType>
    void testBodyShareStateRegularImmediate();

    template <typename FamilyType>
    void testBodyShareStateImmediateRegular();

    template <typename FamilyType>
    void testBodySystolicAndScratchOnSecondCommandList();

    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList2;
};

struct CmdListStateComputeModeStateFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

struct CmdListThreadArbitrationFixture : public CmdListStateComputeModeStateFixture {
    template <typename FamilyType>
    void testBody();
};

struct CmdListLargeGrfFixture : public CmdListStateComputeModeStateFixture {
    template <typename FamilyType>
    void testBody();
};

struct CommandListStateBaseAddressFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

struct CommandListPrivateHeapsFixture : public CommandListStateBaseAddressFixture {
    void setUp();
    void checkAndPrepareBindlessKernel();

    bool isBindlessKernel = false;
    NEO::BindlessHeapsHelper *bindlessHeapsHelper = nullptr;
};

struct CommandListGlobalHeapsFixtureInit : public CommandListStateBaseAddressFixture {
    void setUp();
    void setUpParams(int32_t globalHeapMode);
    void tearDown();
    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandListPrivateHeap;
};

template <int32_t globalHeapMode>
struct CommandListGlobalHeapsFixture : public CommandListGlobalHeapsFixtureInit {
    void setUp() {
        CommandListGlobalHeapsFixtureInit::setUpParams(globalHeapMode);
    }
};

struct ImmediateCmdListSharedHeapsFixture : public ModuleMutableCommandListFixture {
    void setUp();
    void tearDown();

    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandListImmediateCoexisting;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

struct ImmediateCmdListSharedHeapsFlushTaskFixtureInit : public ImmediateCmdListSharedHeapsFixture {
    void setUp(int32_t useImmediateFlushTask);

    enum NonKernelOperation {
        Barrier = 0,
        SignalEvent,
        ResetEvent,
        WaitOnEvents,
        WriteGlobalTimestamp,
        MemoryRangesBarrier
    };

    template <typename FamilyType>
    void testBody(NonKernelOperation operation);

    void appendNonKernelOperation(WhiteBox<L0::CommandListImp> *currentCmdList, NonKernelOperation operation);

    void validateDispatchFlags(bool nonKernel, NEO::ImmediateDispatchFlags &recordedImmediateFlushTaskFlags, const NEO::IndirectHeap *recordedSsh);

    int32_t useImmediateFlushTask;
};

template <int32_t useImmediateFlushTaskT>
struct ImmediateCmdListSharedHeapsFlushTaskFixture : public ImmediateCmdListSharedHeapsFlushTaskFixtureInit {
    void setUp() {
        ImmediateCmdListSharedHeapsFlushTaskFixtureInit::setUp(useImmediateFlushTaskT);
    }
};

class AppendFillFixture : public DeviceFixture {
  public:
    class MockDriverFillHandle : public L0::DriverHandleImp {
      public:
        bool findAllocationDataForRange(const void *buffer,
                                        size_t size,
                                        NEO::SvmAllocationData *&allocData) override;

        const uint32_t rootDeviceIndex = 0u;
        bool forceFalseFromfindAllocationDataForRange = false;
        std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
        NEO::SvmAllocationData data{rootDeviceIndex};
    };

    template <GFXCORE_FAMILY gfxCoreFamily>
    class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
        using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

      public:
        MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

        ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                                 const ze_group_count_t &pThreadGroupDimensions,
                                                 L0::Event *event,
                                                 CmdListKernelLaunchParams &launchParams) override {
            if (numberOfCallsToAppendLaunchKernelWithParams == thresholdOfCallsToAppendLaunchKernelWithParamsToFail) {
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            if (numberOfCallsToAppendLaunchKernelWithParams < 3) {
                threadGroupDimensions[numberOfCallsToAppendLaunchKernelWithParams] = pThreadGroupDimensions;
                xGroupSizes[numberOfCallsToAppendLaunchKernelWithParams] = kernel->getGroupSize()[0];
            }
            numberOfCallsToAppendLaunchKernelWithParams++;
            return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(kernel,
                                                                                      pThreadGroupDimensions,
                                                                                      event,
                                                                                      launchParams);
        }

        ze_group_count_t threadGroupDimensions[3];
        uint32_t xGroupSizes[3];
        uint32_t thresholdOfCallsToAppendLaunchKernelWithParamsToFail = std::numeric_limits<uint32_t>::max();
        uint32_t numberOfCallsToAppendLaunchKernelWithParams = 0;
    };

    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;

    std::unique_ptr<Mock<MockDriverFillHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    static constexpr size_t allocSize = 70;
    static constexpr size_t patternSize = 8;
    uint8_t *dstPtr = nullptr;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    static constexpr size_t immediateAllocSize = 106;
    uint8_t immediatePattern = 4;
    uint8_t *immediateDstPtr = nullptr;
};

struct TestExpectedValues {
    uint32_t expectedPacketsInUse = 0;
    uint32_t expectedKernelCount = 0;
    uint32_t expectedWalkerPostSyncOp = 0;
    uint32_t expectedPostSyncPipeControls = 0;
    uint32_t expectDcFlush = 0;
    uint32_t expectStoreDataImm = 0;
    bool postSyncAddressZero = false;
    bool workloadPartition = false;
};

struct CommandListEventUsedPacketSignalFixture : public CommandListFixture {
    void setUp();

    DebugManagerStateRestore restorer;
};

struct CommandListSecondaryBatchBufferFixture : public CommandListFixture {
    void setUp();

    DebugManagerStateRestore restorer;
};

struct TbxImmediateCommandListFixture : public ModuleMutableCommandListFixture {
    using EventFieldType = uint64_t;

    template <typename FamilyType>
    void setUpT();

    template <typename FamilyType>
    void tearDownT() {
        event.reset(nullptr);
        eventPool.reset(nullptr);

        ModuleMutableCommandListFixture::tearDown();
    }

    void setEvent();

    void setUp() {}
    void tearDown() {}

    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

struct RayTracingCmdListFixture : public ModuleMutableCommandListFixture {
    void setUp();

    uint64_t rtAllocationAddress = 0;
    NEO::GraphicsAllocation *rtAllocation = nullptr;
};

struct CommandListAppendLaunchRayTracingKernelFixture : ModuleFixture {
    void setUp();
    void tearDown();

    ContextImp *contextImp = nullptr;
    WhiteBox<::L0::CommandListImp> *commandList = nullptr;

    ze_group_count_t dispatchKernelArguments;

    void *allocSrc, *allocDst;
    NEO::GraphicsAllocation *buffer1 = nullptr;
    NEO::GraphicsAllocation *buffer2 = nullptr;
};

struct PrimaryBatchBufferCmdListFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

struct PrimaryBatchBufferPreamblelessCmdListFixture : public PrimaryBatchBufferCmdListFixture {
    void setUp();
    void tearDown();

    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList2;
    std::unique_ptr<WhiteBox<L0::CommandListImp>> commandList3;
};

struct ImmediateFlushTaskCmdListFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

struct ImmediateFlushTaskGlobalStatelessCmdListFixture : public ImmediateFlushTaskCmdListFixture {
    void setUp();
};

struct ImmediateFlushTaskCsrSharedHeapCmdListFixture : public ImmediateFlushTaskCmdListFixture {
    void setUp();
};

struct ImmediateFlushTaskPrivateHeapCmdListFixture : public ImmediateFlushTaskCmdListFixture {
    void setUp();
};

struct CommandQueueThreadArbitrationPolicyFixture {
    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;
    WhiteBox<L0::CommandQueue> *commandQueue = nullptr;
    L0::CommandList *commandList = nullptr;
    std::unique_ptr<L0::ult::WhiteBox<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

struct CommandListScratchPatchFixtureInit : public ModuleMutableCommandListFixture {
    void setUpParams(int32_t globalStatelessMode, int32_t heaplessStateInitEnabled, bool scratchAddressPatchingEnabled);
    void tearDown();

    uint64_t getSurfStateGpuBase(bool useImmediate);
    uint64_t getExpectedScratchPatchAddress(uint64_t controllerScratchAddress);

    template <typename FamilyType>
    void testScratchInline(bool useImmediate);

    template <typename FamilyType>
    void testScratchGrowingPatching();

    template <typename FamilyType>
    void testScratchSameNotPatching();

    template <typename FamilyType>
    void testScratchImmediatePatching();

    template <typename FamilyType>
    void testScratchChangedControllerPatching();

    template <typename FamilyType>
    void testScratchCommandViewNoPatching();

    template <typename FamilyType>
    void testExternalScratchPatching();

    template <typename FamilyType>
    void testScratchUndefinedPatching();

    int32_t fixtureGlobalStatelessMode = 0;
    uint32_t scratchInlineOffset = 8;
    uint32_t scratchInlinePointerSize = sizeof(uint64_t);
};

template <int32_t globalStatelessMode, int32_t heaplessStateInitEnabled, bool scratchAddressPatchingEnabled>
struct CommandListScratchPatchFixture : public CommandListScratchPatchFixtureInit {
    void setUp() {
        CommandListScratchPatchFixtureInit::setUpParams(globalStatelessMode, heaplessStateInitEnabled, scratchAddressPatchingEnabled);
    }
};

struct CommandListCreateFixture : public DeviceFixture {
    CmdListMemoryCopyParams copyParams = {};
};

struct AppendMemoryCopyFixture : public DeviceFixture {
    CmdListMemoryCopyParams copyParams = {};
};

} // namespace ult
} // namespace L0
