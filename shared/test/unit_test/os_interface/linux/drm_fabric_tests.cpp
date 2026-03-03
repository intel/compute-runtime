/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_fabric.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(DrmFabricStubTest, givenDrmFabricStubWhenIsAvailableCalledThenReturnsFalse) {
    DrmFabricStub fabricStub;

    EXPECT_FALSE(fabricStub.isAvailable());
}

TEST(DrmFabricStubTest, givenDrmFabricStubWhenFdToHandleCalledThenReturnsMinusOne) {
    DrmFabricStub fabricStub;
    uint8_t handleData[32] = {0};

    int result = fabricStub.fdToHandle(123, handleData);

    EXPECT_EQ(-1, result);
}

TEST(DrmFabricStubTest, givenDrmFabricStubWhenHandleCloseCalledThenReturnsMinusOne) {
    DrmFabricStub fabricStub;
    uint8_t handleData[32] = {1, 2, 3};

    int result = fabricStub.handleClose(handleData);

    EXPECT_EQ(-1, result);
}

TEST(DrmFabricStubTest, givenDrmFabricStubWhenHandleToFdCalledThenReturnsMinusOne) {
    DrmFabricStub fabricStub;
    uint8_t handleData[32] = {1, 2, 3};
    int32_t outFd = 0;

    int result = fabricStub.handleToFd(handleData, outFd);

    EXPECT_EQ(-1, result);
    EXPECT_EQ(0, outFd);
}

} // namespace NEO
