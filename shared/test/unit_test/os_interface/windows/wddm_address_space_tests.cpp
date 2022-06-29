/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class WddmMockReserveAddress : public WddmMock {
  public:
    WddmMockReserveAddress(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}

    void *virtualAlloc(void *inPtr, size_t size, bool topDownHint) override {
        if (returnGood != 0) {
            return WddmMock::virtualAlloc(inPtr, size, topDownHint);
        }

        if (returnInvalidCount != 0) {
            returnInvalidIter++;
            if (returnInvalidIter > returnInvalidCount) {
                return WddmMock::virtualAlloc(inPtr, size, topDownHint);
            }
            if (returnNullCount != 0) {
                returnNullIter++;
                if (returnNullIter > returnNullCount) {
                    return nullptr;
                }
                return reinterpret_cast<void *>(0x1000);
            }
            return reinterpret_cast<void *>(0x1000);
        }

        return nullptr;
    }

    void virtualFree(void *ptr, size_t size) override {
        if ((ptr == reinterpret_cast<void *>(0x1000)) || (ptr == reinterpret_cast<void *>(0x0))) {
            return;
        }

        return WddmMock::virtualFree(ptr, size);
    }

    uint32_t returnGood = 0;
    uint32_t returnInvalidCount = 0;
    uint32_t returnInvalidIter = 0;
    uint32_t returnNullCount = 0;
    uint32_t returnNullIter = 0;
};

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsSuccessfulThenReturnReserveAddress) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMockReserveAddress(rootDeviceEnvironment);
    size_t size = 0x1000;
    void *reserve = nullptr;

    wddm->init();

    wddm->returnGood = 1;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsNullThenReturnNull) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMockReserveAddress(rootDeviceEnvironment);
    size_t size = 0x1000;
    void *reserve = nullptr;

    wddm->init();
    uintptr_t expectedReserve = 0;
    auto ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondSuccessfulThenReturnSecond) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMockReserveAddress(rootDeviceEnvironment);
    size_t size = 0x1000;
    void *reserve = nullptr;

    wddm->init();

    wddm->returnInvalidCount = 1;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenSecondIsInvalidThirdSuccessfulThenReturnThird) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMockReserveAddress(rootDeviceEnvironment);
    size_t size = 0x1000;
    void *reserve = nullptr;

    wddm->init();

    wddm->returnInvalidCount = 2;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondNullThenReturnSecondNull) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMockReserveAddress(rootDeviceEnvironment);
    size_t size = 0x1000;
    void *reserve = nullptr;

    wddm->init();

    wddm->returnInvalidCount = 2;
    wddm->returnNullCount = 1;
    uintptr_t expectedReserve = 0;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}
