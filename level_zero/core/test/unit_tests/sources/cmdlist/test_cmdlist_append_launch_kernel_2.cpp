/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {
struct CommandListAppendLaunchKernelSWTags : public Test<ModuleFixture> {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        NEO::DebugManager.flags.EnableSWTags.set(true);
        ModuleFixture::SetUp();
    }

    DebugManagerStateRestore dbgRestorer;
};

struct CommandListDualStorage : public Test<ModuleFixture> {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        ModuleFixture::SetUp();
    }
    void TearDown() override {
        ModuleFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST_F(CommandListDualStorage, givenIndirectDispatchWithSharedDualStorageMemoryWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::Kernel> kernel;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t *pThreadGroupDimensions = static_cast<ze_group_count_t *>(ptrOffset(alloc, sizeof(ze_group_count_t)));

    pThreadGroupDimensions->groupCountX = 3;
    pThreadGroupDimensions->groupCountY = 4;
    pThreadGroupDimensions->groupCountZ = 5;

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     pThreadGroupDimensions,
                                                     nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(pThreadGroupDimensions);
    ASSERT_NE(nullptr, allocData->cpuAllocation);
    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAllocation);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    uint32_t regAddress = 0;
    uint64_t gpuAddress = 0;
    auto expectedXAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountX)));
    auto expectedYAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountY)));
    auto expectedZAddress = reinterpret_cast<uint64_t>(ptrOffset(pThreadGroupDimensions, offsetof(ze_group_count_t, groupCountZ)));

    auto itor = find<MI_LOAD_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMX, regAddress);
    EXPECT_EQ(expectedXAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMY, regAddress);
    EXPECT_EQ(expectedYAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(GPUGPU_DISPATCHDIMZ, regAddress);
    EXPECT_EQ(expectedZAddress, gpuAddress);

    MI_STORE_REGISTER_MEM *cmd2 = nullptr;
    // Find group count cmds
    do {
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
        cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    } while (itor != cmdList.end() && cmd2->getRegisterAddress() != GPUGPU_DISPATCHDIMX);
    EXPECT_NE(cmdList.end(), itor);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(CS_GPR_R1, cmd2->getRegisterAddress());

    context->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendLaunchKernelThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    createKernel();
    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(6u, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {

        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::KernelName) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendEventResetIsCalledThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendEventReset(hEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    Event *event = Event::fromHandle(hEvent);
    event->destroy();
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendSignalEventThenThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendSignalEvent(hEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    Event *event = Event::fromHandle(hEvent);
    event->destroy();
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendWaitOnEventsThenThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendWaitOnEvents(1, &hEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    Event *event = Event::fromHandle(hEvent);
    event->destroy();
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendMemoryCopyThenThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    void *srcBuffer = reinterpret_cast<void *>(0x0F000000);
    void *dstBuffer = reinterpret_cast<void *>(0x0FF00000);
    size_t size = 1024;
    auto result = commandList->appendMemoryCopy(dstBuffer, srcBuffer, size, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 6u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendMemoryCopyRegionThenThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto cmdStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    void *srcBuffer = reinterpret_cast<void *>(0x0F000000);
    void *dstBuffer = reinterpret_cast<void *>(0x0FF00000);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    ze_result_t result = commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                                             srcBuffer, &sr, width, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 10u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameBegin) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);

    tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::CallNameEnd) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);
}

using CommandListArbitrationPolicyTest = Test<ModuleFixture>;

HWTEST_F(CommandListArbitrationPolicyTest, whenCreatingCommandListThenDefaultThreadArbitrationPolicyIsUsed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          0u,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    GenCmdList parsedCommandList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

HWTEST_F(CommandListArbitrationPolicyTest, whenCreatingCommandListThenChosenThreadArbitrationPolicyIsUsed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideThreadArbitrationPolicy.set(0);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          0u,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    GenCmdList parsedCommandList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::AgeBased),
                      cmd->getDataDword());
        }
    }
}

HWTEST_F(CommandListArbitrationPolicyTest, whenCommandListIsResetThenOriginalThreadArbitrationPolicyIsKept) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whitebox_cast(L0::CommandList::create(productFamily,
                                                                                          device,
                                                                                          NEO::EngineGroupType::RenderCompute,
                                                                                          0u,
                                                                                          returnValue)));
    EXPECT_NE(nullptr, commandList);
    EXPECT_NE(nullptr, commandList->commandContainer.getCommandStream());

    bool found;
    uint64_t originalThreadArbitrationPolicy = std::numeric_limits<uint64_t>::max();
    {
        GenCmdList parsedCommandList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
        EXPECT_GE(2u, miLoadImm.size());

        for (auto it : miLoadImm) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
                EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                          cmd->getDataDword());
                originalThreadArbitrationPolicy = cmd->getDataDword();
                found = false;
            }
        }
    }

    commandList->reset();

    {
        GenCmdList parsedCommandList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            parsedCommandList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommandList.begin(), parsedCommandList.end());
        EXPECT_GE(2u, miLoadImm.size());

        uint64_t newThreadArbitrationPolicy = std::numeric_limits<uint64_t>::max();
        for (auto it : miLoadImm) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
                EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                          cmd->getDataDword());
                newThreadArbitrationPolicy = cmd->getDataDword();
                EXPECT_EQ(originalThreadArbitrationPolicy, newThreadArbitrationPolicy);
            }
        }
    }
}

} // namespace ult
} // namespace L0
