/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
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

inline static int mockAccessFailure(const char *pathname, int mode) {
    return -1;
}

inline static int mockAccessSuccess(const char *pathname, int mode) {
    return 0;
}

inline static int mockStatFailure(const char *pathname, struct stat *sb) noexcept {
    return -1;
}

inline static int mockStatSuccess(const char *pathname, struct stat *sb) noexcept {
    sb->st_mode = S_IWUSR | S_IRUSR;
    return 0;
}

inline static int mockStatNoPermissions(const char *pathname, struct stat *sb) noexcept {
    sb->st_mode = 0;
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

TEST_F(SysmanDeviceFixture, GivenCreateFsAccessHandleWhenCallinggetFsAccessThenCreatedFsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pFsAccess != nullptr) {
        // delete previously allocated pFsAccess
        delete pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = nullptr;
    }
    pLinuxSysmanImp->pFsAccess = L0::Sysman::FsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getFsAccess(), pLinuxSysmanImp->pFsAccess);
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingDirectoryExistsWithValidAndInvalidPathThenSuccessAndFailureAreReturnedRespectively) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->accessSyscall = mockAccessSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(tempFsAccess->directoryExists(path));
    tempFsAccess->accessSyscall = mockAccessFailure;
    path = "invalidDiretory";
    EXPECT_FALSE(tempFsAccess->directoryExists(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicSysfsAccessClassWhenCallingDirectoryExistsWithInvalidPathThenFalseIsRetured) {
    PublicFsAccess *tempSysfsAccess = new PublicFsAccess();
    tempSysfsAccess->accessSyscall = mockAccessFailure;
    std::string path = "invalidDiretory";
    EXPECT_FALSE(tempSysfsAccess->directoryExists(path));
    delete tempSysfsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserHavingWritePermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserHavingReadPermissionsThenSuccessIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatSuccess;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_SUCCESS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithUserNotHavingWritePermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatNoPermissions;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canWrite(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithUserNotHavingReadPermissionsThenInsufficientIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatNoPermissions;
    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanReadWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatFailure;
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenPublicFsAccessClassWhenCallingCanWriteWithInvalidPathThenErrorIsReturned) {
    PublicFsAccess *tempFsAccess = new PublicFsAccess();
    tempFsAccess->statSyscall = mockStatFailure;
    std::string path = "invalidPath";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, tempFsAccess->canRead(path));
    delete tempFsAccess;
}

TEST_F(SysmanDeviceFixture, GivenValidPathnameWhenCallingFsAccessExistsThenSuccessIsReturned) {
    VariableBackup<bool> allowFakeDevicePathBackup(&SysCalls::allowFakeDevicePath, true);
    auto fsAccess = pLinuxSysmanImp->getFsAccess();

    char cwd[PATH_MAX];
    std::string path = getcwd(cwd, PATH_MAX);
    EXPECT_TRUE(fsAccess.fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPathnameWhenCallingFsAccessExistsThenErrorIsReturned) {
    auto fsAccess = pLinuxSysmanImp->getFsAccess();

    std::string path = "noSuchFileOrDirectory";
    EXPECT_FALSE(fsAccess.fileExists(path));
}

TEST_F(SysmanDeviceFixture, GivenCreateSysfsAccessHandleWhenCallinggetSysfsAccessThenCreatedSysfsAccessHandleHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pSysfsAccess != nullptr) {
        // delete previously allocated pSysfsAccess
        delete pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = nullptr;
    }
    pLinuxSysmanImp->pSysfsAccess = L0::Sysman::SysfsAccess::create("");
    EXPECT_EQ(&pLinuxSysmanImp->getSysfsAccess(), pLinuxSysmanImp->pSysfsAccess);
}

TEST_F(SysmanDeviceFixture, GivenCreateProcfsAccessHandleWhenCallinggetProcfsAccessThenCreatedProcfsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pProcfsAccess != nullptr) {
        // delete previously allocated pProcfsAccess
        delete pLinuxSysmanImp->pProcfsAccess;
        pLinuxSysmanImp->pProcfsAccess = nullptr;
    }
    pLinuxSysmanImp->pProcfsAccess = L0::Sysman::ProcfsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getProcfsAccess(), pLinuxSysmanImp->pProcfsAccess);
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
    for (auto i = 0; i < L0::Sysman::FdCache::maxSize + 2; i++) {
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

    class MockFdCache : public FdCache {
      public:
        using FdCache::fdMap;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    std::unique_ptr<MockFdCache> pFdCache = std::make_unique<MockFdCache>();
    std::string fileName = {};
    for (auto i = 0; i < L0::Sysman::FdCache::maxSize; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    fileName = "mockfile0.txt";
    for (auto i = 0; i < 3; i++) {
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    for (auto i = 1; i < L0::Sysman::FdCache::maxSize - 1; i++) {
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

    class MockFdCache : public FdCache {
      public:
        using FdCache::fdMap;
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
    for (auto i = 0; i < L0::Sysman::FdCache::maxSize; i++) {
        fileName = "mockfile" + std::to_string(i) + ".txt";
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    fileName = "mockfile0.txt";
    for (auto i = 0; i < 3; i++) {
        EXPECT_LE(0, pFdCache->getFd(fileName));
    }

    for (auto i = 1; i < L0::Sysman::FdCache::maxSize - 1; i++) {
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
    VariableBackup<bool> allowFakeDevicePathBackup(&SysCalls::allowFakeDevicePath, true);
    auto procfsAccess = pLinuxSysmanImp->getProcfsAccess();

    EXPECT_TRUE(procfsAccess.isAlive(getpid()));
}

TEST_F(SysmanDeviceFixture, GivenInvalidPidWhenCallingProcfsAccessIsAliveThenErrorIsReturned) {
    auto procfsAccess = pLinuxSysmanImp->getProcfsAccess();

    EXPECT_FALSE(procfsAccess.isAlive(reinterpret_cast<::pid_t>(-1)));
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

TEST_F(SysmanMultiDeviceFixture, GivenValidEffectiveUserIdCheckWhetherPermissionsReturnedByIsRootUserAreCorrect) {
    int euid = geteuid();
    auto pFsAccess = pLinuxSysmanImp->getFsAccess();
    if (euid == 0) {
        EXPECT_EQ(true, pFsAccess.isRootUser());
    } else {
        EXPECT_EQ(false, pFsAccess.isRootUser());
    }
}

TEST(SysmanUnknownDriverModelTest, GivenDriverModelTypeIsNotDrmWhenExecutingSysmanOnLinuxThenErrorIsReturned) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.capabilityTable.levelZeroSupported = true;
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pSysmanDeviceImp->pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxSysmanImp->init());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
