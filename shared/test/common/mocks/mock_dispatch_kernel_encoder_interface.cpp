/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using ::testing::Return;

MockDispatchKernelEncoder::MockDispatchKernelEncoder() {
    EXPECT_CALL(*this, getKernelDescriptor).WillRepeatedly(::testing::ReturnRef(kernelDescriptor));

    EXPECT_CALL(*this, getIsaAllocation).WillRepeatedly(Return(&mockAllocation));
    EXPECT_CALL(*this, getCrossThreadDataSize).WillRepeatedly(Return(crossThreadSize));
    EXPECT_CALL(*this, getPerThreadDataSize).WillRepeatedly(Return(perThreadSize));

    EXPECT_CALL(*this, getCrossThreadData).WillRepeatedly(Return(dataCrossThread));
    EXPECT_CALL(*this, getPerThreadData).WillRepeatedly(Return(dataPerThread));
    EXPECT_CALL(*this, getSlmPolicy()).WillRepeatedly(::testing::Return(SlmPolicy::SlmPolicyNone));

    groupSizes[0] = 32u;
    groupSizes[1] = groupSizes[2] = 1;
    EXPECT_CALL(*this, getGroupSize()).WillRepeatedly(Return(groupSizes));

    EXPECT_CALL(*this, requiresGenerationOfLocalIdsByRuntime).WillRepeatedly(Return(localIdGenerationByRuntime));

    expectAnyMockFunctionCall();
}

void MockDispatchKernelEncoder::expectAnyMockFunctionCall() {
    EXPECT_CALL(*this, getSlmTotalSize()).Times(::testing::AnyNumber());

    EXPECT_CALL(*this, getThreadExecutionMask()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getPerThreadDataSizeForWholeThreadGroup()).Times(::testing::AnyNumber());

    EXPECT_CALL(*this, getSurfaceStateHeapData()).Times(::testing::AnyNumber());
    EXPECT_CALL(*this, getSurfaceStateHeapDataSize()).Times(::testing::AnyNumber());

    EXPECT_CALL(*this, getDynamicStateHeapData()).Times(::testing::AnyNumber());
}
