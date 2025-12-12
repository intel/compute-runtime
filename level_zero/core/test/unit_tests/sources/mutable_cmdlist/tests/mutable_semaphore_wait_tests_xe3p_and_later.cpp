/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {
namespace ult {

using MutableSemaphoreWaitTest = Test<MutableHwCommandFixture>;

HWTEST2_F(MutableSemaphoreWaitTest, givenMutableSemaphoreWaitCbEventIndirectCommandWhenCommandIsRestoredThenCommandIsProgrammed, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::cbEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = 0;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = true;

    // prepare buffer for comparison
    MI_SEMAPHORE_WAIT cmdSemaphore;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmdSemaphore,
                                                             semaphoreAddress + offset,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             false, true, qwordDataIndirect, qwordDataIndirect, false);

    // noop command buffer and create mutable object
    memset(this->cmdBufferGpuPtr, 0, sizeof(MI_SEMAPHORE_WAIT));
    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    mutableSemaphoreWait.restoreWithSemaphoreAddress(semaphoreAddress);

    EXPECT_EQ(0, memcmp(&cmdSemaphore, this->cmdBufferGpuPtr, sizeof(MI_SEMAPHORE_WAIT)));
    EXPECT_EQ(inOrderPatchListIndex, mutableSemaphoreWait.getInOrderPatchListIndex());
}

HWTEST2_F(MutableSemaphoreWaitTest, givenMutableSemaphoreWaitCommandWhenCommandValueAndAddressIsSetThenCommandValueAndAddressIsChanged, IsXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitSemaphore.set(0);

    auto type = L0::MCL::MutableSemaphoreWait::Type::regularEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = 0x1234;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

    // prepare buffer for comparison
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->cmdBufferGpuPtr),
                                                             semaphoreAddress + offset,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                             false, true, false, false, false);

    auto semWaitCommand = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->cmdBufferGpuPtr);
    EXPECT_EQ((semaphoreAddress + offset), semWaitCommand->getSemaphoreGraphicsAddress());
    EXPECT_EQ(data, semWaitCommand->getSemaphoreDataDword());

    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    semaphoreAddress = 0x428000;
    mutableSemaphoreWait.setSemaphoreAddress(semaphoreAddress);

    EXPECT_EQ((semaphoreAddress + offset), semWaitCommand->getSemaphoreGraphicsAddress());

    data = 0x5678;
    mutableSemaphoreWait.setSemaphoreValue(data);
    EXPECT_EQ(data, semWaitCommand->getSemaphoreDataDword());
}

HWTEST2_F(MutableSemaphoreWaitTest, givenMutableSemaphoreWait64CommandWhenCommandValueAndAddressIsSetThenCommandValueAndAddressIsChanged, IsXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitSemaphore.set(1);

    auto type = L0::MCL::MutableSemaphoreWait::Type::regularEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = 0x1234;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

    // prepare buffer for comparison
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->cmdBufferGpuPtr),
                                                             semaphoreAddress + offset,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                             false, true, false, false, false);

    auto semWait64Command = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(this->cmdBufferGpuPtr);
    EXPECT_EQ((semaphoreAddress + offset), semWait64Command->getSemaphoreGraphicsAddress());
    EXPECT_EQ(data, semWait64Command->getSemaphoreDataDword());

    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    semaphoreAddress = 0x428000;
    mutableSemaphoreWait.setSemaphoreAddress(semaphoreAddress);

    EXPECT_EQ((semaphoreAddress + offset), semWait64Command->getSemaphoreGraphicsAddress());

    data = 0x5678;
    mutableSemaphoreWait.setSemaphoreValue(data);
    EXPECT_EQ(data, semWait64Command->getSemaphoreDataDword());
}

} // namespace ult
} // namespace L0
