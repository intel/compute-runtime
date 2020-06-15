/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_fs_ras.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;

namespace L0 {
namespace ult {

struct SysmanRasFixture : public DeviceFixture, public ::testing::Test {
  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_ras_handle_t hSysmanRas;
    Mock<RasFsAccess> *pFsAccess = nullptr;
    OsRas *pOsRas = nullptr;
    PublicLinuxRasImp linuxRasImp;
    RasImp *pRasImp = nullptr;
    unsigned long numRasHandlesSetup = 0;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pFsAccess = new NiceMock<Mock<RasFsAccess>>;
        linuxRasImp.pFsAccess = pFsAccess;
        pOsRas = static_cast<OsRas *>(&linuxRasImp);
        pRasImp = new RasImp();
        pRasImp->pOsRas = pOsRas;
        ON_CALL(*pFsAccess, read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pFsAccess, &Mock<RasFsAccess>::getVal));
        ON_CALL(*pFsAccess, fileExists(_))
            .WillByDefault(::testing::Invoke(pFsAccess, &Mock<RasFsAccess>::checkFileExists));
        pRasImp->rasErrorType = ZET_RAS_ERROR_TYPE_UNCORRECTABLE;
        pRasImp->init();
        pOsRas->setRasErrorType(ZET_RAS_ERROR_TYPE_UNCORRECTABLE);
        sysmanImp->pRasHandleContext->handleList.push_back(pRasImp);
        numRasHandlesSetup = sysmanImp->pRasHandleContext->handleList.size();
        hSysman = sysmanImp->toHandle();
        hSysmanRas = pRasImp->toHandle();
    }
    void TearDown() override {
        pRasImp->pOsRas = nullptr;
        if (pFsAccess != nullptr) {
            delete pFsAccess;
            pFsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingSysmanRasGetThenExpectCallToSucceed) {
    zet_sysman_ras_handle_t rasHandle;
    uint32_t count = 0;
    ze_result_t result = zetSysmanRasGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numRasHandlesSetup, count);

    uint32_t testcount = count + 1;

    result = zetSysmanRasGet(hSysman, &testcount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);

    result = zetSysmanRasGet(hSysman, &count, &rasHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numRasHandlesSetup, count);
}

TEST_F(SysmanRasFixture, GivenCorrectCounterNameWhenCheckingRasCounterSupportThenExpectTrue) {
    EXPECT_TRUE(pOsRas->isRasSupported());
}
TEST_F(SysmanRasFixture, GivenCorrectableTypeWhenCheckingRasErrorSupportThenExpectFalse) {
    pOsRas->setRasErrorType(ZET_RAS_ERROR_TYPE_CORRECTABLE);
    EXPECT_FALSE(pOsRas->isRasSupported());
}

TEST_F(SysmanRasFixture, GivenIncorrectDirectoryNameWhenCheckingRasCounterSupportThenExpectFalse) {
    pFsAccess->setRasDirName("wrong/ras/Dir");
    EXPECT_FALSE(pOsRas->isRasSupported());
}

TEST_F(SysmanRasFixture, GivenIncorrectCounterNameWhenCheckingRasCounterValuesThenExpectFailure) {
    pFsAccess->setResetCounterFileName("wrongrasEntry");
    zet_ras_details_t pDetails = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
    EXPECT_NE(ZE_RESULT_SUCCESS, pOsRas->getCounterValues(&pDetails));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingSysmanGetPropertiesThenExpectCallToSucceed) {
    zet_ras_properties_t properties;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetSysmanRasGetProperties(hSysmanRas, &properties));
    EXPECT_EQ(ZET_RAS_ERROR_TYPE_UNCORRECTABLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingSysmanRasGetStateThenExpectCallToSucceed) {
    zet_ras_details_t details, detailsBefore;
    uint64_t totalErrors;
    ze_result_t result;
    memset(&details.numResets, std::numeric_limits<int>::max(), sizeof(details.numResets));
    memset(&details.numProgrammingErrors, std::numeric_limits<int>::max(), sizeof(details.numProgrammingErrors));
    memset(&details.numDriverErrors, std::numeric_limits<int>::max(), sizeof(details.numDriverErrors));
    memset(&details.numComputeErrors, std::numeric_limits<int>::max(), sizeof(details.numComputeErrors));
    memset(&details.numNonComputeErrors, std::numeric_limits<int>::max(), sizeof(details.numNonComputeErrors));
    memset(&details.numCacheErrors, std::numeric_limits<int>::max(), sizeof(details.numCacheErrors));
    memset(&details.numDisplayErrors, std::numeric_limits<int>::max(), sizeof(details.numDisplayErrors));
    detailsBefore = details;
    result = zetSysmanRasGetState(hSysmanRas, false, &totalErrors, &details);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(details.numResets, detailsBefore.numResets);
    EXPECT_NE(details.numProgrammingErrors, detailsBefore.numProgrammingErrors);
    EXPECT_NE(details.numDriverErrors, detailsBefore.numDriverErrors);
    EXPECT_NE(details.numComputeErrors, detailsBefore.numComputeErrors);
    EXPECT_NE(details.numNonComputeErrors, detailsBefore.numNonComputeErrors);
    EXPECT_NE(details.numCacheErrors, detailsBefore.numCacheErrors);
    EXPECT_NE(details.numDisplayErrors, detailsBefore.numDisplayErrors);

    EXPECT_EQ(totalErrors, mockResetCount);

    EXPECT_EQ(details.numResets, mockResetCount);
    EXPECT_NE(details.numProgrammingErrors, mockProgrammingErrorCount);
    EXPECT_NE(details.numDriverErrors, mockDriverErrorCount);
    EXPECT_NE(details.numComputeErrors, mockComputeErrorCount);
    EXPECT_NE(details.numNonComputeErrors, mockNonComputeErrorCount);
    EXPECT_NE(details.numCacheErrors, mockCacheErrorCount);
    EXPECT_NE(details.numDisplayErrors, mockDisplayErrorCount);
}
TEST_F(SysmanRasFixture, GivenNullptrForRasDetailsWhenCallingSysmanRasGetStateThenExpectCallToReturnTotalErrorsOnly) {
    uint64_t totalErrors;
    ze_result_t result;

    result = zetSysmanRasGetState(hSysmanRas, false, &totalErrors, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(totalErrors, mockResetCount);
}
} // namespace ult
} // namespace L0
