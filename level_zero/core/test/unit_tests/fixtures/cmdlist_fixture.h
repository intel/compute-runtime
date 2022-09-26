/*
 * Copyright (C) 2020-2022 Intel Corporation
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

    std::unique_ptr<MockImmutableData> mockKernelImmData;
    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<L0::ult::CommandList> commandListImmediate;
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    L0::ult::CommandQueue *commandQueue;
    NEO::EngineGroupType engineGroupType;
};

struct MultiReturnCommandListFixture : public ModuleMutableCommandListFixture {
    void setUp();

    DebugManagerStateRestore restorer;
};

struct CmdListPipelineSelectStateFixture : public ModuleMutableCommandListFixture {
    void setUp();

    template <typename FamilyType>
    void testBody();

    template <typename FamilyType>
    void testBodyShareStateRegularImmediate();

    template <typename FamilyType>
    void testBodyShareStateImmediateRegular();

    DebugManagerStateRestore restorer;
};

struct CmdListStateComputeModeStateFixture : public ModuleMutableCommandListFixture {
    void setUp();

    DebugManagerStateRestore restorer;
};

struct CmdListThreadArbitrationFixture : public CmdListStateComputeModeStateFixture {
    template <typename FamilyType>
    void testBody();
};

struct CmdListLargeGrfFixture : public CmdListStateComputeModeStateFixture {
    template <typename FamilyType>
    void testBody();
};

struct ImmediateCmdListSharedHeapsFixture : public ModuleMutableCommandListFixture {
    void setUp();

    DebugManagerStateRestore restorer;
};

} // namespace ult
} // namespace L0
