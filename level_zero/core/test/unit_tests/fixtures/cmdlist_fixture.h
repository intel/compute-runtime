/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

class CommandListFixture : public DeviceFixture {
  public:
    void setUp();
    void tearDown();

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
};

struct MultiTileCommandListFixtureInit : public SingleRootMultiSubDeviceFixture {
    void setUp();
    void setUpParams(bool createImmediate, bool createInternal, bool createCopy);
    inline void tearDown() {
        SingleRootMultiSubDeviceFixture::tearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    std::unique_ptr<VariableBackup<bool>> apiSupportBackup;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
};

template <bool createImmediate, bool createInternal, bool createCopy>
struct MultiTileCommandListFixture : public MultiTileCommandListFixtureInit {
    void setUp() {
        MultiTileCommandListFixtureInit::setUp();
        MultiTileCommandListFixtureInit::setUpParams(createImmediate, createInternal, createCopy);
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
                                bool workloadPartition);

struct ModuleMutableCommandListFixture : public ModuleImmutableDataFixture {
    void setUp() {
        setUp(0u);
    }
    void setUp(uint32_t revision);
    void tearDown();
    void setUpImpl();

    std::unique_ptr<MockImmutableData> mockKernelImmData;
    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate;
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    std::unique_ptr<VariableBackup<HardwareInfo>> backupHwInfo;
    L0::ult::CommandQueue *commandQueue;
    NEO::EngineGroupType engineGroupType;

    DebugManagerStateRestore restorer;
};

struct MultiReturnCommandListFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

struct CmdListPipelineSelectStateFixture : public ModuleMutableCommandListFixture {
    void setUp();

    template <typename FamilyType>
    void testBody();

    template <typename FamilyType>
    void testBodyShareStateRegularImmediate();

    template <typename FamilyType>
    void testBodyShareStateImmediateRegular();
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
    uint32_t getMocs(bool l3On);

    size_t expectedSbaCmds = 0;
    bool dshRequired = false;
};

struct CommandListPrivateHeapsFixture : public CommandListStateBaseAddressFixture {
    void setUp();
};

struct CommandListGlobalHeapsFixtureInit : public CommandListStateBaseAddressFixture {
    void setUp();
    void setUpParams(int32_t globalHeapMode);
    void tearDown();
    std::unique_ptr<L0::ult::CommandList> commandListPrivateHeap;
};

template <int32_t globalHeapMode>
struct CommandListGlobalHeapsFixture : public CommandListGlobalHeapsFixtureInit {
    void setUp() {
        CommandListGlobalHeapsFixtureInit::setUpParams(globalHeapMode);
    }
};

struct ImmediateCmdListSharedHeapsFixture : public ModuleMutableCommandListFixture {
    void setUp();
};

class AppendFillFixture : public DeviceFixture {
  public:
    class MockDriverFillHandle : public L0::DriverHandleImp {
      public:
        bool findAllocationDataForRange(const void *buffer,
                                        size_t size,
                                        NEO::SvmAllocationData **allocData) override;

        const uint32_t rootDeviceIndex = 0u;
        std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
        NEO::SvmAllocationData data{rootDeviceIndex};
    };

    template <GFXCORE_FAMILY gfxCoreFamily>
    class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
      public:
        MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

        ze_result_t appendLaunchKernelWithParams(Kernel *kernel,
                                                 const ze_group_count_t *pThreadGroupDimensions,
                                                 L0::Event *event,
                                                 const CmdListKernelLaunchParams &launchParams) override {
            if (numberOfCallsToAppendLaunchKernelWithParams == thresholdOfCallsToAppendLaunchKernelWithParamsToFail) {
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            if (numberOfCallsToAppendLaunchKernelWithParams < 3) {
                threadGroupDimensions[numberOfCallsToAppendLaunchKernelWithParams] = *pThreadGroupDimensions;
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

} // namespace ult
} // namespace L0
