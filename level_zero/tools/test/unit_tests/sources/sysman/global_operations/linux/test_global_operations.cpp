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
#include "mock_global_operations.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;

namespace L0 {
namespace ult {

constexpr int64_t memSize1 = 2048;
constexpr int64_t memSize2 = 1024;
// In mock function getValUnsignedLong, we have set the engines used as 0, 3 and 1.
// Hence, expecting 28 as engine field because 28 in binary would be 00011100
// This indicates bit number 2, 3 and 4 are set, thus this indicates, this process
// used ZET_ENGINE_TYPE_3D, ZET_ENGINE_TYPE_MEDIA and ZET_ENGINE_TYPE_DMA
// Their corresponding mapping with i915 engine numbers are 0, 3 and 1 respectively.
constexpr int64_t engines1 = 28u;
// 4 in binary 0100, as 2nd bit is set, hence it indicates, process used ZET_ENGINE_TYPE_3D
// Corresponding i915 mapped value in mocked getValUnsignedLong() is 0.
constexpr int64_t engines2 = 4u;
constexpr uint32_t totalProcessStates = 2u; // Two process States for two pids
const std::string expectedModelName("0x3ea5");
class SysmanGlobalOperationsFixture : public DeviceFixture, public ::testing::Test {
  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    OsGlobalOperations *pOsGlobalOperations = nullptr;
    Mock<GlobalOperationsSysfsAccess> *pSysfsAccess = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp globalOperationsImp;
    PublicLinuxGlobalOperationsImp linuxGlobalOperationsImp;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<GlobalOperationsSysfsAccess>>;
        linuxGlobalOperationsImp.pSysfsAccess = pSysfsAccess;
        pOsGlobalOperations = static_cast<OsGlobalOperations *>(&linuxGlobalOperationsImp);

        ON_CALL(*pSysfsAccess, read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<GlobalOperationsSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess, read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<GlobalOperationsSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess, scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<GlobalOperationsSysfsAccess>::getScannedDirEntries));

        pGlobalOperationsPrev = sysmanImp->pGlobalOperations;
        sysmanImp->pGlobalOperations = static_cast<GlobalOperations *>(&globalOperationsImp);
        globalOperationsImp.pOsGlobalOperations = pOsGlobalOperations;
        globalOperationsImp.hCoreDevice = sysmanImp->hCoreDevice;
        globalOperationsImp.init();
        hSysman = sysmanImp->toHandle();
    }

    void TearDown() override {
        // restore state
        sysmanImp->pGlobalOperations = pGlobalOperationsPrev;
        globalOperationsImp.pOsGlobalOperations = nullptr;

        // cleanup
        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenValidSysmanHandleWhenCallingzetGlobalOperationsGetPropertiesThenVerifyzetGlobalOperationsGetPropertiesCallSucceeds) {
    zet_sysman_properties_t properties;
    ze_result_t result = zetSysmanDeviceGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.numSubdevices, 0u);
    EXPECT_TRUE(0 == std::memcmp(properties.boardNumber, unknown.c_str(), unknown.size()));
    EXPECT_TRUE(0 == std::memcmp(properties.brandName, vendorIntel.c_str(), vendorIntel.size()));
    EXPECT_TRUE(0 == std::memcmp(properties.driverVersion, unknown.c_str(), unknown.size()));
    EXPECT_TRUE(0 == std::memcmp(properties.modelName, expectedModelName.c_str(), expectedModelName.size()));
    EXPECT_TRUE(0 == std::memcmp(properties.serialNumber, unknown.c_str(), unknown.size()));
    EXPECT_TRUE(0 == std::memcmp(properties.vendorName, vendorIntel.c_str(), vendorIntel.size()));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidSysmanHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturned) {
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanProcessesGetState(hSysman, &count, nullptr));
    EXPECT_EQ(count, totalProcessStates);
    std::vector<zet_process_state_t> processes(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanProcessesGetState(hSysman, &count, processes.data()));
    EXPECT_EQ(processes[0].processId, pid1);
    EXPECT_EQ(processes[0].engines, engines1);
    EXPECT_EQ(processes[0].memSize, memSize1);
    EXPECT_EQ(processes[1].processId, pid2);
    EXPECT_EQ(processes[1].engines, engines2);
    EXPECT_EQ(processes[1].memSize, memSize2);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidSysmanHandleWhileRetrievingInformationAboutHostProcessesUsingFaultyClientFileThenFailureIsReturned) {
    uint32_t count = 0;
    ON_CALL(*pSysfsAccess, scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<GlobalOperationsSysfsAccess>::getScannedDir4Entries));
    ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetSysmanProcessesGetState(hSysman, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidSysmanHandleWhileReadingExistingMemoryFileThenCorrectValueIsReturned) {
    uint64_t memSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysfsAccess->read("clients/6/total_device_memory_buffer_objects/created_bytes", memSize));
    EXPECT_EQ(memSize2, static_cast<int64_t>(memSize));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidSysmanHandleWhileReadingNonExistingFileThenErrorIsReturned) {
    std::vector<std::string> engineEntries;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysfsAccess->scanDirEntries("clients/7/busy", engineEntries));
}

} // namespace ult
} // namespace L0
