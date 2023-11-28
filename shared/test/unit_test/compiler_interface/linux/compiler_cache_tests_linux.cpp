/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_compiler_cache.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "os_inc.h"

#include <array>
#include <list>
#include <memory>

using namespace NEO;

class CompilerCacheMockLinux : public CompilerCache {
  public:
    CompilerCacheMockLinux(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;
};

namespace EvictCachePass {
decltype(NEO::SysCalls::sysCallsScandir) mockScandir = [](const char *dirp,
                                                          struct dirent ***namelist,
                                                          int (*filter)(const struct dirent *),
                                                          int (*compar)(const struct dirent **,
                                                                        const struct dirent **)) -> int {
    struct dirent **v = (struct dirent **)malloc(6 * (sizeof(struct dirent *)));

    v[0] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[0]->d_name, sizeof(v[0]->d_name), "file1.cl_cache", sizeof("file1.cl_cache"));

    v[1] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[1]->d_name, sizeof(v[1]->d_name), "file2.cl_cache", sizeof("file2.cl_cache"));

    v[2] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[2]->d_name, sizeof(v[2]->d_name), "file3.cl_cache", sizeof("file3.cl_cache"));

    v[3] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[3]->d_name, sizeof(v[3]->d_name), "file4.cl_cache", sizeof("file4.cl_cache"));

    v[4] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[4]->d_name, sizeof(v[4]->d_name), "file5.cl_cache", sizeof("file5.cl_cache"));

    v[5] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[5]->d_name, sizeof(v[5]->d_name), "file6.cl_cache", sizeof("file6.cl_cache"));

    *namelist = v;
    return 6;
};

decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
    if (filePath.find("file1") != filePath.npos) {
        statbuf->st_atime = 3;
    }
    if (filePath.find("file2") != filePath.npos) {
        statbuf->st_atime = 6;
    }
    if (filePath.find("file3") != filePath.npos) {
        statbuf->st_atime = 1;
    }
    if (filePath.find("file4") != filePath.npos) {
        statbuf->st_atime = 2;
    }
    if (filePath.find("file5") != filePath.npos) {
        statbuf->st_atime = 4;
    }
    if (filePath.find("file6") != filePath.npos) {
        statbuf->st_atime = 5;
    }
    statbuf->st_size = (MemoryConstants::megaByte / 6) + 10;

    return 0;
};

std::vector<std::string> *unlinkFiles;

decltype(NEO::SysCalls::sysCallsUnlink) mockUnlink = [](const std::string &pathname) -> int {
    unlinkFiles->push_back(pathname);

    return 0;
};
} // namespace EvictCachePass

TEST(CompilerCacheTests, GivenCompilerCacheWithOneMegabyteWhenEvictCacheIsCalledThenUnlinkTwoOldestFiles) {
    std::vector<std::string> unlinkLocalFiles;
    EvictCachePass::unlinkFiles = &unlinkLocalFiles;

    VariableBackup<decltype(NEO::SysCalls::sysCallsScandir)> scandirBackup(&NEO::SysCalls::sysCallsScandir, EvictCachePass::mockScandir);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, EvictCachePass::mockStat);
    VariableBackup<decltype(NEO::SysCalls::sysCallsUnlink)> unlinkBackup(&NEO::SysCalls::sysCallsUnlink, EvictCachePass::mockUnlink);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    uint64_t bytesEvicted{0u};
    EXPECT_TRUE(cache.evictCache(bytesEvicted));

    EXPECT_EQ(unlinkLocalFiles.size(), 2u);

    EXPECT_NE(unlinkLocalFiles[0].find("file3"), unlinkLocalFiles[0].npos);
    EXPECT_NE(unlinkLocalFiles[1].find("file4"), unlinkLocalFiles[1].npos);
}

TEST(CompilerCacheTests, GivenCompilerCacheWithWhenScandirFailThenEvictCacheFail) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});
    VariableBackup<decltype(NEO::SysCalls::sysCallsScandir)> scandirBackup(&NEO::SysCalls::sysCallsScandir, [](const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **)) -> int { return -1; });

    uint64_t bytesEvicted{0u};
    EXPECT_FALSE(cache.evictCache(bytesEvicted));
}

namespace CreateUniqueTempFilePass {
decltype(NEO::SysCalls::sysCallsMkstemp) mockMkstemp = [](char *fileName) -> int {
    memcpy_s(&fileName[22], 20, "123456", sizeof("123456"));
    return 1;
};

decltype(NEO::SysCalls::sysCallsPwrite) mockPwrite = [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
    return count;
};
} // namespace CreateUniqueTempFilePass

TEST(CompilerCacheTests, GivenCompilerCacheWhenCreateUniqueTempFileAndWriteDataThenFileIsCreatedAndDataAreWritten) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkstemp)> mkstempBackup(&NEO::SysCalls::sysCallsMkstemp, CreateUniqueTempFilePass::mockMkstemp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pwriteBackup(&NEO::SysCalls::sysCallsPwrite, CreateUniqueTempFilePass::mockPwrite);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    char x[256] = "/home/cl_cache/mytemp.XXXXXX";
    char compare[256] = "/home/cl_cache/mytemp.123456";
    EXPECT_TRUE(cache.createUniqueTempFileAndWriteData(x, "1", 1));

    EXPECT_EQ(0, memcmp(x, compare, 28));
}

namespace CreateUniqueTempFileFail1 {
decltype(NEO::SysCalls::sysCallsMkstemp) mockMkstemp = [](char *fileName) -> int {
    return -1;
};
} // namespace CreateUniqueTempFileFail1

TEST(CompilerCacheTests, GivenCompilerCacheWithCreateUniqueTempFileAndWriteDataWhenMkstempFailThenFalseIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkstemp)> mkstempBackup(&NEO::SysCalls::sysCallsMkstemp, CreateUniqueTempFileFail1::mockMkstemp);
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    char x[256] = "/home/cl_cache/mytemp.XXXXXX";
    EXPECT_FALSE(cache.createUniqueTempFileAndWriteData(x, "1", 1));
}

namespace CreateUniqueTempFileFail2 {
decltype(NEO::SysCalls::sysCallsMkstemp) mockMkstemp = [](char *fileName) -> int {
    memcpy_s(&fileName[22], 20, "123456", sizeof("123456"));
    return 1;
};

decltype(NEO::SysCalls::sysCallsPwrite) mockPwrite = [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
    return -1;
};
} // namespace CreateUniqueTempFileFail2

TEST(CompilerCacheTests, GivenCompilerCacheWithCreateUniqueTempFileAndWriteDataWhenPwriteFailThenFalseIsReturnedAndCleanupIsMade) {
    std::vector<std::string> unlinkLocalFiles;
    EvictCachePass::unlinkFiles = &unlinkLocalFiles;

    VariableBackup<decltype(NEO::SysCalls::sysCallsUnlink)> unlinkBackup(&NEO::SysCalls::sysCallsUnlink, EvictCachePass::mockUnlink);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkstemp)> mkstempBackup(&NEO::SysCalls::sysCallsMkstemp, CreateUniqueTempFileFail2::mockMkstemp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pwriteBackup(&NEO::SysCalls::sysCallsPwrite, CreateUniqueTempFileFail2::mockPwrite);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    char x[256] = "/home/cl_cache/mytemp.XXXXXX";
    EXPECT_FALSE(cache.createUniqueTempFileAndWriteData(x, "1", 1));
    EXPECT_EQ(NEO::SysCalls::closeFuncArgPassed, 1);
    EXPECT_EQ(0, memcmp(x, unlinkLocalFiles[0].c_str(), 28));
}

TEST(CompilerCacheTests, GivenCompilerCacheWithCreateUniqueTempFileAndWriteDataWhenCloseFailThenFalseIsReturned) {
    int closeRetVal = -1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkstemp)> mkstempBackup(&NEO::SysCalls::sysCallsMkstemp, CreateUniqueTempFilePass::mockMkstemp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pwriteBackup(&NEO::SysCalls::sysCallsPwrite, CreateUniqueTempFilePass::mockPwrite);
    VariableBackup<decltype(NEO::SysCalls::closeFuncRetVal)> closeBackup(&NEO::SysCalls::closeFuncRetVal, closeRetVal);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    char x[256] = "/home/cl_cache/mytemp.XXXXXX";
    EXPECT_FALSE(cache.createUniqueTempFileAndWriteData(x, "1", 1));
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenRenameFailThenFalseIsReturned) {
    int unlinkTemp = 0;
    VariableBackup<decltype(NEO::SysCalls::unlinkCalled)> unlinkBackup(&NEO::SysCalls::unlinkCalled, unlinkTemp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRename)> renameBackup(&NEO::SysCalls::sysCallsRename, [](const char *currName, const char *dstName) -> int { return -1; });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.renameTempFileBinaryToProperName("src", "dst"));
    EXPECT_EQ(NEO::SysCalls::unlinkCalled, 1);
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenRenameThenTrueIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRename)> renameBackup(&NEO::SysCalls::sysCallsRename, [](const char *currName, const char *dstName) -> int { return 0; });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.renameTempFileBinaryToProperName("src", "dst"));
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenConfigFileIsInacessibleThenFdIsSetToNegativeNumber) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int { errno = EACCES;  return -1; });

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), -1);
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenCannotLockConfigFileThenFdIsSetToNegativeNumber) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});
    int flockRetVal = -1;

    VariableBackup<decltype(NEO::SysCalls::flockRetVal)> flockBackup(&NEO::SysCalls::flockRetVal, flockRetVal);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), -1);
}

#include <fcntl.h>

namespace LockConfigFileAndReadSize {
int openWithMode(const char *file, int flags, int mode) {

    if (flags == (O_CREAT | O_EXCL | O_RDWR)) {
        return 1;
    }

    errno = 2;
    return -1;
}

int open(const char *file, int flags) {
    errno = 2;
    return -1;
}
} // namespace LockConfigFileAndReadSize

TEST(CompilerCacheTests, GivenCompilerCacheWhenScandirFailInLockConfigFileThenFdIsSetToNegativeNumber) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    VariableBackup<decltype(NEO::SysCalls::sysCallsScandir)> scandirBackup(&NEO::SysCalls::sysCallsScandir, [](const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **)) -> int { return -1; });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpenWithMode)> openWithModeBackup(&NEO::SysCalls::sysCallsOpenWithMode, LockConfigFileAndReadSize::openWithMode);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, LockConfigFileAndReadSize::open);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), -1);
}

namespace lockConfigFileAndReadSizeMocks {
decltype(NEO::SysCalls::sysCallsScandir) mockScandir = [](const char *dirp,
                                                          struct dirent ***namelist,
                                                          int (*filter)(const struct dirent *),
                                                          int (*compar)(const struct dirent **,
                                                                        const struct dirent **)) -> int {
    struct dirent **v = (struct dirent **)malloc(4 * (sizeof(struct dirent *)));

    v[0] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[0]->d_name, sizeof(v[0]->d_name), "file1.cl_cache", sizeof("file1.cl_cache"));

    v[1] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[1]->d_name, sizeof(v[1]->d_name), "file2.cl_cache", sizeof("file2.cl_cache"));

    v[2] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[2]->d_name, sizeof(v[2]->d_name), "file3.cl_cache", sizeof("file3.cl_cache"));

    v[3] = (struct dirent *)malloc(sizeof(struct dirent));

    memcpy_s(v[3]->d_name, sizeof(v[3]->d_name), "file4.cl_cache", sizeof("file4.cl_cache"));

    *namelist = v;
    return 4;
};

decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
    if (filePath.find("file1") != filePath.npos) {
        statbuf->st_atime = 3;
    }
    if (filePath.find("file2") != filePath.npos) {
        statbuf->st_atime = 6;
    }
    if (filePath.find("file3") != filePath.npos) {
        statbuf->st_atime = 1;
    }
    if (filePath.find("file4") != filePath.npos) {
        statbuf->st_atime = 2;
    }

    statbuf->st_size = (MemoryConstants::megaByte / 4);

    return 0;
};
} // namespace lockConfigFileAndReadSizeMocks

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileWithFileCreationThenFdIsSetProperSizeIsSetAndScandirIsCalled) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});
    int scandirCalledTemp = 0;

    VariableBackup<decltype(NEO::SysCalls::scandirCalled)> scandirCalledBackup(&NEO::SysCalls::scandirCalled, scandirCalledTemp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsScandir)> scandirBackup(&NEO::SysCalls::sysCallsScandir, lockConfigFileAndReadSizeMocks::mockScandir);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, lockConfigFileAndReadSizeMocks::mockStat);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpenWithMode)> openWithModeBackup(&NEO::SysCalls::sysCallsOpenWithMode, LockConfigFileAndReadSize::openWithMode);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, LockConfigFileAndReadSize::open);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), 1);
    EXPECT_EQ(NEO::SysCalls::scandirCalled, 1);
    EXPECT_EQ(directory, MemoryConstants::megaByte);
}

namespace LockConfigFileAndConfigFileIsCreatedInMeantime {
int openCalledTimes = 0;
int openWithMode(const char *file, int flags, int mode) {

    if (flags == (O_CREAT | O_EXCL | O_RDWR)) {
        return -1;
    }

    return 0;
}

int open(const char *file, int flags) {
    if (openCalledTimes == 0) {
        openCalledTimes++;
        errno = 2;
        return -1;
    }

    return 1;
}

size_t configSize = MemoryConstants::megaByte;
ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    memcpy(buf, &configSize, sizeof(configSize));
    return 0;
}
} // namespace LockConfigFileAndConfigFileIsCreatedInMeantime

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileAndOtherProcessCreateInMeanTimeThenFdIsSetProperSizeIsSetAndScandirIsNotCalled) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});
    int openCalledTimes = 0;
    int scandirCalledTimes = 0;

    VariableBackup<decltype(NEO::SysCalls::scandirCalled)> scandirCalledBackup(&NEO::SysCalls::scandirCalled, scandirCalledTimes);
    VariableBackup<decltype(LockConfigFileAndConfigFileIsCreatedInMeantime::openCalledTimes)> openCalledBackup(&LockConfigFileAndConfigFileIsCreatedInMeantime::openCalledTimes, openCalledTimes);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, LockConfigFileAndConfigFileIsCreatedInMeantime::pread);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpenWithMode)> openWithModeBackup(&NEO::SysCalls::sysCallsOpenWithMode, LockConfigFileAndConfigFileIsCreatedInMeantime::openWithMode);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, LockConfigFileAndConfigFileIsCreatedInMeantime::open);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), 1);
    EXPECT_EQ(NEO::SysCalls::scandirCalled, 0);
    EXPECT_EQ(directory, LockConfigFileAndConfigFileIsCreatedInMeantime::configSize);
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileThenFdIsSetProperSizeIsSetAndScandirIsNotCalled) {
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});
    int scandirCalledTimes = 0;

    VariableBackup<decltype(NEO::SysCalls::scandirCalled)> scandirCalledBackup(&NEO::SysCalls::scandirCalled, scandirCalledTimes);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, LockConfigFileAndConfigFileIsCreatedInMeantime::pread);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), NEO::SysCalls::fakeFileDescriptor);
    EXPECT_EQ(NEO::SysCalls::scandirCalled, 0);
    EXPECT_EQ(directory, LockConfigFileAndConfigFileIsCreatedInMeantime::configSize);
}

class CompilerCacheFailingLokcConfigFileAndReadSizeLinux : public CompilerCache {
  public:
    CompilerCacheFailingLokcConfigFileAndReadSizeLinux(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        std::get<int>(fd) = -1;
        return;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileFailThenCacheBinaryReturnsFalse) {
    CompilerCacheFailingLokcConfigFileAndReadSizeLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnTrueIfAnotherProcessCreateCacheFile : public CompilerCache {
  public:
    CompilerCacheLinuxReturnTrueIfAnotherProcessCreateCacheFile(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        std::get<int>(fd) = 1;
        return;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenFileAlreadyExistsThenCacheBinaryReturnsTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return 0; });

    CompilerCacheLinuxReturnTrueIfAnotherProcessCreateCacheFile cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnFalseOnCacheBinaryIfEvictFailed : public CompilerCache {
  public:
    CompilerCacheLinuxReturnFalseOnCacheBinaryIfEvictFailed(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        directorySize = MemoryConstants::megaByte;
        std::get<int>(fd) = 1;
        return;
    }

    bool evictCache(uint64_t &bytesEvicted) override {
        return false;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenEvictCacheFailThenCacheBinaryReturnsFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    CompilerCacheLinuxReturnFalseOnCacheBinaryIfEvictFailed cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateUniqueFileFailed : public CompilerCache {
  public:
    CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateUniqueFileFailed(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        directorySize = MemoryConstants::megaByte;
        std::get<int>(fd) = 1;
        return;
    }

    bool evictCache(uint64_t &bytesEvicted) override {
        return true;
    }

    bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) override {
        return false;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenCreateUniqueFileFailThenCacheBinaryReturnsFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateUniqueFileFailed cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnFalseOnCacheBinaryIfRenameFileFailed : public CompilerCache {
  public:
    CompilerCacheLinuxReturnFalseOnCacheBinaryIfRenameFileFailed(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        directorySize = MemoryConstants::megaByte;
        std::get<int>(fd) = 1;
        return;
    }

    bool evictCache(uint64_t &bytesEvicted) override {
        return true;
    }

    bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) override {
        return true;
    }

    bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) override {
        return false;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenRenameFileFailThenCacheBinaryReturnsFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    CompilerCacheLinuxReturnFalseOnCacheBinaryIfRenameFileFailed cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnTrueOnCacheBinary : public CompilerCache {
  public:
    CompilerCacheLinuxReturnTrueOnCacheBinary(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        directorySize = MemoryConstants::megaByte;
        std::get<int>(fd) = 1;
        return;
    }

    bool evictCache(uint64_t &bytesEvicted) override {
        return true;
    }

    bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) override {
        return true;
    }

    bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) override {
        return true;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenAllFunctionsSuccedThenCacheBinaryReturnsTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return 0; });

    CompilerCacheLinuxReturnTrueOnCacheBinary cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.cacheBinary("config.file", "1", 1));
}

namespace NonExistingPathIsSet {
bool pathExistsMock(const std::string &path) {
    return false;
}
} // namespace NonExistingPathIsSet

TEST(CompilerCacheHelper, GivenNonExistingPathWhenCheckDefaultCacheDirSettingsThenFalseIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, NonExistingPathIsSet::pathExistsMock);

    std::string cacheDir = "";
    EXPECT_FALSE(checkDefaultCacheDirSettings(cacheDir, envReader));
}

namespace XDGEnvPathIsSet {
bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return true;
    }
    if (path.find("xdg/directory/") != path.npos) {
        return true;
    }
    return false;
}
} // namespace XDGEnvPathIsSet

TEST(CompilerCacheHelper, GivenXdgCachePathSetWhenCheckDefaultCacheDirSettingsThenProperPathIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XDGEnvPathIsSet::pathExistsMock);

    std::string cacheDir = "";

    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomeEnvPathIsSet {
bool pathExistsMock(const std::string &path) {
    if (path.find("home/directory/.cache/neo_compiler_cache") != path.npos) {
        return true;
    }
    if (path.find("home/directory/.cache/") != path.npos) {
        return true;
    }
    return false;
}
} // namespace HomeEnvPathIsSet

TEST(CompilerCacheHelper, GivenHomeCachePathSetWhenCheckDefaultCacheDirSettingsThenProperDirectoryIsSet) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomeEnvPathIsSet::pathExistsMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace XdgPathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("xdg/directory/") != path.npos) {
        return true;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    if (dir.find("xdg/directory/neo_compiler_cache") != dir.npos) {
        mkdirCalled = true;
        errno = EEXIST;
        return -1;
    }
    return 0;
}
} // namespace XdgPathIsSetAndOtherProcessCreatesPath

TEST(CompilerCacheHelper, GivenXdgEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperDirectoryIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";
    bool mkdirCalledTemp = false;

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndOtherProcessCreatesPath::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndOtherProcessCreatesPath::mkdirMock);
    VariableBackup<bool> mkdirCalledBackup(&XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled, mkdirCalledTemp);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
    EXPECT_TRUE(XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled);
}

namespace XdgPathIsSetAndNeedToCreate {
bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("xdg/directory") != path.npos) {
        return true;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace XdgPathIsSetAndNeedToCreate

TEST(CompilerCacheHelper, GivenXdgEnvWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(CompilerCacheHelper, GivenXdgEnvWithoutTrailingSlashWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomePathIsSetAndNeedToCreate {
bool pathExistsMock(const std::string &path) {
    if (path.find("home/directory/.cache/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("home/directory/.cache/") != path.npos) {
        return true;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace HomePathIsSetAndNeedToCreate

TEST(CompilerCacheHelper, GivenHomeCachePathSetWithoutTrailingSlashWhenCheckDefaultCacheDirSettingsThenProperDirectoryIsCreated) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["HOME"] = "home/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomePathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomePathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(CompilerCacheHelper, GivenHomeCachePathSetWhenCheckDefaultCacheDirSettingsThenProperDirectoryIsCreated) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomePathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomePathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace HomePathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

bool pathExistsMock(const std::string &path) {
    if (path.find("home/directory/.cache/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("home/directory/.cache/") != path.npos) {
        return true;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    if (dir.find("home/directory/.cache/neo_compiler_cache") != dir.npos) {
        mkdirCalled = true;
        errno = EEXIST;
        return -1;
    }
    return 0;
}
} // namespace HomePathIsSetAndOtherProcessCreatesPath

TEST(CompilerCacheHelper, GivenHomeEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperDirectoryIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["HOME"] = "home/directory/";
    bool mkdirCalledTemp = false;

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomePathIsSetAndOtherProcessCreatesPath::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomePathIsSetAndOtherProcessCreatesPath::mkdirMock);
    VariableBackup<bool> mkdirCalledBackup(&HomePathIsSetAndOtherProcessCreatesPath::mkdirCalled, mkdirCalledTemp);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
    EXPECT_TRUE(HomePathIsSetAndOtherProcessCreatesPath::mkdirCalled);
}

TEST(CompilerCacheHelper, GivenExistingPathWhenGettingFileModificationTimeThenModificationTimeIsReturned) {
    decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
        if (filePath.find("file1") != filePath.npos) {
            statbuf->st_mtime = 3;
        }
        return 0;
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, mockStat);

    EXPECT_EQ(getFileModificationTime("/tmp/file1"), 3);
}

TEST(CompilerCacheHelper, GivenNotExistingPathWhenGettingFileModificationTimeThenZeroIsReturned) {
    decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
        return -1;
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, mockStat);

    EXPECT_EQ(getFileModificationTime("/tmp/file1"), 0);
}

TEST(CompilerCacheHelper, GivenExistingPathWhenGettingFileSizeThenSizeIsReturned) {
    decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
        if (filePath.find("file1") != filePath.npos) {
            statbuf->st_size = 20;
        }
        return 0;
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, mockStat);

    EXPECT_EQ(getFileSize("/tmp/file1"), 20u);
}

TEST(CompilerCacheHelper, GivenNotExistingPathWhenGettingFileSizeThenZeroIsReturned) {
    decltype(NEO::SysCalls::sysCallsStat) mockStat = [](const std::string &filePath, struct stat *statbuf) -> int {
        return -1;
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, mockStat);

    EXPECT_EQ(getFileSize("/tmp/file1"), 0u);
}
