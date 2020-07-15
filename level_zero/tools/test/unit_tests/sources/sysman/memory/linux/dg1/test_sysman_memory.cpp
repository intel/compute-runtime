/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_memory.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;
namespace L0 {
namespace ult {

class SysmanMemoryFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    ze_device_handle_t hCoreDevice;
    Mock<MemoryNeoDrm> *pDrm = nullptr;

    OsMemory *pOsMemory = nullptr;
    PublicLinuxMemoryImp linuxMemoryImp;
    MemoryImp *pMemoryImp = nullptr;
    zet_sysman_mem_handle_t hSysmanMemory;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        hCoreDevice = device->toHandle();
        auto executionEnvironment = neoDevice->getExecutionEnvironment();
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
        pDrm = new NiceMock<Mock<MemoryNeoDrm>>(rootDeviceEnvironment);
        linuxMemoryImp.pDrm = pDrm;
        pOsMemory = static_cast<OsMemory *>(&linuxMemoryImp);

        ON_CALL(*pDrm, queryMemoryInfo())
            .WillByDefault(::testing::Invoke(pDrm, &Mock<MemoryNeoDrm>::queryMemoryInfoMockPositiveTest));
        pMemoryImp = new MemoryImp();
        pMemoryImp->pOsMemory = pOsMemory;
        pMemoryImp->init();
        sysmanImp->pMemoryHandleContext->handleList.push_back(pMemoryImp);
        hSysmanMemory = pMemoryImp->toHandle();
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        pMemoryImp->pOsMemory = nullptr;

        if (pDrm != nullptr) {
            delete pDrm;
            pDrm = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanMemoryFixture, GivenComponentCountZeroWhenCallingZetSysmanMemoryGetThenZeroCountIsReturnedAndVerifySysmanMemoryGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanMemoryGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GE(count, 1u);
    uint32_t testcount = count + 1;

    result = zetSysmanMemoryGet(hSysman, &testcount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);
}

TEST_F(SysmanMemoryFixture, GivenValidMemoryHandleWhenCallingzetSysmanMemoryGetPropertiesThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    zet_mem_properties_t properties;

    ze_result_t result = zetSysmanMemoryGetProperties(hSysmanMemory, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_MEM_TYPE_DDR, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(0u, properties.subdeviceId);
    EXPECT_EQ(0u, properties.physicalSize);
}

TEST_F(SysmanMemoryFixture, GivenValidMemoryHandleWhenCallingzetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceeds) {
    zet_mem_state_t state;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetSysmanMemoryGetState(hSysmanMemory, &state));
    EXPECT_EQ(ZET_MEM_HEALTH_OK, state.health);
    EXPECT_EQ((probedSizeRegionOne - unallocatedSizeRegionOne), state.allocatedSize);
    EXPECT_EQ(probedSizeRegionOne, state.maxSize);
}

TEST_F(SysmanMemoryFixture, GivenValidMemoryHandleWhenCallingzetSysmanMemoryGetStateAndIfQueryMemoryInfoFailsThenErrorIsReturned) {
    zet_mem_state_t state;

    ON_CALL(*pDrm, queryMemoryInfo())
        .WillByDefault(::testing::Invoke(pDrm, &Mock<MemoryNeoDrm>::queryMemoryInfoMockReturnFalse));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanMemoryGetState(hSysmanMemory, &state));
}

TEST_F(SysmanMemoryFixture, GivenValidMemoryHandleWhenCallingzetSysmanMemoryGetStateAndIfQueryMemoryDidntProvideDeviceMemoryThenErrorIsReturned) {
    zet_mem_state_t state;

    ON_CALL(*pDrm, queryMemoryInfo())
        .WillByDefault(::testing::Invoke(pDrm, &Mock<MemoryNeoDrm>::queryMemoryInfoMockWithoutDevice));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanMemoryGetState(hSysmanMemory, &state));
}
} // namespace ult
} // namespace L0
