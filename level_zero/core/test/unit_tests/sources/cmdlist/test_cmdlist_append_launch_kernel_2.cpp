/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/multi_tile_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "test_traits_common.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

namespace L0 {
namespace ult {
struct CommandListAppendLaunchKernelSWTags : public Test<ModuleFixture> {
    void SetUp() override {

        NEO::debugManager.flags.EnableSWTags.set(true);
        ModuleFixture::setUp();
    }

    DebugManagerStateRestore dbgRestorer;
};

struct CommandListDualStorage : public Test<ModuleFixture> {
    void SetUp() override {

        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        ModuleFixture::setUp();
    }
    void TearDown() override {
        ModuleFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST_F(CommandListDualStorage, givenIndirectDispatchWithSharedDualStorageMemoryWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    uint32_t globalWorkSizeXOffset = 0x20u;
    uint32_t globalWorkSizeYOffset = 0x24u;
    uint32_t globalWorkSizeZOffset = 0x28u;
    uint32_t numWorkGroupXOffset = 0x30u;
    uint32_t numWorkGroupYOffset = 0x34u;
    uint32_t numWorkGroupZOffset = 0x38u;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = globalWorkSizeXOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = globalWorkSizeYOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = globalWorkSizeZOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = numWorkGroupXOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = numWorkGroupYOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = numWorkGroupZOffset;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

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
                                                     *pThreadGroupDimensions,
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(static_cast<void *>(pThreadGroupDimensions));
    ASSERT_NE(nullptr, allocData->cpuAllocation);
    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAllocation);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

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

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimX, regAddress);
    EXPECT_EQ(expectedXAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimY, regAddress);
    EXPECT_EQ(expectedYAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimZ, regAddress);
    EXPECT_EQ(expectedZAddress, gpuAddress);

    MI_STORE_REGISTER_MEM *cmd2 = nullptr;
    // Find group count cmds
    do {
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
        cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    } while (itor != cmdList.end() && cmd2->getRegisterAddress() != RegisterOffsets::gpgpuDispatchDimX);
    EXPECT_NE(cmdList.end(), itor);

    auto groupCountStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimX);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupXOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimY);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupYOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimZ);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupZOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    auto workSizeStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workSizeStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR1);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeXOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeYOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeZOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListDualStorage, givenIndirectDispatchWithSharedDualStorageMemoryAndInlineDataWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.crossThreadDataSize = 0x60u;
    kernel.descriptor.kernelAttributes.flags.passInlineData = true;

    uint32_t globalWorkSizeXOffset = 0x40u;
    uint32_t globalWorkSizeYOffset = 0x44u;
    uint32_t globalWorkSizeZOffset = 0x48u;
    uint32_t numWorkGroupXOffset = 0x30u;
    uint32_t numWorkGroupYOffset = 0x34u;
    uint32_t numWorkGroupZOffset = 0x38u;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = globalWorkSizeXOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = globalWorkSizeYOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = globalWorkSizeZOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = numWorkGroupXOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = numWorkGroupYOffset;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = numWorkGroupZOffset;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

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
                                                     *pThreadGroupDimensions,
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(static_cast<void *>(pThreadGroupDimensions));
    ASSERT_NE(nullptr, allocData->cpuAllocation);
    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAllocation);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

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

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimX, regAddress);
    EXPECT_EQ(expectedXAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimY, regAddress);
    EXPECT_EQ(expectedYAddress, gpuAddress);

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    regAddress = cmd->getRegisterAddress();
    gpuAddress = cmd->getMemoryAddress();

    EXPECT_EQ(RegisterOffsets::gpgpuDispatchDimZ, regAddress);
    EXPECT_EQ(expectedZAddress, gpuAddress);

    MI_STORE_REGISTER_MEM *cmd2 = nullptr;
    // Find group count cmds
    do {
        itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
        cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    } while (itor != cmdList.end() && cmd2->getRegisterAddress() != RegisterOffsets::gpgpuDispatchDimX);
    EXPECT_NE(cmdList.end(), itor);

    auto groupCountStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimX);
    auto &compilerProductHelper = device->getCompilerProductHelper();

    bool heapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(heapless);

    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupXOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimY);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupYOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimZ);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupZOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    auto workSizeStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workSizeStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR1);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeXOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeYOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeZOffset - inlineDataSize);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernelSWTags, givenEnableSWTagsWhenAppendLaunchKernelThenTagsAreInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    createKernel();
    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(6u, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {

        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::kernelName) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendEventReset(hEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendSignalEvent(hEvent, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &hEvent);

    auto result = commandList->appendWaitOnEvents(1, &hEvent, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 4u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    void *srcBuffer = reinterpret_cast<void *>(0x0F000000);
    void *dstBuffer = reinterpret_cast<void *>(0x0FF00000);
    size_t size = 1024;
    CmdListMemoryCopyParams copyParams = {};
    auto result = commandList->appendMemoryCopy(dstBuffer, srcBuffer, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 6u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto usedSpaceBefore = cmdStream->getUsed();

    void *srcBuffer = reinterpret_cast<void *>(0x0F000000);
    void *dstBuffer = reinterpret_cast<void *>(0x0FF00000);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};
    ze_result_t result = commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                                             srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), 0), usedSpaceAfter));
    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    uint32_t expecteNumberOfNops = 10u;
    EXPECT_EQ(expecteNumberOfNops, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {
        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameBegin) == noop->getIdentificationNumber() &&
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
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::callNameEnd) == noop->getIdentificationNumber() &&
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

using CmdlistAppendLaunchKernelTests = Test<ModuleImmutableDataFixture>;
struct CmdlistAppendLaunchKernelWithImplicitArgsTests : CmdlistAppendLaunchKernelTests {

    void SetUp() override {
        CmdlistAppendLaunchKernelTests::SetUp();
        memset(&expectedImplicitArgs, 0, sizeof(expectedImplicitArgs));

        expectedImplicitArgs.header.structSize = ImplicitArgsV0::getSize();
        expectedImplicitArgs.header.structVersion = 0;

        expectedImplicitArgs.numWorkDim = 3;
        expectedImplicitArgs.simdWidth = 32;
        expectedImplicitArgs.localSizeX = 2;
        expectedImplicitArgs.localSizeY = 3;
        expectedImplicitArgs.localSizeZ = 4;
        expectedImplicitArgs.globalOffsetX = 1;
        expectedImplicitArgs.globalOffsetY = 2;
        expectedImplicitArgs.globalOffsetZ = 3;
        expectedImplicitArgs.groupCountX = 2;
        expectedImplicitArgs.groupCountY = 1;
        expectedImplicitArgs.groupCountZ = 3;
    }

    void TearDown() override {
        commandList.reset(nullptr);
        CmdlistAppendLaunchKernelTests::TearDown();
    }

    template <typename FamilyType>
    void dispatchKernelWithImplicitArgs() {
        expectedImplicitArgs.globalSizeX = expectedImplicitArgs.localSizeX * expectedImplicitArgs.groupCountX;
        expectedImplicitArgs.globalSizeY = expectedImplicitArgs.localSizeY * expectedImplicitArgs.groupCountY;
        expectedImplicitArgs.globalSizeZ = expectedImplicitArgs.localSizeZ * expectedImplicitArgs.groupCountZ;

        std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
        auto kernelDescriptor = mockKernelImmData->kernelDescriptor;

        UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(*kernelDescriptor);
        kernelDescriptor->kernelAttributes.simdSize = expectedImplicitArgs.simdWidth;
        kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[0] = workgroupDimOrder[0];
        kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[1] = workgroupDimOrder[1];
        kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[2] = workgroupDimOrder[2];
        createModuleFromMockBinary(0u, false, mockKernelImmData.get());

        auto kernel = std::make_unique<MockKernel>(module.get());

        ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernel->initialize(&kernelDesc);
        kernel->kernelRequiresGenerationOfLocalIdsByRuntime = kernelRequiresGenerationOfLocalIdsByRuntime;
        kernel->requiredWorkgroupOrder = requiredWorkgroupOrder;
        kernel->setCrossThreadData(sizeof(uint64_t));

        EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
        ASSERT_NE(nullptr, kernel->getImplicitArgs());

        kernel->setGroupSize(expectedImplicitArgs.localSizeX, expectedImplicitArgs.localSizeY, expectedImplicitArgs.localSizeZ);
        kernel->setGlobalOffsetExp(static_cast<uint32_t>(expectedImplicitArgs.globalOffsetX), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetY), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetZ));
        kernel->patchGlobalOffset();

        ze_result_t result{};
        commandList.reset(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        auto indirectHeap = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
        indirectHeapAllocation = indirectHeap->getGraphicsAllocation();

        ze_group_count_t groupCount{expectedImplicitArgs.groupCountX, expectedImplicitArgs.groupCountY, expectedImplicitArgs.groupCountZ};
        CmdListKernelLaunchParams launchParams = {};
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
        implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(reinterpret_cast<const ImplicitArgs *>(&expectedImplicitArgs), *kernelDescriptor, !kernelRequiresGenerationOfLocalIdsByRuntime, rootDeviceEnvironment);
        auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
        auto sizePerThreadDataForWholeGroup = kernel->getPerThreadDataSizeForWholeThreadGroup();
        EXPECT_EQ(indirectHeap->getUsed(), alignUp(sizeCrossThreadData + sizePerThreadDataForWholeGroup + implicitArgsProgrammingSize, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment()));

        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            expectedImplicitArgs.localIdTablePtr = indirectHeapAllocation->getGpuAddress();
        }
        expectedImplicitArgs.printfBufferPtr = kernel->getPrintfBufferAllocation()->getGpuAddress();
    }
    std::unique_ptr<L0::CommandList> commandList;
    GraphicsAllocation *indirectHeapAllocation = nullptr;
    ImplicitArgsV0 expectedImplicitArgs = {};
    std::array<uint8_t, 3> workgroupDimOrder{0, 1, 2};
    uint32_t implicitArgsProgrammingSize = 0u;

    bool kernelRequiresGenerationOfLocalIdsByRuntime = true;
    uint32_t requiredWorkgroupOrder = 0;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, CmdlistAppendLaunchKernelWithImplicitArgsTests, givenXeHpAndLaterPlatformWhenAppendLaunchKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithLocalIds) {
    std::array<uint16_t, 3> localSize{2, 3, 4};
    size_t totalLocalSize = localSize[0] * localSize[1] * localSize[2];

    expectedImplicitArgs.localSizeX = localSize[0];
    expectedImplicitArgs.localSizeY = localSize[1];
    expectedImplicitArgs.localSizeZ = localSize[2];

    dispatchKernelWithImplicitArgs<FamilyType>();

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth);
    auto numGrf = GrfConfig::defaultGrfNumber;
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize(), MemoryConstants::cacheLineSize);
    const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, workgroupDimOrder, false, grfSize, numGrf, rootDeviceEnvironment);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();
    size_t sizeForLocalIds = NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, numGrf, 3u, totalLocalSize, rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CmdlistAppendLaunchKernelWithImplicitArgsTests, givenPreXeHpPlatformWhenAppendLaunchKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithoutLocalIds) {
    dispatchKernelWithImplicitArgs<FamilyType>();

    auto implicitArgsInIndirectData = indirectHeapAllocation->getUnderlyingBuffer();
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));

    auto crossThreadDataInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), ImplicitArgsV0::getAlignedSize());

    auto programmedImplicitArgsGpuVA = reinterpret_cast<uint64_t *>(crossThreadDataInIndirectData)[0];
    EXPECT_EQ(indirectHeapAllocation->getGpuAddress(), programmedImplicitArgsGpuVA);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CmdlistAppendLaunchKernelWithImplicitArgsTests, givenXeHpAndLaterPlatformAndHwGeneratedLocalIdsWhenAppendLaunchKernelWithImplicitArgsThenImplicitArgsLocalIdsRespectWalkOrder) {
    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    kernelRequiresGenerationOfLocalIdsByRuntime = false;
    requiredWorkgroupOrder = 2; // walk order 1 0 2

    std::array<uint8_t, 3> expectedDimOrder = {1, 0, 2};

    std::array<uint16_t, 3> localSize{2, 3, 4};
    size_t totalLocalSize = localSize[0] * localSize[1] * localSize[2];

    expectedImplicitArgs.localSizeX = localSize[0];
    expectedImplicitArgs.localSizeY = localSize[1];
    expectedImplicitArgs.localSizeZ = localSize[2];

    dispatchKernelWithImplicitArgs<FamilyType>();

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth);
    auto numGrf = GrfConfig::defaultGrfNumber;
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - ImplicitArgsV0::getSize(), MemoryConstants::cacheLineSize);
    const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, expectedDimOrder, false, grfSize, numGrf, rootDeviceEnvironment);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();
    size_t sizeForLocalIds = NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, numGrf, 3u, totalLocalSize, rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CmdlistAppendLaunchKernelWithImplicitArgsTests, givenXeHpAndLaterPlatformWhenAppendLaunchKernelWithImplicitArgsAndSimd1ThenLocalIdsAreGeneratedCorrectly) {
    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    expectedImplicitArgs.simdWidth = 1;
    expectedImplicitArgs.localSizeX = 2;
    expectedImplicitArgs.localSizeY = 2;
    expectedImplicitArgs.localSizeZ = 1;

    dispatchKernelWithImplicitArgs<FamilyType>();

    uint16_t expectedLocalIds[][3] = {{0, 0, 0},
                                      {0, 1, 0},
                                      {0, 0, 1},
                                      {0, 1, 1}};

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeof(expectedLocalIds)));

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();

    EXPECT_EQ(alignUp(sizeof(expectedLocalIds), MemoryConstants::cacheLineSize), localIdsProgrammingSize);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

HWTEST_F(CmdlistAppendLaunchKernelTests, givenKernelWithoutImplicitArgsWhenAppendLaunchKernelThenImplicitArgsAreNotSentToIndirectHeap) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_FALSE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    EXPECT_EQ(nullptr, kernel->getImplicitArgs());

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result{};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto indirectHeap = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);

    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto sizePerThreadDataForWholeGroup = kernel->getPerThreadDataSizeForWholeThreadGroup();
    EXPECT_EQ(indirectHeap->getUsed(), sizeCrossThreadData + sizePerThreadDataForWholeGroup);
}

HWTEST2_F(CmdlistAppendLaunchKernelTests, givenKernelWithScratchAndPrivateWhenAppendLaunchKernelThenCmdListHasCorrectPrivateAndScratchSizesSet, IsAtLeastXeCore) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x200;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x100;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_FALSE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    EXPECT_EQ(nullptr, kernel->getImplicitArgs());

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result{};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(commandList->getCommandListPerThreadScratchSize(1u), static_cast<uint32_t>(0x100));
    EXPECT_EQ(commandList->getCommandListPerThreadScratchSize(0u), static_cast<uint32_t>(0x200));
}

struct CmdlistAppendLaunchKernelTestsWithoutHeaplessMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastXeCore::isMatched<productFamily>() && !(TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessRequired);
    }
};

HWTEST2_F(CmdlistAppendLaunchKernelTests, givenGlobalBindlessAllocatorAndKernelWithPrivateScratchWhenAppendLaunchKernelThenCmdContainerHasBindfulSSHAllocated, CmdlistAppendLaunchKernelTestsWithoutHeaplessMatcher) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    UnitTestSetter::disableHeapless(restorer);

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    execEnv->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x40;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_FALSE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    EXPECT_EQ(nullptr, kernel->getImplicitArgs());

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result{};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(nullptr, commandList->getCmdContainer().getIndirectHeap(HeapType::surfaceState));

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, commandList->getCmdContainer().getIndirectHeap(HeapType::surfaceState));
}

HWTEST2_F(CmdlistAppendLaunchKernelTests, givenGlobalBindlessAllocatorAndKernelWithScratchWhenAppendLaunchKernelThenCmdContainerHasBindfulSSHAllocated, CmdlistAppendLaunchKernelTestsWithoutHeaplessMatcher) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    UnitTestSetter::disableHeapless(restorer);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    execEnv->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x40;
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_FALSE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    EXPECT_EQ(nullptr, kernel->getImplicitArgs());

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result{};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(nullptr, commandList->getCmdContainer().getIndirectHeap(HeapType::surfaceState));

    ze_group_count_t groupCount = {3, 2, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_NE(nullptr, commandList->getCmdContainer().getIndirectHeap(HeapType::surfaceState));
}

HWTEST_F(CmdlistAppendLaunchKernelTests, whenEncodingWorkDimForIndirectDispatchThenSizeIsProperlyEstimated) {

    Mock<::L0::KernelImp> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    {
        uint32_t groupSize[] = {1, 1, 1};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, false);
        auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->getCmdContainer(), 0x4, 0u, groupSize, nullptr);
        auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 2};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, false);
        auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->getCmdContainer(), 0x4, 0u, groupSize, nullptr);
        auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 1};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, true);
        auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->getCmdContainer(), 0x2, 0u, groupSize, nullptr);
        auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 2};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, true);
        auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->getCmdContainer(), 0x2, 0u, groupSize, nullptr);
        auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST_F(CommandListAppendLaunchKernel, givenCooperativeAndNonCooperativeKernelsWhenAppendLaunchCooperativeKernelIsCalledThenReturnSuccess) {
    Mock<::L0::KernelImp> kernel;
    std::unique_ptr<Module> pMockModule = std::make_unique<Mock<Module>>(device, nullptr);
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithSlmSizeExceedingLocalMemorySizeWhenAppendLaunchKernelWithParamsIsCalledThenDebugMsgErrIsPrintedAndOutOfDeviceMemoryIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);

    Mock<::L0::KernelImp> kernel;
    std::unique_ptr<Module> pMockModule = std::make_unique<Mock<Module>>(device, nullptr);
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    CmdListKernelLaunchParams launchParams = {};

    ::testing::internal::CaptureStderr();

    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);

    auto localMemSize = static_cast<uint32_t>(device->getNEODevice()->getDeviceInfo().localMemSize);
    kernel.immutableData.kernelDescriptor->kernelAttributes.slmInlineSize = localMemSize + 10u;

    ::testing::internal::CaptureStderr();

    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    const char *pStr = nullptr;
    std::string emptyString = "";
    zeDriverGetLastErrorDescription(device->getDriverHandle(), &pStr);
    EXPECT_NE(0, strcmp(pStr, emptyString.c_str()));
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);

    output = testing::internal::GetCapturedStderr();
    const auto &slmInlineSize = kernel.immutableData.kernelDescriptor->kernelAttributes.slmInlineSize;
    std::string expectedOutput = "Size of SLM (" + std::to_string(slmInlineSize) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_EQ(expectedOutput, output);
}

HWTEST_F(CommandListAppendLaunchKernel, givenTwoKernelPrivateAllocsWhichTogetherExceedGlobalMemSizeWhenAppendLaunchKernelWithParamsIsCalledOnlyThenAllocationHappens) {

    auto devInfo = device->getNEODevice()->getDeviceInfo();
    auto kernelsNb = 2u;
    uint32_t margin1KB = (1 << 10);
    auto overAllocMinSize = static_cast<uint32_t>(devInfo.globalMemSize / kernelsNb / devInfo.computeUnitsUsedForScratch) + margin1KB;
    auto kernelNames = std::array<std::string, 2u>{"test1", "test2"};

    auto &kernelImmDatas = this->module->kernelImmDatas;
    for (size_t i = 0; i < kernelsNb; i++) {
        auto &kernelDesc = const_cast<KernelDescriptor &>(kernelImmDatas[i]->getDescriptor());
        kernelDesc.kernelAttributes.perHwThreadPrivateMemorySize = overAllocMinSize + static_cast<uint32_t>(i * MemoryConstants::cacheLineSize);
        kernelDesc.kernelAttributes.flags.usesPrintf = false;
        kernelDesc.kernelMetadata.kernelName = kernelNames[i];
    }

    EXPECT_FALSE(this->module->shouldAllocatePrivateMemoryPerDispatch());
    this->module->checkIfPrivateMemoryPerDispatchIsNeeded();
    EXPECT_TRUE(this->module->shouldAllocatePrivateMemoryPerDispatch());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    pCommandList->device = this->module->getDevice();
    auto memoryMgr = static_cast<OsAgnosticMemoryManager *>(pCommandList->device->getNEODevice()->getExecutionEnvironment()->memoryManager.get());
    memoryMgr->turnOnFakingBigAllocations();

    auto kernels = std::vector<std::unique_ptr<WhiteBox<::L0::KernelImp>>>();
    for (size_t i = 0; i < kernelsNb; i++) {
        EXPECT_EQ(pCommandList->getOwnedPrivateAllocationsSize(), i);
        kernels.push_back(this->createKernelWithName(kernelNames[i]));
        // This function is called by appendLaunchKernelWithParams
        pCommandList->allocateOrReuseKernelPrivateMemoryIfNeeded(kernels[i].get(),
                                                                 kernels[i]->getKernelDescriptor().kernelAttributes.perHwThreadPrivateMemorySize);
        EXPECT_EQ(pCommandList->getOwnedPrivateAllocationsSize(), i + 1);
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenTwoKernelPrivateAllocsWhichDontExceedGlobalMemSizeWhenAppendLaunchKernelWithParamsIsCalledThenNoAllocationIsDone) {

    auto devInfo = device->getNEODevice()->getDeviceInfo();
    auto kernelsNb = 2u;
    uint32_t margin128KB = 131072u;
    auto underAllocSize = static_cast<uint32_t>(devInfo.globalMemSize / kernelsNb / devInfo.computeUnitsUsedForScratch) - margin128KB;
    auto kernelNames = std::array<std::string, 2u>{"test1", "test2"};

    auto &kernelImmDatas = this->module->kernelImmDatas;
    for (size_t i = 0; i < kernelsNb; i++) {
        auto &kernelDesc = const_cast<KernelDescriptor &>(kernelImmDatas[i]->getDescriptor());
        kernelDesc.kernelAttributes.perHwThreadPrivateMemorySize = underAllocSize;
        kernelDesc.kernelAttributes.flags.usesPrintf = false;
        kernelDesc.kernelMetadata.kernelName = kernelNames[i];
    }

    EXPECT_FALSE(this->module->shouldAllocatePrivateMemoryPerDispatch());
    this->module->checkIfPrivateMemoryPerDispatchIsNeeded();
    EXPECT_FALSE(this->module->shouldAllocatePrivateMemoryPerDispatch());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    pCommandList->device = this->module->getDevice();
    auto memoryMgr = static_cast<OsAgnosticMemoryManager *>(pCommandList->device->getNEODevice()->getExecutionEnvironment()->memoryManager.get());
    memoryMgr->turnOnFakingBigAllocations();

    auto kernels = std::vector<std::unique_ptr<WhiteBox<::L0::KernelImp>>>();
    for (size_t i = 0; i < kernelsNb; i++) {
        EXPECT_EQ(pCommandList->getOwnedPrivateAllocationsSize(), 0u);
        kernels.push_back(this->createKernelWithName(kernelNames[i]));
        // This function is called by appendLaunchKernelWithParams
        pCommandList->allocateOrReuseKernelPrivateMemoryIfNeeded(kernels[i].get(),
                                                                 kernels[i]->getKernelDescriptor().kernelAttributes.perHwThreadPrivateMemorySize);
        EXPECT_EQ(pCommandList->getOwnedPrivateAllocationsSize(), 0u);
    }
}
HWTEST2_F(CommandListAppendLaunchKernel, GivenDebugToggleSetWhenUpdateStreamPropertiesIsCalledThenCorrectThreadArbitrationPolicyIsSet, IsHeapfulSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto defaultThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    auto nonDefaultThreadArbitrationPolicy = defaultThreadArbitrationPolicy + 1;

    Mock<::L0::KernelImp> kernel;
    std::unique_ptr<Module> pMockModule = std::make_unique<Mock<Module>>(device, nullptr);
    kernel.module = pMockModule.get();

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    // initial kernel with no policy preference
    const ze_group_count_t launchKernelArgs = {};
    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(defaultThreadArbitrationPolicy, pCommandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);

    // policy changed to non-default state
    pCommandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value = nonDefaultThreadArbitrationPolicy;
    // another kernel with no policy preference - do not update policy
    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(nonDefaultThreadArbitrationPolicy, pCommandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);

    // another kernel with no policy preference, this time with debug toggle set - update policy back to default value
    debugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.set(true);
    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(defaultThreadArbitrationPolicy, pCommandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);
}

using MultiTileCommandListAppendLaunchKernelXeHpCoreTest = Test<MultiTileCommandListAppendLaunchKernelFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileCommandListAppendLaunchKernelXeHpCoreTest, givenImplicitScalingEnabledWhenAppendingKernelWithEventThenAllEventPacketsAreUsed) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.index = 0;
    auto deviceHandle = device->toHandle();

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device->getDriverHandle(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Event> event(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_event_handle_t hEventHandle = event->toHandle();

    EXPECT_EQ(4u, commandList->partitionCount);

    ze_group_count_t groupCount{256, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(4u, event->getPacketsInUse());

    EXPECT_EQ(4u, commandList->partitionCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileCommandListAppendLaunchKernelXeHpCoreTest, givenDebugVariableSetWhenUsingNonTimestampEventThenDontOverridePostSyncMode) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYN_DATA = decltype(FamilyType::template getPostSyncType<WalkerType>());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.index = 0;
    auto deviceHandle = device->toHandle();

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(device->getDriverHandle(), context, 1, &deviceHandle, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Event> event(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    EXPECT_FALSE(event->isUsingContextEndOffset());

    ze_event_handle_t hEventHandle = event->toHandle();

    ze_group_count_t groupCount{256, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(4u, event->getPacketsInUse());
    EXPECT_EQ(4u, commandList->partitionCount);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandList->getCmdContainer().getCommandStream()->getCpuBase(), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(it, cmdList.end());
    auto walker = genCmdCast<WalkerType *>(*it);

    EXPECT_TRUE(walker->getWorkloadPartitionEnable());
    auto &postSync = walker->getPostSync();
    EXPECT_EQ(POSTSYN_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
}

HWTEST2_F(MultiTileCommandListAppendLaunchKernelXeHpCoreTest, givenCooperativeKernelWhenAppendingKernelsThenSetProperPartitionSize, IsAtLeastXeCore) {

    using WalkerType = typename FamilyType::DefaultWalkerType;

    ze_group_count_t groupCount{16, 1, 1};

    auto commandListWithNonCooperativeKernel = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandListWithNonCooperativeKernel->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeBefore = commandListWithNonCooperativeKernel->getCmdContainer().getCommandStream()->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    result = commandListWithNonCooperativeKernel->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = commandListWithNonCooperativeKernel->getCmdContainer().getCommandStream()->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandListWithNonCooperativeKernel->getCmdContainer().getCommandStream()->getCpuBase(), sizeBefore), sizeAfter - sizeBefore));
    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorWalker, cmdList.end());
    auto walker = genCmdCast<WalkerType *>(*itorWalker);
    ASSERT_NE(nullptr, walker);

    EXPECT_TRUE(walker->getWorkloadPartitionEnable());
    EXPECT_EQ(4u, walker->getPartitionSize());

    auto commandListWithCooperativeKernel = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandListWithCooperativeKernel->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    sizeBefore = commandListWithCooperativeKernel->getCmdContainer().getCommandStream()->getUsed();
    launchParams.isCooperative = true;
    result = commandListWithCooperativeKernel->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = commandListWithCooperativeKernel->getCmdContainer().getCommandStream()->getUsed();
    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandListWithCooperativeKernel->getCmdContainer().getCommandStream()->getCpuBase(), sizeBefore), sizeAfter - sizeBefore));

    const auto &gfxCoreHelper = device->getGfxCoreHelper();
    bool singleTileExecImplicitScalingRequired = gfxCoreHelper.singleTileExecImplicitScalingRequired(true);
    itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorWalker, cmdList.end());

    auto walker2 = genCmdCast<WalkerType *>(*itorWalker);
    ASSERT_NE(nullptr, walker2);
    EXPECT_TRUE(walker2->getWorkloadPartitionEnable());
    if (singleTileExecImplicitScalingRequired) {
        EXPECT_EQ(16u, walker2->getPartitionSize());
    } else {
        EXPECT_EQ(4u, walker2->getPartitionSize());
    }
}

HWTEST2_F(MultiTileCommandListAppendLaunchKernelXeHpCoreTest,
          givenRegularCommandListWhenSynchronizationRequiredThenExpectJumpingBbStartCommandToSecondary, IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);

    ze_group_count_t groupCount{128, 1, 1};

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.get(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto it = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(it, cmdList.end());

    auto walker = genCmdCast<WalkerType *>(*it);
    EXPECT_TRUE(walker->getWorkloadPartitionEnable());

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    if (commandList->dispatchCmdListBatchBufferAsPrimary) {
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());
    } else {
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());
    }
}

} // namespace ult
} // namespace L0
