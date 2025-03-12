/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace NEO {
namespace SysCalls {
extern bool allowFakeDevicePath;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace Sysman {
namespace ult {

struct dirent mockEntries[] = {
    {0, 0, 0, 0, "."},
    {0, 0, 0, 0, "mockDir1"},
    {0, 0, 0, 0, "mockDir2"},
};

inline static int mockStatFailure(const std::string &filePath, struct stat *statbuf) noexcept {
    return -1;
}

inline static int mockStatFailure2(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR;
    return 0;
}

inline static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR | S_IFREG;
    return 0;
}

inline static int mockStatNoPermissions(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = 0;
    return 0;
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanKmdInterfaceWhenCallingListDirectoriesThenSuccessIsReturned) {

    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pFsAccess = pSysmanKmdInterface->getFsAccess();
    const uint32_t numEntries = sizeof(mockEntries) / sizeof(mockEntries[0]);

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::vector<std::string> listFiles;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFsAccess->listDirectory("MockDir", listFiles));
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingDirectoryExistsWithValidAndInvalidPathThenSuccessAndFailureAreReturnedRespectively) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    NEO::SysCalls::allowFakeDevicePath = true;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(tempFsAccess->directoryExists(path));
    NEO::SysCalls::allowFakeDevicePath = false;
    path = "invalidDiretory";
    EXPECT_FALSE(tempFsAccess->directoryExists(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicSysfsAccessClassWhenCallingDirectoryExistsWithInvalidPathThenFalseIsRetured) {
    PublicFsAccess *tempSysfsAccess = new PublicFsAccess();
    std::string path = "invalidDiretory";
    EXPECT_FALSE(tempSysfsAccess->directoryExists(path));
    delete tempSysfsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserHavingWritePermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingSysmanFromHandleThenValidSysmanDeviceIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    SysmanDevice *sysmanDevice = SysmanDevice::fromHandle(validHandle);
    EXPECT_NE(nullptr, sysmanDevice);
    EXPECT_EQ(pSysmanDevice, sysmanDevice);
}

TEST_F(SysmanDeviceFixture, GivenInvalidSysmanDeviceHandleWhenCallingSysmanFromHandleThenNullptrIsReturned) {
    zes_device_handle_t invalidHandle = nullptr;
    EXPECT_EQ(nullptr, SysmanDevice::fromHandle(invalidHandle));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleAndGivenGlobalSysmanDriverIsNullptrWhenCallingSysmanFromHandleThenValidSysmanDeviceIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    L0::Sysman::globalSysmanDriver = nullptr;
    EXPECT_EQ(nullptr, SysmanDevice::fromHandle(validHandle));
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserHavingReadPermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanMultiDeviceFixture, GivenInvalidSysmanDeviceHandleWhenCallingSysmanDeviceFunctionsforMultipleDevicesThenUninitializedErrorIsReturned) {
    zes_device_handle_t invalidHandle = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::performanceGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGetCardDomain(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::frequencyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::temperatureGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::standbyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetSubDeviceProperties(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::processesGetState(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceReset(invalidHandle, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::engineGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetBars(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetStats(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::schedulerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::rasGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::memoryGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fanGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::diagnosticsGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::firmwareGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEventRegister(invalidHandle, u_int32_t(0)));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccAvailable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccConfigurable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetEccState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceSetEccState(invalidHandle, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceResetExt(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGetMultiPortThroughput(invalidHandle, count, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEnumEnabledVF(invalidHandle, &count, nullptr));
}

TEST_F(SysmanDeviceFixture, GivenInvalidSysmanDeviceHandleWhenCallingSysmanDeviceFunctionsThenUninitializedErrorIsReturned) {
    zes_device_handle_t invalidHandle = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::performanceGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGetCardDomain(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::frequencyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::temperatureGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::standbyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetSubDeviceProperties(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::processesGetState(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceReset(invalidHandle, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::engineGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetBars(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetStats(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::schedulerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::rasGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::memoryGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fanGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::diagnosticsGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::firmwareGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEventRegister(invalidHandle, u_int32_t(0)));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccAvailable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccConfigurable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetEccState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceSetEccState(invalidHandle, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceResetExt(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGetMultiPortThroughput(invalidHandle, count, nullptr, nullptr));
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserNotHavingWritePermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatNoPermissions);
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserNotHavingReadPermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatNoPermissions);
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatFailure);
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatFailure);
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidPathnameWhenCallingFsAccessExistsThenSuccessIsReturned) {
    VariableBackup<bool> allowFakeDevicePathBackup(&SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    auto fsAccess = &pLinuxSysmanImp->getFsAccess();

    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(fsAccess->fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenStatCallFailsWhenCallingFsAccessExistsThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatFailure);
    auto fsAccess = &pLinuxSysmanImp->getSysfsAccess();

    std::string path = "";
    EXPECT_FALSE(fsAccess->fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenPathIsNotOfFileTypeWhenCallingFsAccessExistsThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatFailure2);
    auto fsAccess = &pLinuxSysmanImp->getSysfsAccess();

    std::string path = "";
    EXPECT_FALSE(fsAccess->fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPathnameWhenCallingFsAccessExistsThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    auto fsAccess = &pLinuxSysmanImp->getFsAccess();

    std::string path = "noSuchFileOrDirectory";
    EXPECT_FALSE(fsAccess->fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPathnameWhenCallingSysFsAccessScanDirEntriesThenErrorIsReturned) {
    auto pSysFsAccess = &pLinuxSysmanImp->getSysfsAccess();

    std::string path = "noSuchDirectory";
    std::vector<std::string> listFiles;
    EXPECT_NE(ZE_RESULT_SUCCESS, pSysFsAccess->scanDirEntries(path, listFiles));
}

TEST_F(SysmanDeviceFixture, GivenSysfsAccessClassAndIntegerWhenCallingReadOnMultipleFilesThenSuccessIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string value = "123";
        memcpy(buf, value.data(), value.size());
        return value.size();
    });

    PublicSysfsAccess *tempSysfsAccess = new PublicSysfsAccess();
    std::string fileName = {};
    int iVal32;
    for (auto i = 0; i < L0::Sysman::FdCacheInterface::maxSize + 2; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_EQ(ZE_RESULT_SUCCESS, tempSysfsAccess->read(fileName, iVal32));
    }
    delete tempSysfsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidMockMutexFsAccessWhenCallingReadThenMutexLockCounterMatchesNumberOfReadCalls) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string value = "123";
        memcpy(buf, value.data(), value.size());
        return value.size();
    });

    class MockMutexFsAccess : public L0::Sysman::FsAccessInterface {
      public:
        uint32_t mutexLockCounter = 0;
        std::unique_lock<std::mutex> obtainMutex() override {
            mutexLockCounter++;
            std::unique_lock<std::mutex> mutexLock = L0::Sysman::FsAccessInterface::obtainMutex();
            EXPECT_TRUE(mutexLock.owns_lock());
            return mutexLock;
        }
    };
    MockMutexFsAccess *tempFsAccess = new MockMutexFsAccess();
    std::string fileName = {};
    uint32_t iVal32;
    uint32_t testReadCount = 10;
    for (uint32_t i = 0; i < testReadCount; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->read(fileName, iVal32));
    }
    EXPECT_EQ(tempFsAccess->mutexLockCounter, testReadCount);
    delete tempFsAccess;
}

TEST(FdCacheTest, GivenValidFdCacheWhenCallingGetFdOnSameFileThenVerifyCacheIsUpdatedProperly) {

    class MockFdCache : public FdCacheInterface {
      public:
        using FdCacheInterface::fdMap;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    std::unique_ptr<MockFdCache> pFdCache = std::make_unique<MockFdCache>();
    std::string fileName = {};
    for (auto i = 0; i < L0::Sysman::FdCacheInterface::maxSize; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    fileName = "mockfile0.txt";
    for (auto i = 0; i < 3; i++) {
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    for (auto i = 1; i < L0::Sysman::FdCacheInterface::maxSize - 1; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    // Get Fd after the cache is full.
    EXPECT_LE(0, pFdCache->getFd("dummy.txt"));

    // Verify Cache have the elements that are accessed more number of times
    EXPECT_NE(pFdCache->fdMap.end(), pFdCache->fdMap.find("mockfile0.txt"));

    // Verify cache doesn't have an element that is accessed less number of times.
    EXPECT_EQ(pFdCache->fdMap.end(), pFdCache->fdMap.find("mockfile9.txt"));
}

TEST(FdCacheTest, GivenValidFdCacheWhenClearingCacheThenVerifyProperFdsAreClosedAndCacheIsUpdatedProperly) {

    class MockFdCache : public FdCacheInterface {
      public:
        using FdCacheInterface::fdMap;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
        EXPECT_EQ(fileDescriptor, 1);
        return 0;
    });

    MockFdCache *pFdCache = new MockFdCache();
    std::string fileName = {};
    for (auto i = 0; i < L0::Sysman::FdCacheInterface::maxSize; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    fileName = "mockfile0.txt";
    for (auto i = 0; i < 3; i++) {
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    for (auto i = 1; i < L0::Sysman::FdCacheInterface::maxSize - 1; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    // Get Fd after the cache is full.
    EXPECT_LE(0, pFdCache->getFd("dummy.txt"));

    // Verify Cache have the elements that are accessed more number of times
    EXPECT_NE(pFdCache->fdMap.end(), pFdCache->fdMap.find("mockfile0.txt"));

    // Verify cache doesn't have an element that is accessed less number of times.
    EXPECT_EQ(pFdCache->fdMap.end(), pFdCache->fdMap.find("mockfile9.txt"));

    delete pFdCache;
}

TEST_F(SysmanDeviceFixture, GivenSysfsAccessClassAndOpenSysCallFailsWhenCallingReadThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string value = "123";
        memcpy(buf, value.data(), value.size());
        return value.size();
    });

    PublicSysfsAccess *tempSysfsAccess = new PublicSysfsAccess();
    const std::string fileName = "mockFile.txt";
    int iVal32;

    errno = ENOENT;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, tempSysfsAccess->read(fileName, iVal32));
    delete tempSysfsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidPidWhenCallingProcfsAccessIsAliveThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&SysCalls::allowFakeDevicePath, true);
    auto procfsAccess = &pLinuxSysmanImp->getProcfsAccess();

    EXPECT_TRUE(procfsAccess->isAlive(getpid()));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPidWhenCallingProcfsAccessIsAliveThenErrorIsReturned) {
    auto procfsAccess = &pLinuxSysmanImp->getProcfsAccess();

    EXPECT_FALSE(procfsAccess->isAlive(reinterpret_cast<::pid_t>(-1)));
}

TEST_F(SysmanDeviceFixture, GivenValidPciPathWhileGettingCardBusPortThenReturnedPathIs1LevelUpThenTheCurrentPath) {
    const std::string mockBdf = "0000:00:02.0";
    const std::string mockRealPath = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/" + mockBdf;
    const std::string mockRealPath1LevelUp = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0";

    std::string pciRootPort1 = pLinuxSysmanImp->getPciCardBusDirectoryPath(mockRealPath);
    EXPECT_EQ(pciRootPort1, mockRealPath1LevelUp);

    std::string pciRootPort2 = pLinuxSysmanImp->getPciCardBusDirectoryPath("device");
    EXPECT_EQ(pciRootPort2, "device");
}

TEST_F(SysmanDeviceFixture, GivenSysfsAccessAndValidDeviceNameWhenCallingBindDeviceThenSuccessIsReturned) {

    std::string_view deviceName = "card0";

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> mockPwrite(&NEO::SysCalls::sysCallsPwrite, [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
        std::string_view deviceName = "card0";
        return static_cast<ssize_t>(deviceName.size());
    });

    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysfsAccess->bindDevice(pSysmanKmdInterface->getGpuBindEntry(), deviceName.data()));
}

TEST_F(SysmanDeviceFixture, GivenSysfsAccessAndValidDeviceNameWhenCallingUnbindDeviceThenSuccessIsReturned) {

    std::string_view deviceName = "card0";

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> mockPwrite(&NEO::SysCalls::sysCallsPwrite, [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
        std::string_view deviceName = "card0";
        return static_cast<ssize_t>(deviceName.size());
    });

    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysfsAccess->unbindDevice(pSysmanKmdInterface->getGpuUnBindEntry(), deviceName.data()));
}

TEST_F(SysmanMultiDeviceFixture, GivenValidEffectiveUserIdCheckWhetherPermissionsReturnedByIsRootUserAreCorrect) {
    int euid = geteuid();
    auto pFsAccess = &pLinuxSysmanImp->getFsAccess();
    if (euid == 0) {
        EXPECT_EQ(true, pFsAccess->isRootUser());
    } else {
        EXPECT_EQ(false, pFsAccess->isRootUser());
    }
}

TEST(SysmanUnknownDriverModelTest, GivenDriverModelTypeIsNotDrmWhenExecutingSysmanOnLinuxThenErrorIsReturned) {
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxSysmanImp->init());
}

TEST(SysmanErrorCodeTest, GivenDifferentErrorCodesWhenCallingGetResultThenVerifyProperZeResultErrorIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, LinuxSysmanImp::getResult(EPERM));
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, LinuxSysmanImp::getResult(EACCES));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, LinuxSysmanImp::getResult(ENOENT));
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, LinuxSysmanImp::getResult(EBUSY));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, LinuxSysmanImp::getResult(EEXIST));
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWithInvalidPciDomainWhenCallingGenerateUuidFromPciBusInfoThenFalseIsReturned) {
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
    uint32_t subDeviceId = 0;
    bool result = pLinuxSysmanImp->generateUuidFromPciAndSubDeviceInfo(subDeviceId, pciBusInfo, uuid);
    EXPECT_FALSE(result);
}

TEST_F(SysmanDeviceFixture, GivenNullOsInterfaceObjectWhenRetrievingUuidsOfDeviceThenNoUuidsAreReturned) {
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);

    auto &rootDeviceEnvironment = (pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    rootDeviceEnvironment.osInterface = nullptr;

    std::vector<std::string> uuids;
    pLinuxSysmanImp->getDeviceUuids(uuids);
    EXPECT_EQ(0u, uuids.size());
}

TEST_F(SysmanDeviceFixture, GivenInvalidPciBusInfoWhenRetrievingUuidThenFalseIsReturned) {
    DebugManagerStateRestore restore;
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();

    auto driverModel = std::make_unique<NEO::MockDriverModel>();
    driverModel->pciSpeedInfo = {1, 1, 1};
    PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
    driverModel->pciBusInfo = pciBusInfo;
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(driverModel));

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);

    debugManager.flags.EnableChipsetUniqueUUID.set(0);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    uint32_t subDeviceId = 0;
    bool result = pLinuxSysmanImp->getUuidFromSubDeviceInfo(subDeviceId, uuid);
    EXPECT_FALSE(result);
}

struct MockSysmanLinuxProductHelper : public ProductHelperHw<IGFX_UNKNOWN> {
    MockSysmanLinuxProductHelper() = default;
    bool getUuid(DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override {
        auto pDrm = driverModel->as<Drm>();
        if (pDrm->getFileDescriptor() >= 0) {
            return true;
        }
        return false;
    }
};

TEST_F(SysmanDeviceFixture, GivenValidDeviceWhenRetrievingUuidThenValidFdIsVerifiedInProductHelper) {
    std::unique_ptr<ProductHelper> mockProductHelper = std::make_unique<MockSysmanLinuxProductHelper>();
    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);

    auto &rootDeviceEnvironment = (pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    uint32_t subDeviceId = 0;
    bool result = pLinuxSysmanImp->getUuidFromSubDeviceInfo(subDeviceId, uuid);
    EXPECT_TRUE(result);

    std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
