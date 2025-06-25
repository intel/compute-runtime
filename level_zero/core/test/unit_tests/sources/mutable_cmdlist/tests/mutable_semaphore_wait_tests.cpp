/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutableSemaphoreWaitTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableSemaphoreWaitTest,
            givenMutableSemaphoreWaitRegularEventCommandWhenCommandIsNoopedThenBufferSpaceIsZeroed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::regularEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = Event::STATE_CLEARED;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->cmdBufferGpuPtr),
                                                             semaphoreAddress,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                             false, true, false, false, false);

    // prepare noop buffer for comparison
    uint8_t noopSpace[sizeof(MI_SEMAPHORE_WAIT)];
    memset(noopSpace, 0, sizeof(MI_SEMAPHORE_WAIT));

    // initialize command and mutable object
    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    mutableSemaphoreWait.noop();

    EXPECT_EQ(0, memcmp(noopSpace, this->cmdBufferGpuPtr, sizeof(MI_SEMAPHORE_WAIT)));
    EXPECT_EQ(inOrderPatchListIndex, mutableSemaphoreWait.getInOrderPatchListIndex());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableSemaphoreWaitTest,
            givenMutableSemaphoreWaitRegularEventCommandWhenCommandIsRestoredThenCommandIsProgrammed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::regularEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = Event::STATE_CLEARED;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

    // prepare buffer for comparison
    MI_SEMAPHORE_WAIT cmdSemaphore;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmdSemaphore,
                                                             semaphoreAddress + offset,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                             false, true, false, false, false);

    // noop command buffer and create mutable object
    memset(this->cmdBufferGpuPtr, 0, sizeof(MI_SEMAPHORE_WAIT));
    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    mutableSemaphoreWait.restoreWithSemaphoreAddress(semaphoreAddress);

    EXPECT_EQ(0, memcmp(&cmdSemaphore, this->cmdBufferGpuPtr, sizeof(MI_SEMAPHORE_WAIT)));
    EXPECT_EQ(inOrderPatchListIndex, mutableSemaphoreWait.getInOrderPatchListIndex());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableSemaphoreWaitTest,
            givenMutableSemaphoreWaitCbEventTimestampSyncCommandWhenCommandIsRestoredThenCommandIsProgrammed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::cbEventTimestampSyncWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = Event::STATE_CLEARED;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

    // prepare buffer for comparison
    MI_SEMAPHORE_WAIT cmdSemaphore;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmdSemaphore,
                                                             semaphoreAddress + offset,
                                                             data,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                             false, true, false, false, false);

    // noop command buffer and create mutable object
    memset(this->cmdBufferGpuPtr, 0, sizeof(MI_SEMAPHORE_WAIT));
    L0::MCL::MutableSemaphoreWaitHw<FamilyType> mutableSemaphoreWait(this->cmdBufferGpuPtr, offset, inOrderPatchListIndex, type, qwordDataIndirect);

    mutableSemaphoreWait.restoreWithSemaphoreAddress(semaphoreAddress);

    EXPECT_EQ(0, memcmp(&cmdSemaphore, this->cmdBufferGpuPtr, sizeof(MI_SEMAPHORE_WAIT)));
    EXPECT_EQ(inOrderPatchListIndex, mutableSemaphoreWait.getInOrderPatchListIndex());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableSemaphoreWaitTest,
            givenMutableSemaphoreWaitCbEventCommandWhenCommandIsRestoredThenCommandIsProgrammed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::cbEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = 0;
    size_t inOrderPatchListIndex = 2;
    bool qwordDataIndirect = false;

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

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableSemaphoreWaitTest,
            givenMutableSemaphoreWaitCommandWhenCommandValueOrAddressIsSetThenCommandValueIsChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto type = L0::MCL::MutableSemaphoreWait::Type::regularEventWait;
    size_t offset = 0x10;
    uint64_t semaphoreAddress = 0x26000;
    uint64_t data = Event::STATE_CLEARED;
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

    data = Event::STATE_SIGNALED;
    mutableSemaphoreWait.setSemaphoreValue(data);
    EXPECT_EQ(data, semWaitCommand->getSemaphoreDataDword());
}

} // namespace ult
} // namespace L0
