/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace WalkerPartition;
using namespace NEO;

struct WalkerPartitionPvcAndLaterTests : public ::testing::Test {
    void TearDown() override {
        auto initialCommandBufferPointer = cmdBuffer;
        if (checkForProperCmdBufferAddressOffset) {
            EXPECT_EQ(ptrDiff(cmdBufferAddress, initialCommandBufferPointer), totalBytesProgrammed);
        }
    }
    char cmdBuffer[4096u];
    uint32_t totalBytesProgrammed = 0u;
    void *cmdBufferAddress = cmdBuffer;
    bool checkForProperCmdBufferAddressOffset = true;
    bool synchronizeBeforeExecution = false;
};

HWTEST2_F(WalkerPartitionPvcAndLaterTests, givenMiAtomicWhenItIsProgrammedThenAllFieldsAreSetCorrectly, IsAtLeastXeHpcCore) {
    auto expectedUsedSize = sizeof(WalkerPartition::MI_ATOMIC<FamilyType>);
    uint64_t gpuAddress = 0xFFFFFFDFEEDBAC10llu;

    void *miAtomicAddress = cmdBufferAddress;
    programMiAtomic<FamilyType>(cmdBufferAddress,
                                totalBytesProgrammed, gpuAddress, true, MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

    auto miAtomic = genCmdCast<WalkerPartition::MI_ATOMIC<FamilyType> *>(miAtomicAddress);
    ASSERT_NE(nullptr, miAtomic);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);
    EXPECT_EQ(MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomic->getAtomicOpcode());
    EXPECT_EQ(0u, miAtomic->getDataSize());
    EXPECT_TRUE(miAtomic->getCsStall());
    EXPECT_EQ(MI_ATOMIC<FamilyType>::MEMORY_TYPE::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, miAtomic->getMemoryType());
    EXPECT_TRUE(miAtomic->getReturnDataControl());
    EXPECT_FALSE(miAtomic->getWorkloadPartitionIdOffsetEnable());
    auto memoryAddress = UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomic);

    EXPECT_EQ(gpuAddress, memoryAddress);
}

HWTEST2_F(WalkerPartitionPvcAndLaterTests, givenProgramBatchBufferStartCommandWhenItIsCalledThenCommandIsProgrammedCorrectly, IsAtLeastXeHpcCore) {
    auto expectedUsedSize = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
    uint64_t gpuAddress = 0xFFFFFFDFEEDBAC10llu;

    void *batchBufferStartAddress = cmdBufferAddress;
    WalkerPartition::programMiBatchBufferStart<FamilyType>(cmdBufferAddress, totalBytesProgrammed, gpuAddress, true, false);
    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(batchBufferStartAddress);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    if (FamilyType::gfxCoreFamily == IGFX_XE_HPC_CORE) {
        // bits 57-63 are zeroed
        EXPECT_EQ((gpuAddress & 0x1FFFFFFFFFFFFFF), batchBufferStart->getBatchBufferStartAddress());
    } else {
        // bits 48-63 are zeroed
        EXPECT_EQ((gpuAddress & 0xFFFFFFFFFFFF), batchBufferStart->getBatchBufferStartAddress());
    }
    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    EXPECT_FALSE(batchBufferStart->getEnableCommandCache());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    EXPECT_EQ(BATCH_BUFFER_START<FamilyType>::ADDRESS_SPACE_INDICATOR::ADDRESS_SPACE_INDICATOR_PPGTT, batchBufferStart->getAddressSpaceIndicator());
}
