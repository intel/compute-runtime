/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "test.h"
#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/mocks/mock_wddm.h"

using namespace OCLRT;

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

using WddmReserveAddressTest = Test<GmmEnvironmentFixture>;

TEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsSuccessfulThenReturnReserveAddress) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    EXPECT_TRUE(ret);

    wddm->returnGood = 1;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsNullThenReturnNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    EXPECT_TRUE(ret);
    uintptr_t expectedReserve = 0;
    ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}

TEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondSuccessfulThenReturnSecond) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 1;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST_F(WddmReserveAddressTest, givenWddmWhenSecondIsInvalidThirdSuccessfulThenReturnThird) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 2;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

TEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondNullThenReturnSecondNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 2;
    wddm->returnNullCount = 1;
    uintptr_t expectedReserve = 0;

    ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}
