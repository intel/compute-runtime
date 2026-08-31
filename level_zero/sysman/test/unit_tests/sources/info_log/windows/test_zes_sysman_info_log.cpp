/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance_imp.h"
#include "level_zero/sysman/source/api/info_log/windows/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_driver.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t expectedInfoLogHandleCount = 1u;
static constexpr uint32_t mockReadBufferSize = 64u;
static constexpr uint64_t mockReadTimeout = 10u;
static constexpr uint64_t mockPeekTimeout = 20u;
static constexpr const char *mockInstanceName = "my_instance";
static constexpr const char *mockOtherInstanceName = "my_other_instance";

struct MockOsSysmanDriver : public OsSysmanDriver {
    MockOsSysmanDriver() {
        context.supportedFormats = {ZES_INTEL_INFO_LOG_FORMAT_CPER};
    }
    ze_result_t eventsListen(uint64_t, uint32_t, zes_device_handle_t *, uint32_t *, zes_event_type_flags_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t driverEventsListen(uint64_t, uint32_t, zes_device_handle_t *, uint32_t *, zes_event_type_flags_t *, zes_event_type_flags_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t driverEventRegister(zes_event_type_flags_t) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) override {
        return context.infoLogGet(pCount, phInfoLogs);
    }
    ze_result_t rescanDevices(SysmanDriverHandleImp *, uint32_t *, zes_device_handle_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    InfoLogHandleContext context;
};

struct MockInfoLog : public InfoLog {
    ze_result_t infoLogGetProperties(zes_intel_info_log_properties_exp_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t infoLogCreateInstance(const char *, zes_intel_info_log_instance_exp_desc_t *,
                                      zes_intel_info_log_instance_handle_t *) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t destroyInstance(InfoLogInstance *pInstance) override {
        destroyInstanceCallCount++;
        pLastDestroyedInstance = pInstance;
        return ZE_RESULT_SUCCESS;
    }
    void destroyAllInstances() override {
        destroyAllInstancesCallCount++;
    }
    uint32_t destroyAllInstancesCallCount = 0;
    uint32_t destroyInstanceCallCount = 0;
    InfoLogInstance *pLastDestroyedInstance = nullptr;
};

struct MockInfoLogInstance : public InfoLogInstance {
    ze_result_t readWithMetadata(uint64_t timeout, uint32_t *, uint8_t *, uint32_t *,
                                 zes_intel_info_log_metadata_exp *,
                                 zes_intel_info_log_read_status_exp_t *) override {
        readCallCount++;
        lastTimeout = timeout;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t peekWithMetadata(uint64_t timeout, uint32_t *, uint8_t *, uint32_t *,
                                 zes_intel_info_log_metadata_exp *,
                                 zes_intel_info_log_read_status_exp_t *) override {
        peekCallCount++;
        lastTimeout = timeout;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t destroy() override {
        destroyCallCount++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t teardown() override {
        teardownCallCount++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t readCallCount = 0;
    uint32_t peekCallCount = 0;
    uint32_t destroyCallCount = 0;
    uint32_t teardownCallCount = 0;
    uint64_t lastTimeout = 0;
};

struct MockOsInfoLogInstance : public OsInfoLogInstance {
    MockOsInfoLogInstance(uint32_t *pTeardownCallCount) : pTeardownCallCount(pTeardownCallCount) {}
    ze_result_t readWithMetadata(uint64_t timeout, uint32_t *, uint8_t *, uint32_t *,
                                 zes_intel_info_log_metadata_exp *,
                                 zes_intel_info_log_read_status_exp_t *) override {
        readCallCount++;
        lastTimeout = timeout;
        return readResult;
    }
    ze_result_t peekWithMetadata(uint64_t timeout, uint32_t *, uint8_t *, uint32_t *,
                                 zes_intel_info_log_metadata_exp *,
                                 zes_intel_info_log_read_status_exp_t *) override {
        peekCallCount++;
        lastTimeout = timeout;
        return peekResult;
    }
    ze_result_t teardown() override {
        (*pTeardownCallCount)++;
        return teardownResult;
    }
    int getTracePipeFd() const override { return -1; }
    ze_result_t readResult = ZE_RESULT_SUCCESS;
    ze_result_t peekResult = ZE_RESULT_SUCCESS;
    ze_result_t teardownResult = ZE_RESULT_SUCCESS;
    uint32_t readCallCount = 0;
    uint32_t peekCallCount = 0;
    uint64_t lastTimeout = 0;
    uint32_t *pTeardownCallCount;
};

struct MockOsInfoLog : public OsInfoLog {
    ze_result_t getProperties(zes_intel_info_log_properties_exp_t *pProperties) override {
        if (getPropertiesResult != ZE_RESULT_SUCCESS) {
            return getPropertiesResult;
        }
        pProperties->infoLogType = ZES_INTEL_INFO_LOG_TYPE_EXP_DEVICE;
        pProperties->infoLogFormat = ZES_INTEL_INFO_LOG_FORMAT_CPER;
        pProperties->isNamedInstancedCollectionSupported = isNamedInstancedCollectionSupported;
        pProperties->isPeekSupported = true;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t createInstance(const char *, zes_intel_info_log_instance_exp_desc_t *,
                               std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) override {
        if (createInstanceResult == ZE_RESULT_SUCCESS) {
            pOsInfoLogInstance = std::make_unique<MockOsInfoLogInstance>(&teardownCallCount);
        }
        return createInstanceResult;
    }
    ze_result_t getPropertiesResult = ZE_RESULT_SUCCESS;
    ze_result_t createInstanceResult = ZE_RESULT_SUCCESS;
    bool isNamedInstancedCollectionSupported = true;
    uint32_t teardownCallCount = 0;
};

class SysmanInfoLogFixture : public SysmanDriverHandleTest {
  protected:
    void TearDown() override {
        if (mockOsSysmanDriver != nullptr) {
            driverHandle->pOsSysmanDriver = pRealOsSysmanDriver;
        }
        SysmanDriverHandleTest::TearDown();
    }

    void installMockOsSysmanDriver() {
        mockOsSysmanDriver = std::make_unique<MockOsSysmanDriver>();
        pRealOsSysmanDriver = driverHandle->pOsSysmanDriver;
        driverHandle->pOsSysmanDriver = mockOsSysmanDriver.get();
    }

    std::vector<zes_intel_info_log_handle_t> getInfoLogHandles(uint32_t count) {
        std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
        return handles;
    }

    std::unique_ptr<InfoLogImp> createInfoLogWithMockOsBackend(MockOsInfoLog **ppMockOsInfoLog,
                                                               bool namedCollectionSupported = true) {
        auto pInfoLogImp = std::make_unique<InfoLogImp>(ZES_INTEL_INFO_LOG_FORMAT_CPER);
        auto mockOsInfoLog = std::make_unique<MockOsInfoLog>();
        mockOsInfoLog->isNamedInstancedCollectionSupported = namedCollectionSupported;
        *ppMockOsInfoLog = mockOsInfoLog.get();
        pInfoLogImp->pOsInfoLog = std::move(mockOsInfoLog);
        pInfoLogImp->init();
        return pInfoLogImp;
    }

    static zes_intel_info_log_instance_exp_desc_t makeInstanceDesc() {
        zes_intel_info_log_instance_exp_desc_t desc = {};
        desc.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_INSTANCE_EXP_DESC;
        return desc;
    }

    std::unique_ptr<MockOsSysmanDriver> mockOsSysmanDriver;
    OsSysmanDriver *pRealOsSysmanDriver = nullptr;
};

TEST_F(SysmanInfoLogFixture, GivenInfoLogImpWhenDestroyingAnInstanceItDoesNotOwnThenInvalidNullHandleIsReturned) {
    auto pInfoLogImp = std::make_unique<InfoLogImp>(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    MockInfoLogInstance foreignInstance;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, pInfoLogImp->destroyInstance(&foreignInstance));
    EXPECT_EQ(0u, foreignInstance.teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogContextAlreadyExistsWhenEnumeratingInfoLogsAgainThenSameContextIsReused) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanInfoLogFixture, GivenMultipleInfoLogHandlesWhenDestroyingAllInstancesTwiceThenEveryHandleIsNotifiedEachTime) {
    InfoLogHandleContext context;

    auto firstInfoLog = std::make_unique<MockInfoLog>();
    auto secondInfoLog = std::make_unique<MockInfoLog>();
    auto *pFirstInfoLog = firstInfoLog.get();
    auto *pSecondInfoLog = secondInfoLog.get();
    context.handleList.push_back(std::move(firstInfoLog));
    context.handleList.push_back(std::move(secondInfoLog));

    context.destroyAllInstances();
    context.destroyAllInstances();

    EXPECT_EQ(2u, pFirstInfoLog->destroyAllInstancesCallCount);
    EXPECT_EQ(2u, pSecondInfoLog->destroyAllInstancesCallCount);
    EXPECT_EQ(2u, static_cast<uint32_t>(context.handleList.size()));
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogHandlesWhenReleasingInfoLogHandlesThenHandleListIsEmptied) {
    InfoLogHandleContext context;
    context.supportedFormats = {ZES_INTEL_INFO_LOG_FORMAT_CPER};

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context.infoLogGet(&count, nullptr));
    ASSERT_EQ(expectedInfoLogHandleCount, count);
    ASSERT_EQ(expectedInfoLogHandleCount, static_cast<uint32_t>(context.handleList.size()));

    context.releaseInfoLogHandles();
    EXPECT_TRUE(context.handleList.empty());
}

TEST_F(SysmanInfoLogFixture, GivenCountZeroOrGreaterThanAvailableWhenEnumeratingInfoLogsThenCountIsUpdatedAndOneHandleIsReturned) {
    installMockOsSysmanDriver();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(expectedInfoLogHandleCount, count);

    count = 5;
    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    EXPECT_EQ(expectedInfoLogHandleCount, count);
    EXPECT_NE(nullptr, handles[0]);
    EXPECT_EQ(nullptr, handles[1]);
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    VariableBackup<decltype(driverHandle->pOsSysmanDriver)> osSysmanDriverBackup(&driverHandle->pOsSysmanDriver, nullptr);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingGetPropertiesExpThenUnsupportedFeatureIsReturned) {
    installMockOsSysmanDriver();

    auto handles = getInfoLogHandles(expectedInfoLogHandleCount);
    ASSERT_NE(nullptr, handles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogGetPropertiesExp(handles[0], &properties));

    EXPECT_FALSE(properties.isNamedInstancedCollectionSupported);
    EXPECT_FALSE(properties.isPeekSupported);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingCreateInstanceExpThenUnsupportedFeatureIsReturned) {
    installMockOsSysmanDriver();

    auto handles = getInfoLogHandles(expectedInfoLogHandleCount);
    ASSERT_NE(nullptr, handles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogCreateInstanceExp(handles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingCreateNamedInstanceExpThenUnsupportedFeatureIsReturned) {
    installMockOsSysmanDriver();

    auto handles = getInfoLogHandles(expectedInfoLogHandleCount);
    ASSERT_NE(nullptr, handles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogCreateInstanceExp(handles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
}

TEST_F(SysmanInfoLogFixture, GivenWddmBackendWhenAskedForPropertiesAndForAnInstanceThenUnsupportedFeatureIsReturnedAndNoOsInstanceIsProduced) {
    WddmInfoLogImp wddmInfoLog;

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, wddmInfoLog.getProperties(&properties));

    auto desc = makeInstanceDesc();
    std::unique_ptr<OsInfoLogInstance> pOsInstance;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, wddmInfoLog.createInstance(nullptr, &desc, pOsInstance));
    EXPECT_EQ(nullptr, pOsInstance.get());

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, wddmInfoLog.createInstance(mockInstanceName, &desc, pOsInstance));
    EXPECT_EQ(nullptr, pOsInstance.get());

    EXPECT_TRUE(OsInfoLog::getSupportedInfoLogFormats().empty());
}

TEST_F(SysmanInfoLogFixture, GivenInstanceHandleWhenCallingInstanceReadAndPeekWithMetadataExpThenCallsAreForwardedToTheInstance) {
    MockInfoLogInstance instance;

    uint32_t size = mockReadBufferSize;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    zes_intel_info_log_metadata_exp descriptor = {};
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstanceReadWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, &readStatus));
    EXPECT_EQ(1u, instance.readCallCount);
    EXPECT_EQ(mockReadTimeout, instance.lastTimeout);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstancePeekWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, &readStatus));
    EXPECT_EQ(1u, instance.peekCallCount);
    EXPECT_EQ(mockReadTimeout, instance.lastTimeout);
}

TEST_F(SysmanInfoLogFixture, GivenInstanceHandleWhenCallingInstanceDeleteExpThenCallIsForwardedToTheInstance) {
    MockInfoLogInstance instance;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(instance.toHandle()));
    EXPECT_EQ(1u, instance.destroyCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenSysmanInitFromCoreWhenCallingInfoLogEntryPointsThenUnsupportedFeatureIsReturnedWithoutReachingTheHandles) {
    installMockOsSysmanDriver();

    auto handles = getInfoLogHandles(expectedInfoLogHandleCount);
    ASSERT_NE(nullptr, handles[0]);

    MockInfoLogInstance instance;
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, true);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogGetPropertiesExp(handles[0], &properties));

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogCreateInstanceExp(handles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    uint32_t size = mockReadBufferSize;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    zes_intel_info_log_metadata_exp descriptor = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstanceReadWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstancePeekWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogInstanceDeleteExp(instance.toHandle()));

    EXPECT_EQ(0u, instance.readCallCount);
    EXPECT_EQ(0u, instance.peekCallCount);
    EXPECT_EQ(0u, instance.destroyCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenNeitherInitFlagSetWhenCallingInfoLogEntryPointsThenUninitializedIsReturnedWithoutReachingTheHandles) {
    installMockOsSysmanDriver();

    auto handles = getInfoLogHandles(expectedInfoLogHandleCount);
    ASSERT_NE(nullptr, handles[0]);

    MockInfoLogInstance instance;
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, false);
    VariableBackup<bool> sysmanOnlyInitBackup(&L0::Sysman::sysmanOnlyInit, false);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogGetPropertiesExp(handles[0], &properties));

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogCreateInstanceExp(handles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    uint32_t size = mockReadBufferSize;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    zes_intel_info_log_metadata_exp descriptor = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED,
              zesIntelInfoLogInstanceReadWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED,
              zesIntelInfoLogInstancePeekWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogInstanceDeleteExp(instance.toHandle()));

    EXPECT_EQ(0u, instance.readCallCount);
    EXPECT_EQ(0u, instance.peekCallCount);
    EXPECT_EQ(0u, instance.destroyCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPropertyCaptureSucceededWhenGettingPropertiesThenCapturedValuesAreReturned) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogGetProperties(&properties));

    EXPECT_EQ(ZES_INTEL_INFO_LOG_TYPE_EXP_DEVICE, properties.infoLogType);
    EXPECT_EQ(ZES_INTEL_INFO_LOG_FORMAT_CPER, properties.infoLogFormat);
    EXPECT_TRUE(properties.isNamedInstancedCollectionSupported);
    EXPECT_TRUE(properties.isPeekSupported);

    EXPECT_EQ(ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP, properties.stype);
}

TEST_F(SysmanInfoLogFixture, GivenNamedCollectionUnsupportedWhenCreatingNamedInstanceThenUnsupportedFeatureIsReturned) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog, false);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(nullptr, &desc, &hInstance));
    EXPECT_NE(nullptr, hInstance);
}

TEST_F(SysmanInfoLogFixture, GivenNamedCollectionSupportedWhenCreatingTheSameNamedInstanceTwiceThenTheSecondRequestReportsTheNameInUse) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hFirstInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hFirstInstance));
    EXPECT_NE(nullptr, hFirstInstance);

    zes_intel_info_log_instance_handle_t hSecondInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE,
              pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hSecondInstance));
    EXPECT_EQ(nullptr, hSecondInstance);

    zes_intel_info_log_instance_handle_t hOtherInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockOtherInstanceName, &desc, &hOtherInstance));
    EXPECT_NE(nullptr, hOtherInstance);
    EXPECT_NE(hFirstInstance, hOtherInstance);
}

TEST_F(SysmanInfoLogFixture, GivenOsBackendFailsToCreateTheInstanceWhenCreatingAnInstanceThenTheBackendErrorIsReturnedAndNothingIsRegistered) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog);
    pMockOsInfoLog->createInstanceResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pInfoLogImp->infoLogCreateInstance(nullptr, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    pMockOsInfoLog->createInstanceResult = ZE_RESULT_SUCCESS;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hInstance));
    EXPECT_NE(nullptr, hInstance);

    pInfoLogImp->destroyAllInstances();
    EXPECT_EQ(1u, pMockOsInfoLog->teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenOwnedNamedAndUnnamedInstancesWhenDestroyingThemThenTheyAreTornDownAndOnlyTheNameIsReleased) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hNamedInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hNamedInstance));

    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->destroyInstance(InfoLogInstance::fromHandle(hNamedInstance)));
    EXPECT_EQ(1u, pMockOsInfoLog->teardownCallCount);

    zes_intel_info_log_instance_handle_t hReusedInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hReusedInstance));

    zes_intel_info_log_instance_handle_t hUnnamedInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(nullptr, &desc, &hUnnamedInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->destroyInstance(InfoLogInstance::fromHandle(hUnnamedInstance)));
    EXPECT_EQ(2u, pMockOsInfoLog->teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenOwnedInstancesWhenDestroyingAllInstancesThenEveryInstanceIsTornDownAndNamesAreReleased) {
    MockOsInfoLog *pMockOsInfoLog = nullptr;
    auto pInfoLogImp = createInfoLogWithMockOsBackend(&pMockOsInfoLog);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hNamedInstance = nullptr;
    zes_intel_info_log_instance_handle_t hUnnamedInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hNamedInstance));
    ASSERT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(nullptr, &desc, &hUnnamedInstance));

    pInfoLogImp->destroyAllInstances();
    EXPECT_EQ(2u, pMockOsInfoLog->teardownCallCount);

    zes_intel_info_log_instance_handle_t hReusedInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->infoLogCreateInstance(mockInstanceName, &desc, &hReusedInstance));
    EXPECT_NE(nullptr, hReusedInstance);
}

TEST_F(SysmanInfoLogFixture, GivenRealInfoLogInstanceWhenReadingAndPeekingThenBothCallsAreForwardedToTheOsInstance) {
    uint32_t teardownCallCount = 0;
    auto osInstance = std::make_unique<MockOsInfoLogInstance>(&teardownCallCount);
    osInstance->readResult = ZE_RESULT_SUCCESS;
    osInstance->peekResult = ZE_RESULT_NOT_READY;
    auto *pOsInstance = osInstance.get();

    InfoLogInstanceImp instance(nullptr, nullptr, std::move(osInstance));
    EXPECT_FALSE(instance.isNamed());
    EXPECT_TRUE(instance.getInstanceName().empty());

    uint32_t size = mockReadBufferSize;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    zes_intel_info_log_metadata_exp descriptor = {};
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;

    EXPECT_EQ(ZE_RESULT_SUCCESS,
              zesIntelInfoLogInstanceReadWithMetadataExp(instance.toHandle(), mockReadTimeout, &size, buffer.data(), &recordCount, &descriptor, &readStatus));
    EXPECT_EQ(1u, pOsInstance->readCallCount);
    EXPECT_EQ(0u, pOsInstance->peekCallCount);
    EXPECT_EQ(mockReadTimeout, pOsInstance->lastTimeout);

    EXPECT_EQ(ZE_RESULT_NOT_READY,
              zesIntelInfoLogInstancePeekWithMetadataExp(instance.toHandle(), mockPeekTimeout, &size, buffer.data(), &recordCount, &descriptor, &readStatus));
    EXPECT_EQ(1u, pOsInstance->readCallCount);
    EXPECT_EQ(1u, pOsInstance->peekCallCount);
    EXPECT_EQ(mockPeekTimeout, pOsInstance->lastTimeout);
}

TEST_F(SysmanInfoLogFixture, GivenRealInfoLogInstanceWhenDeletingItThenTheOwningInfoLogIsAskedToDestroyIt) {
    MockInfoLog infoLog;
    uint32_t teardownCallCount = 0;
    auto osInstance = std::make_unique<MockOsInfoLogInstance>(&teardownCallCount);

    InfoLogInstanceImp instance(&infoLog, mockInstanceName, std::move(osInstance));
    EXPECT_TRUE(instance.isNamed());
    EXPECT_EQ(mockInstanceName, instance.getInstanceName());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(instance.toHandle()));
    EXPECT_EQ(1u, infoLog.destroyInstanceCallCount);
    EXPECT_EQ(&instance, infoLog.pLastDestroyedInstance);

    EXPECT_EQ(0u, teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenRealInfoLogInstanceAlreadyTornDownWhenTearingItDownAgainThenTheOsInstanceIsToldOnlyOnce) {
    uint32_t teardownCallCount = 0;
    auto osInstance = std::make_unique<MockOsInfoLogInstance>(&teardownCallCount);

    InfoLogInstanceImp instance(nullptr, nullptr, std::move(osInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.teardown());
    EXPECT_EQ(1u, teardownCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.teardown());
    EXPECT_EQ(1u, teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenOsInstanceTeardownFailsWhenTearingDownTheRealInfoLogInstanceThenTheFailureIsReported) {
    uint32_t teardownCallCount = 0;
    auto osInstance = std::make_unique<MockOsInfoLogInstance>(&teardownCallCount);
    osInstance->teardownResult = ZE_RESULT_ERROR_UNKNOWN;

    InfoLogInstanceImp instance(nullptr, nullptr, std::move(osInstance));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, instance.teardown());
    EXPECT_EQ(1u, teardownCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.teardown());
    EXPECT_EQ(1u, teardownCallCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
