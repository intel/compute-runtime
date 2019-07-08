/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/options.h"
#include "test.h"
#include "unit_tests/mocks/mock_wddm.h"

using namespace NEO;

class WddmMockReserveAddress : public WddmMock {
  public:
    WddmMockReserveAddress() : WddmMock() {}

    void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) override {
        if (returnGood != 0) {
            return WddmMock::virtualAlloc(inPtr, size, flags, type);
        }

        if (returnInvalidCount != 0) {
            returnInvalidIter++;
            if (returnInvalidIter > returnInvalidCount) {
                return WddmMock::virtualAlloc(inPtr, size, flags, type);
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

    int virtualFree(void *ptr, size_t size, unsigned long flags) override {
        if ((ptr == reinterpret_cast<void *>(0x1000)) || (ptr == reinterpret_cast<void *>(0x0))) {
            return 1;
        }

        return WddmMock::virtualFree(ptr, size, flags);
    }

    uint32_t returnGood = 0;
    uint32_t returnInvalidCount = 0;
    uint32_t returnInvalidIter = 0;
    uint32_t returnNullCount = 0;
    uint32_t returnNullIter = 0;
};

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsSuccessfulThenReturnReserveAddress) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);

    wddm->returnGood = 1;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsNullThenReturnNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);
    uintptr_t expectedReserve = 0;
    auto ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondSuccessfulThenReturnSecond) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);

    wddm->returnInvalidCount = 1;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenSecondIsInvalidThirdSuccessfulThenReturnThird) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);

    wddm->returnInvalidCount = 2;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondNullThenReturnSecondNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);

    wddm->returnInvalidCount = 2;
    wddm->returnNullCount = 1;
    uintptr_t expectedReserve = 0;

    auto ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}
