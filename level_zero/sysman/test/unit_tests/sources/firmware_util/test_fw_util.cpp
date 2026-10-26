/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"

#include <algorithm>
#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

static uint32_t mockFwUtilDeviceCloseCallCount = 0;
bool MockFwUtilOsLibrary::mockLoad = true;
constexpr uint32_t fwDataMajorVersion = 10;
constexpr uint16_t fwDataOemVersion = 10;
constexpr uint16_t fwDataVcnVersion = 1;

static int mockIgscDeviceFwVersion(struct igsc_device_handle *handle,
                                   struct igsc_fw_version *version) {
    return IGSC_SUCCESS;
}

static int mockIgscDeviceFwDataVersion(struct igsc_device_handle *handle,
                                       struct igsc_fwdata_version *version) {
    version->major_version = fwDataMajorVersion;
    version->oem_manuf_data_version = fwDataOemVersion;
    version->major_vcn = fwDataVcnVersion;
    return IGSC_SUCCESS;
}

static int mockIgscDeviceOpromVersion(struct igsc_device_handle *handle,
                                      uint32_t opromType,
                                      struct igsc_oprom_version *version) {
    return IGSC_SUCCESS;
}

static int mockIgscDevicePscVersion(struct igsc_device_handle *handle,
                                    struct igsc_psc_version *version) {
    return IGSC_SUCCESS;
}

static std::vector<std::string> mockLoadedLibraryNames;
static std::vector<std::string> mockValidLibraryNames;

static int mockIgscDeviceInitByDevice(struct igsc_device_handle *handle, const char *devicePath) {
    return IGSC_SUCCESS;
}

static int mockIgscDeviceIteratorCreate(struct igsc_device_iterator **iter) {
    return IGSC_SUCCESS;
}

static int mockIgscDeviceIteratorNext(struct igsc_device_iterator *iter, struct igsc_device_info *info) {
    memset(info, 0, sizeof(*info));
    strncpy(info->name, "/dev/mei0", IGSC_INFO_NAME_SIZE - 1);
    return IGSC_SUCCESS;
}

static void mockIgscDeviceIteratorDestroy(struct igsc_device_iterator *iter) {
}

static int mockIgscDeviceClose(struct igsc_device_handle *handle) {
    return IGSC_SUCCESS;
}

static int mockIgscDummyFunction() {
    return IGSC_SUCCESS;
}

static OsLibrary *mockLoadValidFwUtilLibraryByName(const OsLibraryCreateProperties &properties) {
    mockLoadedLibraryNames.push_back(properties.libraryName);
    if (std::find(mockValidLibraryNames.begin(), mockValidLibraryNames.end(), properties.libraryName) == mockValidLibraryNames.end()) {
        return nullptr;
    }
    auto osLibHandle = new MockFwUtilOsLibrary();
    auto dummyFunction = reinterpret_cast<void *>(&mockIgscDummyFunction);
    osLibHandle->funcMap["igsc_device_init_by_device_info"] = reinterpret_cast<void *>(&mockIgscDeviceInitByDevice);
    osLibHandle->funcMap["igsc_device_get_device_info"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_fw_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_fwdata_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_iterator_create"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorCreate);
    osLibHandle->funcMap["igsc_device_iterator_next"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorNext);
    osLibHandle->funcMap["igsc_device_iterator_destroy"] = reinterpret_cast<void *>(&mockIgscDeviceIteratorDestroy);
    osLibHandle->funcMap["igsc_device_fw_update"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_fwdata_update"] = dummyFunction;
    osLibHandle->funcMap["igsc_image_oprom_init"] = dummyFunction;
    osLibHandle->funcMap["igsc_image_oprom_type"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_oprom_update"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_oprom_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_psc_version"] = dummyFunction;
    osLibHandle->funcMap["igsc_device_close"] = reinterpret_cast<void *>(&mockIgscDeviceClose);
    osLibHandle->funcMap["igsc_device_update_late_binding_config"] = dummyFunction;
    osLibHandle->funcMap["igsc_ifr_get_status_ext"] = dummyFunction;
    osLibHandle->funcMap["igsc_ifr_run_mem_ppr_test"] = dummyFunction;
    return osLibHandle;
}

static OsLibrary *mockLoadEmptyFwUtilLibraryByName(const OsLibraryCreateProperties &properties) {
    mockLoadedLibraryNames.push_back(properties.libraryName);
    return new MockFwUtilOsLibrary();
}

class FwUtilLibraryNamesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mockLoadedLibraryNames.clear();
        mockValidLibraryNames.clear();
    }

    void TearDown() override {
        std::vector<std::string>().swap(mockLoadedLibraryNames);
        std::vector<std::string>().swap(mockValidLibraryNames);
    }

    VariableBackup<std::vector<std::string>> libraryNamesBackup{&L0::Sysman::FirmwareUtilImp::fwUtilLibraryNames, {"libigsc_mock.so.1", "libigsc_mock.so.0"}};
    VariableBackup<decltype(L0::Sysman::deviceInitByDevice)> deviceInitByDeviceBackup{&L0::Sysman::deviceInitByDevice};
    VariableBackup<decltype(L0::Sysman::deviceGetDeviceInfo)> deviceGetDeviceInfoBackup{&L0::Sysman::deviceGetDeviceInfo};
    VariableBackup<decltype(L0::Sysman::deviceGetFwVersion)> deviceGetFwVersionBackup{&L0::Sysman::deviceGetFwVersion};
    VariableBackup<decltype(L0::Sysman::deviceGetFwDataVersion)> deviceGetFwDataVersionBackup{&L0::Sysman::deviceGetFwDataVersion};
    VariableBackup<decltype(L0::Sysman::deviceIteratorCreate)> deviceIteratorCreateBackup{&L0::Sysman::deviceIteratorCreate};
    VariableBackup<decltype(L0::Sysman::deviceItreatorNext)> deviceItreatorNextBackup{&L0::Sysman::deviceItreatorNext};
    VariableBackup<decltype(L0::Sysman::deviceItreatorDestroy)> deviceItreatorDestroyBackup{&L0::Sysman::deviceItreatorDestroy};
    VariableBackup<decltype(L0::Sysman::deviceFwUpdate)> deviceFwUpdateBackup{&L0::Sysman::deviceFwUpdate};
    VariableBackup<decltype(L0::Sysman::deviceFwDataUpdate)> deviceFwDataUpdateBackup{&L0::Sysman::deviceFwDataUpdate};
    VariableBackup<decltype(L0::Sysman::imageOpromInit)> imageOpromInitBackup{&L0::Sysman::imageOpromInit};
    VariableBackup<decltype(L0::Sysman::imageOpromType)> imageOpromTypeBackup{&L0::Sysman::imageOpromType};
    VariableBackup<decltype(L0::Sysman::deviceOpromUpdate)> deviceOpromUpdateBackup{&L0::Sysman::deviceOpromUpdate};
    VariableBackup<decltype(L0::Sysman::deviceOpromVersion)> deviceOpromVersionBackup{&L0::Sysman::deviceOpromVersion};
    VariableBackup<decltype(L0::Sysman::deviceGetPscVersion)> deviceGetPscVersionBackup{&L0::Sysman::deviceGetPscVersion};
    VariableBackup<decltype(L0::Sysman::deviceClose)> deviceCloseBackup{&L0::Sysman::deviceClose};
    VariableBackup<decltype(L0::Sysman::deviceUpdateLateBindingConfig)> deviceUpdateLateBindingConfigBackup{&L0::Sysman::deviceUpdateLateBindingConfig};
    VariableBackup<decltype(L0::Sysman::deviceIfrGetStatusExt)> deviceIfrGetStatusExtBackup{&L0::Sysman::deviceIfrGetStatusExt};
    VariableBackup<decltype(L0::Sysman::deviceIfrRunMemPPRTest)> deviceIfrRunMemPPRTestBackup{&L0::Sysman::deviceIfrRunMemPPRTest};
};

TEST_F(FwUtilLibraryNamesTest, GivenNoLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenAllLibraryNamesAreTriedInOrderAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, L0::Sysman::FirmwareUtilImp::fwUtilLibraryNames);
}

TEST_F(FwUtilLibraryNamesTest, GivenFirstLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsCreatedWithoutTryingOtherLibraries) {
    mockValidLibraryNames = {"libigsc_mock.so.1", "libigsc_mock.so.0"};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    ASSERT_NE(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, std::vector<std::string>({"libigsc_mock.so.1"}));
    delete pFwUtil;
}

TEST_F(FwUtilLibraryNamesTest, GivenOnlySecondLibraryCanBeLoadedWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsCreatedUsingSecondLibrary) {
    mockValidLibraryNames = {"libigsc_mock.so.0"};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    ASSERT_NE(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, L0::Sysman::FirmwareUtilImp::fwUtilLibraryNames);
    delete pFwUtil;
}

TEST_F(FwUtilLibraryNamesTest, GivenFirstLibraryLoadsButEntryPointsAreMissingWhenCreatingFirmwareUtilInterfaceThenOtherLibrariesAreNotTriedAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadEmptyFwUtilLibraryByName};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_EQ(mockLoadedLibraryNames, std::vector<std::string>({"libigsc_mock.so.1"}));
}

TEST_F(FwUtilLibraryNamesTest, GivenEmptyLibraryNamesListWhenCreatingFirmwareUtilInterfaceThenNoLibraryIsLoadedAndFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<std::vector<std::string>> emptyLibraryNamesBackup{&L0::Sysman::FirmwareUtilImp::fwUtilLibraryNames, {}};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, mockLoadValidFwUtilLibraryByName};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
    EXPECT_TRUE(mockLoadedLibraryNames.empty());
}

TEST(FwUtilDeleteTest, GivenLibraryWasNotSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsNotAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    VariableBackup<decltype(L0::Sysman::deviceClose)> mockDeviceClose(&L0::Sysman::deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 0u);
}

TEST(FwUtilDeleteTest, GivenLibraryWasSetWhenFirmwareUtilInterfaceIsDeletedThenLibraryFunctionIsAccessed) {

    mockFwUtilDeviceCloseCallCount = 0;

    VariableBackup<decltype(L0::Sysman::deviceClose)> mockDeviceClose(&L0::Sysman::deviceClose, [](struct igsc_device_handle *handle) -> int {
        mockFwUtilDeviceCloseCallCount++;
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    // Prepare dummy OsLibrary for library, since no access is expected
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    delete pFwUtilImp;
    EXPECT_EQ(mockFwUtilDeviceCloseCallCount, 1u);
}

class FwUtilTestFixture : public ::testing::Test {
  protected:
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = nullptr;
    MockFwUtilOsLibrary *osLibHandle = nullptr;
    std::vector<std::string> fwTypes;
    void SetUp() override {
        pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
        osLibHandle = new MockFwUtilOsLibrary();
        osLibHandle->funcMap["igsc_device_fw_version"] = reinterpret_cast<void *>(&mockIgscDeviceFwVersion);
        osLibHandle->funcMap["igsc_device_fwdata_version"] = reinterpret_cast<void *>(&mockIgscDeviceFwDataVersion);
        osLibHandle->funcMap["igsc_device_oprom_version"] = reinterpret_cast<void *>(&mockIgscDeviceOpromVersion);
        osLibHandle->funcMap["igsc_device_psc_version"] = reinterpret_cast<void *>(&mockIgscDevicePscVersion);
        pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
        pFwUtilImp->getDeviceSupportedFwTypes(fwTypes);
    }

    void TearDown() override {
        delete pFwUtilImp->libraryHandle;
        pFwUtilImp->libraryHandle = nullptr;
        delete pFwUtilImp;
    }
};

TEST_F(FwUtilTestFixture, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndGetProcAddressFailsThenFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST_F(FwUtilTestFixture, GivenLibraryWasSetWhenCreatingFirmwareUtilInterfaceAndIgscDeviceIterCreateFailsThenFirmwareUtilInterfaceIsNotCreated) {
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST_F(FwUtilTestFixture, GivenLibraryWasNotSetWhenCreatingFirmwareUtilInterfaceThenFirmwareUtilInterfaceIsNotCreated) {
    L0::Sysman::ult::MockFwUtilOsLibrary::mockLoad = false;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockFwUtilOsLibrary::load};
    L0::Sysman::FirmwareUtil *pFwUtil = L0::Sysman::FirmwareUtil::create(0, 0, 0, 0);
    EXPECT_EQ(pFwUtil, nullptr);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenGetVersionIsCalledForGfxDataTypeAndIgscCallFailsThenCallReturnsError) {
    VariableBackup<decltype(L0::Sysman::deviceGetFwDataVersion)> mockFirmwareVersionFailure(&L0::Sysman::deviceGetFwDataVersion, [](struct igsc_device_handle *handle, struct igsc_fwdata_version *version) -> int {
        return IGSC_ERROR_BAD_IMAGE;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    std::string firmwareVersion;
    ze_result_t result = pFwUtilImp->getFwVersion(*gfxDataIt, firmwareVersion);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenGetVersionIsCalledForGfxDataTypeAndPermissionIsDeniedThenCallReturnsPermissionError) {
    VariableBackup<decltype(L0::Sysman::deviceGetFwDataVersion)> mockFirmwareVersionFailure(&L0::Sysman::deviceGetFwDataVersion, [](struct igsc_device_handle *handle, struct igsc_fwdata_version *version) -> int {
        return IGSC_ERROR_PERMISSION_DENIED;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    std::string firmwareVersion;
    ze_result_t result = pFwUtilImp->getFwVersion(*gfxDataIt, firmwareVersion);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenGetVersionIsCalledForGfxDataTypeThenProperVersionIsReturned) {
    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    std::string firmwareVersion;
    ze_result_t result = pFwUtilImp->getFwVersion(*gfxDataIt, firmwareVersion);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::string expectedFirmwareVersion = "Major : " + std::to_string(fwDataMajorVersion) + ", OEM Manufacturing Data : " + std::to_string(fwDataOemVersion) + ", Major VCN : " + std::to_string(fwDataVcnVersion);
    EXPECT_EQ(expectedFirmwareVersion, firmwareVersion);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenFirmwareFlashIsCalledForGfxDataTypeAndIgscCallFailsThenFlashingFails) {
    VariableBackup<decltype(L0::Sysman::deviceFwDataUpdate)> mockFirmwareFlashFailure(&L0::Sysman::deviceFwDataUpdate, [](struct igsc_device_handle *handle, const uint8_t *buffer, const uint32_t bufferLen, igsc_progress_func_t progressFunc, void *ctx) -> int {
        return IGSC_ERROR_BAD_IMAGE;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto result = pFwUtilImp->flashFirmware(*gfxDataIt, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenFirmwareFlashIsCalledForGfxDataTypeAndPermissionIsDeniedThenPermissionErrorIsReturned) {
    VariableBackup<decltype(L0::Sysman::deviceFwDataUpdate)> mockFirmwareFlashFailure(&L0::Sysman::deviceFwDataUpdate, [](struct igsc_device_handle *handle, const uint8_t *buffer, const uint32_t bufferLen, igsc_progress_func_t progressFunc, void *ctx) -> int {
        return IGSC_ERROR_PERMISSION_DENIED;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto result = pFwUtilImp->flashFirmware(*gfxDataIt, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenFirmwareFlashIsCalledForGfxDataTypeAndNoMemoryIsReportedThenOutOfHostMemoryIsReturned) {
    VariableBackup<decltype(L0::Sysman::deviceFwDataUpdate)> mockFirmwareFlashFailure(&L0::Sysman::deviceFwDataUpdate, [](struct igsc_device_handle *handle, const uint8_t *buffer, const uint32_t bufferLen, igsc_progress_func_t progressFunc, void *ctx) -> int {
        return IGSC_ERROR_NOMEM;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto result = pFwUtilImp->flashFirmware(*gfxDataIt, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
    EXPECT_EQ(result, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
}

TEST_F(FwUtilTestFixture, GivenFirmwareUtilInstanceWhenFirmwareFlashIsCalledForGfxDataTypeAndIgscCallSucceedsThenFlashingIsSuccessful) {
    VariableBackup<decltype(L0::Sysman::deviceFwDataUpdate)> mockFirmwareFlashSuccess(&L0::Sysman::deviceFwDataUpdate, [](struct igsc_device_handle *handle, const uint8_t *buffer, const uint32_t bufferLen, igsc_progress_func_t progressFunc, void *ctx) -> int {
        return IGSC_SUCCESS;
    });

    auto gfxDataIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("GFX_DATA"));
    EXPECT_NE(gfxDataIt, fwTypes.end());

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);

    auto result = pFwUtilImp->flashFirmware(*gfxDataIt, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

static uint32_t mockOpromImageType = 0;
static int mockOpromDataUpdateResult = IGSC_SUCCESS;
static int mockOpromCodeUpdateResult = IGSC_SUCCESS;

static int mockIgscImageOpromInit(struct igsc_oprom_image **img,
                                  const uint8_t *buffer,
                                  uint32_t bufferLen) {
    return IGSC_SUCCESS;
}

static int mockIgscImageOpromType(struct igsc_oprom_image *img,
                                  uint32_t *opromType) {
    *opromType = mockOpromImageType;
    return IGSC_SUCCESS;
}

static int mockIgscDeviceOpromUpdate(struct igsc_device_handle *handle,
                                     uint32_t opromType,
                                     struct igsc_oprom_image *img,
                                     igsc_progress_func_t progressFunc,
                                     void *ctx) {
    return (opromType == IGSC_OPROM_DATA) ? mockOpromDataUpdateResult : mockOpromCodeUpdateResult;
}

class FwUtilOpromFlashTestFixture : public FwUtilTestFixture {
  protected:
    void SetUp() override {
        FwUtilTestFixture::SetUp();
        mockOpromImageType = 0;
        mockOpromDataUpdateResult = IGSC_SUCCESS;
        mockOpromCodeUpdateResult = IGSC_SUCCESS;
    }

    ze_result_t flashOprom() {
        auto opromIt = std::find(fwTypes.begin(), fwTypes.end(), std::string("OptionROM"));
        EXPECT_NE(opromIt, fwTypes.end());

        uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
        memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
        return pFwUtilImp->flashFirmware(*opromIt, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
    }

    VariableBackup<decltype(L0::Sysman::imageOpromInit)> backupImageOpromInit{&L0::Sysman::imageOpromInit, &mockIgscImageOpromInit};
    VariableBackup<decltype(L0::Sysman::imageOpromType)> backupImageOpromType{&L0::Sysman::imageOpromType, &mockIgscImageOpromType};
    VariableBackup<decltype(L0::Sysman::deviceOpromUpdate)> backupDeviceOpromUpdate{&L0::Sysman::deviceOpromUpdate, &mockIgscDeviceOpromUpdate};
};

TEST_F(FwUtilOpromFlashTestFixture, GivenDataOnlyOpromImageWhenDataUpdateFailsThenFlashingFails) {
    mockOpromImageType = IGSC_OPROM_DATA;
    mockOpromDataUpdateResult = IGSC_ERROR_BAD_IMAGE;

    EXPECT_EQ(flashOprom(), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilOpromFlashTestFixture, GivenCodeOnlyOpromImageWhenCodeUpdateFailsThenFlashingFails) {
    mockOpromImageType = IGSC_OPROM_CODE;
    mockOpromCodeUpdateResult = IGSC_ERROR_BAD_IMAGE;

    EXPECT_EQ(flashOprom(), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilOpromFlashTestFixture, GivenOpromImageWithBothPartitionsWhenOnlyDataUpdateFailsThenDataErrorIsReturned) {
    mockOpromImageType = IGSC_OPROM_DATA | IGSC_OPROM_CODE;
    mockOpromDataUpdateResult = IGSC_ERROR_BAD_IMAGE;

    EXPECT_EQ(flashOprom(), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilOpromFlashTestFixture, GivenOpromImageWithBothPartitionsWhenOnlyCodeUpdateFailsThenCodeErrorIsReturned) {
    mockOpromImageType = IGSC_OPROM_DATA | IGSC_OPROM_CODE;
    mockOpromCodeUpdateResult = IGSC_ERROR_PERMISSION_DENIED;

    EXPECT_EQ(flashOprom(), ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
}

TEST_F(FwUtilOpromFlashTestFixture, GivenOpromImageWithBothPartitionsWhenBothUpdatesFailThenFirstErrorIsReturned) {
    mockOpromImageType = IGSC_OPROM_DATA | IGSC_OPROM_CODE;
    mockOpromDataUpdateResult = IGSC_ERROR_BAD_IMAGE;
    mockOpromCodeUpdateResult = IGSC_ERROR_PERMISSION_DENIED;

    EXPECT_EQ(flashOprom(), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(FwUtilOpromFlashTestFixture, GivenOpromImageWithBothPartitionsWhenBothUpdatesSucceedThenFlashingIsSuccessful) {
    mockOpromImageType = IGSC_OPROM_DATA | IGSC_OPROM_CODE;

    EXPECT_EQ(flashOprom(), ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
