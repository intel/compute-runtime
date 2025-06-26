/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_cmdlist.h"

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Event;
struct Kernel;

namespace ult {

struct MutableCommandListFixtureInit : public ModuleImmutableDataFixture {
    static constexpr uint16_t defaultCrossThreadOffset = 64;
    static constexpr uint16_t defaultNextArgOffset = 32;
    static constexpr uint32_t crossThreadInitSize = 1024;
    static constexpr uint32_t kernelIsaMutationFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                                                       ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                                                       ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                                                       ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    static constexpr uint32_t kernel1Bit = (1u << 0);
    static constexpr uint32_t kernel2Bit = (1u << 1);
    static constexpr uint32_t kernelAllMask = kernel1Bit | kernel2Bit;

    void setUp(bool createInOrder);
    void tearDown();

    std::unique_ptr<MutableCommandList> createMutableCmdList();
    Event *createTestEvent(bool cbEvent, bool signalScope, bool timestamp, bool external);
    void *allocateUsm(size_t size);
    NEO::GraphicsAllocation *getUsmAllocation(void *usm);
    void resizeKernelArg(uint32_t resize);
    void prepareKernelArg(uint16_t argIndex, L0::MCL::VariableType varType, uint32_t kernelMask);
    std::vector<L0::MCL::Variable *> getVariableList(uint64_t commandId, L0::MCL::VariableType varType, L0::Kernel *kernelOption);
    void overridePatchedScratchAddress(uint64_t scratchAddress);

    DebugManagerStateRestore restorer;
    ze_mutable_command_id_exp_desc_t mutableCommandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    ze_mutable_commands_exp_desc_t mutableCommandsDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};

    CmdListKernelLaunchParams testLaunchParams = {};

    std::vector<void *> usmAllocations;
    std::vector<ze_event_handle_t> eventHandles;
    std::vector<Event *> events;
    std::vector<void *> externalStorages;

    std::unique_ptr<MockImmutableData> mockKernelImmData;
    std::unique_ptr<MockImmutableData> mockKernelImmData2;
    std::unique_ptr<MutableCommandList> mutableCommandList;
    std::unique_ptr<MockKernel> kernel;
    std::unique_ptr<MockKernel> kernel2;
    std::unique_ptr<MockModule> module2;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData2;
    std::unique_ptr<VariableBackup<::NEO::HardwareInfo>> backupHwInfo;

    uint64_t commandId = 0;
    uint64_t externalEventCounterValue = 0x10;
    uint64_t *externalEventDeviceAddress = nullptr;
    uint64_t externalEventIncrementValue = 0x2;

    ze_event_pool_handle_t eventPoolHandle = nullptr;
    ze_kernel_handle_t kernelHandle = nullptr;
    ze_kernel_handle_t kernel2Handle = nullptr;
    ze_kernel_handle_t kernelMutationGroup[2] = {};

    ze_group_count_t testGroupCount = {1, 1, 1};

    ::NEO::EngineGroupType engineGroupType;

    uint16_t crossThreadOffset = defaultCrossThreadOffset;
    uint16_t nextArgOffset = defaultNextArgOffset;

    bool createInOrder;
};

template <bool createInOrderT>
struct MutableCommandListFixture : public MutableCommandListFixtureInit {
    void setUp() {
        MutableCommandListFixtureInit::setUp(createInOrderT);
    }
};

struct WhiteBoxMutableResidencyAllocations : public ::L0::MCL::MutableResidencyAllocations {
    using MutableResidencyAllocations::addedAllocations;
};

} // namespace ult
} // namespace L0
