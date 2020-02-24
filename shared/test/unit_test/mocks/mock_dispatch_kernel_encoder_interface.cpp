/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using ::testing::Return;

MockDispatchKernelEncoder::MockDispatchKernelEncoder() {
    EXPECT_CALL(*this, getIsaAllocation).WillRepeatedly(Return(&mockAllocation));
    EXPECT_CALL(*this, getSizeCrossThreadData).WillRepeatedly(Return(crossThreadSize));
    EXPECT_CALL(*this, getSizePerThreadData).WillRepeatedly(Return(perThreadSize));

    EXPECT_CALL(*this, getCrossThread).WillRepeatedly(Return(&dataCrossThread));
    EXPECT_CALL(*this, getPerThread).WillRepeatedly(Return(&dataPerThread));
    expectAnyMockFunctionCall();
}
void MockDispatchKernelEncoder::expectAnyMockFunctionCall() {
    EXPECT_CALL(*this, hasBarriers()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSlmTotalSize()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getBindingTableOffset()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getBorderColor()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSamplerTableOffset()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getNumSurfaceStates()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getNumSamplers()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSimdSize()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getPerThreadScratchSize()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getPerThreadExecutionMask()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSizePerThreadDataForWholeGroup()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSizeSurfaceStateHeapData()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getCountOffsets()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSizeOffsets()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getLocalWorkSize()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getNumGrfRequired()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getThreadsPerThreadGroupCount()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, hasGroupCounts()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSurfaceStateHeap()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getDynamicStateHeap()).Times(::testing::AnyNumber());
}