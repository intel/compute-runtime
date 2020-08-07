/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_fs_ras.h"

using ::testing::_;
using ::testing::Matcher;
using ::testing::NiceMock;

namespace L0 {
namespace ult {

struct SysmanRasFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<RasFsAccess>> ptestFsAccess;
    FsAccess *pOriginalFsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        ptestFsAccess = std::make_unique<NiceMock<Mock<RasFsAccess>>>();
        pOriginalFsAccess = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = ptestFsAccess.get();
        ON_CALL(*ptestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(ptestFsAccess.get(), &Mock<RasFsAccess>::getVal));
        ON_CALL(*ptestFsAccess.get(), fileExists(_))
            .WillByDefault(::testing::Invoke(ptestFsAccess.get(), &Mock<RasFsAccess>::checkFileExists));
        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        pSysmanDeviceImp->pRasHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pOriginalFsAccess;
    }
};

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasZeroHandlesInReturn) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, 0u);

    count = 0;
    std::vector<zes_ras_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

} // namespace ult
} // namespace L0
