/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gen9/reg_configs.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

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

    auto groupCountStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMX);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupXOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMY);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupYOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMZ);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupZOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    auto workSizeStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workSizeStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeXOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeYOffset);

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

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeZOffset);

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListDualStorage, givenIndirectDispatchWithSharedDualStorageMemoryAndInlineDataWhenAppendingThenWorkGroupCountAndGlobalWorkSizeIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using INLINE_DATA = typename FamilyType::INLINE_DATA;

    Mock<::L0::Kernel> kernel;
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

    auto groupCountStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMX);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupXOffset - sizeof(INLINE_DATA));

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMY);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupYOffset - sizeof(INLINE_DATA));

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    groupCountStoreRegisterMemCmd.setRegisterAddress(GPUGPU_DISPATCHDIMZ);
    groupCountStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + numWorkGroupZOffset - sizeof(INLINE_DATA));

    EXPECT_EQ(cmd2->getRegisterAddress(), groupCountStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), groupCountStoreRegisterMemCmd.getMemoryAddress());

    auto workSizeStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workSizeStoreRegisterMemCmd.setRegisterAddress(CS_GPR_R1);

    // Find workgroup size cmds
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeXOffset - sizeof(INLINE_DATA));

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd2 = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeYOffset - sizeof(INLINE_DATA));

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

    workSizeStoreRegisterMemCmd.setMemoryAddress(commandList->commandContainer.getIndirectHeap(HeapType::INDIRECT_OBJECT)->getGraphicsAllocation()->getGpuAddress() + globalWorkSizeZOffset - sizeof(INLINE_DATA));

    EXPECT_EQ(cmd2->getRegisterAddress(), workSizeStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd2->getMemoryAddress(), workSizeStoreRegisterMemCmd.getMemoryAddress());

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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
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

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
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

using CmdlistAppendLaunchKernelTests = Test<ModuleImmutableDataFixture>;
HWTEST_F(CmdlistAppendLaunchKernelTests, givenKernelWithImplicitArgsWhenAppendLaunchKernelThenImplicitArgsAreSentToIndirectHeap) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;
    auto simd = kernelDescriptor->kernelAttributes.simdSize;
    kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[0] = 2;
    kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[1] = 1;
    kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[2] = 0;
    createModuleFromBinary(0u, false, mockKernelImmData.get());

    auto kernel = std::make_unique<MockKernel>(module.get());

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel->initialize(&kernelDesc);

    EXPECT_TRUE(kernel->getKernelDescriptor().kernelAttributes.flags.requiresImplicitArgs);
    ASSERT_NE(nullptr, kernel->getImplicitArgs());

    kernel->setGroupSize(4, 5, 6);
    kernel->setGroupCount(3, 2, 1);
    kernel->setGlobalOffsetExp(1, 2, 3);
    kernel->patchGlobalOffset();

    ze_result_t result{};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto indirectHeap = commandList->commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    memset(indirectHeap->getSpace(0), 0, kernel->getSizeForImplicitArgsPatching());

    ze_group_count_t groupCount{3, 2, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto sizePerThreadDataForWholeGroup = kernel->getPerThreadDataSizeForWholeThreadGroup();
    EXPECT_EQ(indirectHeap->getUsed(), sizeCrossThreadData + sizePerThreadDataForWholeGroup + kernel->getSizeForImplicitArgsPatching());

    ImplicitArgs expectedImplicitArgs{sizeof(ImplicitArgs)};
    expectedImplicitArgs.numWorkDim = 3;
    expectedImplicitArgs.simdWidth = simd;
    expectedImplicitArgs.localSizeX = 4;
    expectedImplicitArgs.localSizeY = 5;
    expectedImplicitArgs.localSizeZ = 6;
    expectedImplicitArgs.globalSizeX = 12;
    expectedImplicitArgs.globalSizeY = 10;
    expectedImplicitArgs.globalSizeZ = 6;
    expectedImplicitArgs.globalOffsetX = 1;
    expectedImplicitArgs.globalOffsetY = 2;
    expectedImplicitArgs.globalOffsetZ = 3;
    expectedImplicitArgs.groupCountX = 3;
    expectedImplicitArgs.groupCountY = 2;
    expectedImplicitArgs.groupCountZ = 1;
    expectedImplicitArgs.localIdTablePtr = indirectHeap->getGraphicsAllocation()->getGpuAddress();
    expectedImplicitArgs.printfBufferPtr = kernel->getPrintfBufferAllocation()->getGpuAddress();

    auto sizeForImplicitArgPatching = kernel->getSizeForImplicitArgsPatching();

    EXPECT_LT(0u, sizeForImplicitArgPatching);

    auto localIdsProgrammingSize = sizeForImplicitArgPatching - sizeof(ImplicitArgs);

    auto expectedLocalIds = alignedMalloc(localIdsProgrammingSize, 64);
    memset(expectedLocalIds, 0, localIdsProgrammingSize);
    constexpr uint32_t grfSize = sizeof(typename FamilyType::GRF);
    NEO::generateLocalIDs(expectedLocalIds, simd,
                          std::array<uint16_t, 3>{{4, 5, 6}},
                          std::array<uint8_t, 3>{{kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[0],
                                                  kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[1],
                                                  kernelDescriptor->kernelAttributes.workgroupDimensionsOrder[2]}},
                          false, grfSize);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeap->getCpuBase(), localIdsProgrammingSize));
    auto pImplicitArgs = reinterpret_cast<ImplicitArgs *>(ptrOffset(indirectHeap->getCpuBase(), localIdsProgrammingSize));
    EXPECT_EQ(0, memcmp(&expectedImplicitArgs, pImplicitArgs, sizeof(ImplicitArgs)));

    alignedFree(expectedLocalIds);
}
HWTEST_F(CmdlistAppendLaunchKernelTests, givenKernelWithoutImplicitArgsWhenAppendLaunchKernelThenImplicitArgsAreNotSentToIndirectHeap) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    createModuleFromBinary(0u, false, mockKernelImmData.get());

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount = {3, 2, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto indirectHeap = commandList->commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);

    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto sizePerThreadDataForWholeGroup = kernel->getPerThreadDataSizeForWholeThreadGroup();
    EXPECT_EQ(indirectHeap->getUsed(), sizeCrossThreadData + sizePerThreadDataForWholeGroup);

    auto sizeForImplicitArgPatching = kernel->getSizeForImplicitArgsPatching();

    EXPECT_EQ(0u, sizeForImplicitArgPatching);
}

HWTEST2_F(CmdlistAppendLaunchKernelTests, givenKernelWitchScratchAndPrivateWhenAppendLaunchKernelThenCmdListHasCorrectPrivateAndScratchSizesSet, IsAtLeastXeHpCore) {
    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    auto kernelDescriptor = mockKernelImmData->kernelDescriptor;
    kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x200;
    kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x100;
    createModuleFromBinary(0u, false, mockKernelImmData.get());

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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount = {3, 2, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(commandList->getCommandListPerThreadPrivateScratchSize(), static_cast<uint32_t>(0x100));
    EXPECT_EQ(commandList->getCommandListPerThreadScratchSize(), static_cast<uint32_t>(0x200));
}

HWTEST_F(CmdlistAppendLaunchKernelTests, whenEncodingWorkDimForIndirectDispatchThenSizeIsProperlyEstimated) {

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    {
        uint32_t groupSize[] = {1, 1, 1};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, false);
        auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->commandContainer, 0x4, 0u, groupSize);
        auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 2};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, false);
        auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->commandContainer, 0x4, 0u, groupSize);
        auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 1};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, true);
        auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->commandContainer, 0x2, 0u, groupSize);
        auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
    {
        uint32_t groupSize[] = {1, 1, 2};
        auto estimate = EncodeIndirectParams<FamilyType>::getCmdsSizeForSetWorkDimIndirect(groupSize, true);
        auto sizeBefore = commandList->commandContainer.getCommandStream()->getUsed();
        EncodeIndirectParams<FamilyType>::setWorkDimIndirect(commandList->commandContainer, 0x2, 0u, groupSize);
        auto sizeAfter = commandList->commandContainer.getCommandStream()->getUsed();
        EXPECT_LE(sizeAfter - sizeBefore, estimate);
    }
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeAndNonCooperativeKernelsWhenAppendLaunchCooperativeKernelIsCalledThenReturnError, IsAtLeastSkl) {
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    bool isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

struct MultiTileCommandListAppendLaunchFunctionXeHpCoreFixture : public MultiDeviceModuleFixture {
    void SetUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);

        MultiDeviceFixture::numRootDevices = 1u;
        MultiDeviceFixture::numSubDevices = 4u;

        MultiDeviceModuleFixture::SetUp();
        createModuleFromBinary(0u);
        createKernel(0u);

        device = driverHandle->devices[0];

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

        ze_result_t returnValue;
        commandList = whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    }

    void TearDown() {
        commandList->destroy();
        contextImp->destroy();

        MultiDeviceModuleFixture::TearDown();
    }

    ContextImp *contextImp = nullptr;
    WhiteBox<::L0::CommandList> *commandList = nullptr;
    L0::Device *device = nullptr;
    VariableBackup<bool> backup{&NEO::ImplicitScaling::apiSupport, true};
};

using MultiTileCommandListAppendLaunchFunctionXeHpCoreTest = Test<MultiTileCommandListAppendLaunchFunctionXeHpCoreFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileCommandListAppendLaunchFunctionXeHpCoreTest, givenImplicitScalingEnabledWhenAppendingKernelWithEventThenAllEventPacketsAreUsed) {
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
    std::unique_ptr<L0::Event> event(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_event_handle_t hEventHandle = event->toHandle();

    EXPECT_EQ(4u, commandList->partitionCount);

    ze_group_count_t groupCount{256, 1, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, hEventHandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(4u, event->getPacketsInUse());

    EXPECT_EQ(4u, commandList->partitionCount);
}

HWTEST2_F(MultiTileCommandListAppendLaunchFunctionXeHpCoreTest, givenCooperativeKernelWhenAppendingKernelsThenDoNotUseImplicitScaling, IsAtLeastXeHpCore) {
    ze_group_count_t groupCount{1, 1, 1};

    auto commandListWithNonCooperativeKernel = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandListWithNonCooperativeKernel->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeBefore = commandListWithNonCooperativeKernel->commandContainer.getCommandStream()->getUsed();
    result = commandListWithNonCooperativeKernel->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = commandListWithNonCooperativeKernel->commandContainer.getCommandStream()->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandListWithNonCooperativeKernel->commandContainer.getCommandStream()->getCpuBase(), sizeBefore), sizeAfter - sizeBefore));
    auto itorWalker = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    auto cmd = genCmdCast<typename FamilyType::WALKER_TYPE *>(*itorWalker);
    EXPECT_TRUE(cmd->getWorkloadPartitionEnable());

    auto commandListWithCooperativeKernel = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandListWithCooperativeKernel->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    sizeBefore = commandListWithCooperativeKernel->commandContainer.getCommandStream()->getUsed();
    result = commandListWithCooperativeKernel->appendLaunchKernelWithParams(kernel->toHandle(), &groupCount, nullptr, false, false, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = commandListWithCooperativeKernel->commandContainer.getCommandStream()->getUsed();
    cmdList.clear();
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandListWithNonCooperativeKernel->commandContainer.getCommandStream()->getCpuBase(), sizeBefore), sizeAfter - sizeBefore));

    itorWalker = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    cmd = genCmdCast<typename FamilyType::WALKER_TYPE *>(*itorWalker);
    EXPECT_TRUE(cmd->getWorkloadPartitionEnable());
}

} // namespace ult
} // namespace L0
