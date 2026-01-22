/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "elements_struct.h"

using namespace NEO;

class CompilerCacheMockLinux : public CompilerCache {
  public:
    CompilerCacheMockLinux(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createCacheDirectories;
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::getCachedFiles;
    using CompilerCache::getFiles;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenCreateCacheDirectoriesIsCalledThenReturnsTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, [](const std::string &dir) -> int {
        return 0;
    });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.createCacheDirectories("file.cl_cache"));
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenDirectoryAlreadyExistsThenCreateCacheDirectoriesReturnsTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, [](const std::string &dir) -> int {
        errno = EEXIST;
        return -1;
    });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.createCacheDirectories("file.cl_cache"));
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenMkdirFailsThenCreateCacheDirectoriesReturnsFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, [](const std::string &dir) -> int {
        errno = EACCES;
        return -1;
    });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createCacheDirectories("file.cl_cache"));
}

namespace GetCachedFilesPass {
size_t cachedFileIdx;
std::vector<ElementsStruct> *mockCachedFiles;
int currentDirLevel; // 0 = root, 1 = f, 2 = i
bool returnedSubdir;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        currentDirLevel = 0;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x1);
    }
    if (strcmp(path, "/home/cl_cache/f") == 0) {
        currentDirLevel = 1;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x2);
    }
    if (strcmp(path, "/home/cl_cache/f/i") == 0) {
        currentDirLevel = 2;
        cachedFileIdx = 0;
        return reinterpret_cast<DIR *>(0x3);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "f", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x2)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "i", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x3)) {
        if (cachedFileIdx >= mockCachedFiles->size()) {
            return nullptr;
        }
        const ElementsStruct &el = (*mockCachedFiles)[cachedFileIdx];
        std::string fname = el.path.substr(el.path.find_last_of('/') + 1);
        strncpy(entry.d_name, fname.c_str(), sizeof(entry.d_name) - 1);
        entry.d_type = DT_REG;
        cachedFileIdx++;
        return &entry;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1) ||
        dirp == reinterpret_cast<DIR *>(0x2) ||
        dirp == reinterpret_cast<DIR *>(0x3)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/f" || path == "/home/cl_cache/f/i") {
        buf->st_mode = S_IFDIR;
        return 0;
    }
    for (const auto &el : *mockCachedFiles) {
        if (el.path == path) {
            buf->st_mode = S_IFREG;
            buf->st_size = el.fileSize;
            buf->st_atime = el.lastAccessTime;
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetCachedFilesPass

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetCachedFilesIsCalledThenProperFilesAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetCachedFilesPass::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetCachedFilesPass::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetCachedFilesPass::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetCachedFilesPass::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> cachedFiles;
    GetCachedFilesPass::mockCachedFiles = new std::vector<ElementsStruct>{
        ElementsStruct{3, 100, "/home/cl_cache/f/i/file1.cl_cache"},
        ElementsStruct{6, 200, "/home/cl_cache/f/i/file2.cl_cache"},
        ElementsStruct{1, 300, "/home/cl_cache/f/i/file3.cl_cache"},
        ElementsStruct{2, 150, "/home/cl_cache/f/i/file4.txt"}};

    EXPECT_TRUE(cache.getCachedFiles(cachedFiles));
    EXPECT_EQ(cachedFiles.size(), 3u);
    EXPECT_EQ(cachedFiles[0].path, "/home/cl_cache/f/i/file3.cl_cache");
    EXPECT_EQ(cachedFiles[1].path, "/home/cl_cache/f/i/file1.cl_cache");
    EXPECT_EQ(cachedFiles[2].path, "/home/cl_cache/f/i/file2.cl_cache");

    delete GetCachedFilesPass::mockCachedFiles;
}

namespace EvictCachePass {
std::vector<std::string> *unlinkFiles;

decltype(NEO::SysCalls::sysCallsUnlink) mockUnlink = [](const std::string &pathname) -> int {
    unlinkFiles->push_back(pathname);

    return 0;
};
} // namespace EvictCachePass

class CompilerCacheMockGetCachedFilesLinux : public CompilerCacheMockLinux {
  public:
    using CompilerCache::getFiles;
    using CompilerCacheMockLinux::compareByLastAccessTime;
    using CompilerCacheMockLinux::CompilerCacheMockLinux;
    using CompilerCacheMockLinux::getCachedFiles;

    bool getFiles(const std::string &startPath, const std::function<bool(const std::string_view &)> &filter, std::vector<ElementsStruct> &foundFiles) override {
        foundFiles = {
            ElementsStruct{3, (MemoryConstants::megaByte / 6) + 10, "file1.cl_cache"},
            ElementsStruct{6, (MemoryConstants::megaByte / 6) + 10, "file2.cl_cache"},
            ElementsStruct{1, (MemoryConstants::megaByte / 6) + 10, "file3.cl_cache"},
            ElementsStruct{2, (MemoryConstants::megaByte / 6) + 10, "file4.cl_cache"},
            ElementsStruct{4, (MemoryConstants::megaByte / 6) + 10, "file5.cl_cache"},
            ElementsStruct{5, (MemoryConstants::megaByte / 6) + 10, "file6.cl_cache"}};

        return true;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWithOneMegabyteWhenEvictCacheIsCalledThenUnlinkTwoOldestFiles) {
    std::vector<std::string> unlinkLocalFiles;
    EvictCachePass::unlinkFiles = &unlinkLocalFiles;

    VariableBackup<decltype(NEO::SysCalls::sysCallsUnlink)> unlinkBackup(&NEO::SysCalls::sysCallsUnlink, EvictCachePass::mockUnlink);

    CompilerCacheMockGetCachedFilesLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte - 2u});

    uint64_t bytesEvicted{0u};
    EXPECT_TRUE(cache.evictCache(bytesEvicted));

    EXPECT_EQ(unlinkLocalFiles.size(), 2u);

    auto file0 = unlinkLocalFiles[0];
    auto file1 = unlinkLocalFiles[1];
    EXPECT_NE(unlinkLocalFiles[0].find("file3"), file0.npos);
    EXPECT_NE(unlinkLocalFiles[1].find("file4"), file1.npos);
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRename)> renameBackup(&NEO::SysCalls::sysCallsRename, [](const char *currName, const char *dstName) -> int { return -1; });

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.renameTempFileBinaryToProperName("src", "dst"));
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

    errno = ENOENT;
    return -1;
}

int open(const char *file, int flags) {
    errno = ENOENT;
    return -1;
}
} // namespace LockConfigFileAndReadSize

class CompilerCacheMockGetCachedFilesFailsLinux : public CompilerCacheMockLinux {
  public:
    using CompilerCache::getFiles;
    using CompilerCacheMockLinux::CompilerCacheMockLinux;
    using CompilerCacheMockLinux::getCachedFiles;

    bool getFiles(const std::string &startPath, const std::function<bool(const std::string_view &)> &filter, std::vector<ElementsStruct> &foundFiles) override {
        return false;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetCachedFilesFailsInLockConfigFileThenFdIsSetToNegativeNumber) {
    CompilerCacheMockGetCachedFilesFailsLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpenWithMode)> openWithModeBackup(&NEO::SysCalls::sysCallsOpenWithMode, LockConfigFileAndReadSize::openWithMode);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, LockConfigFileAndReadSize::open);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), -1);
}

class CompilerCacheMockGetCachedFilesWorksLinux : public CompilerCacheMockLinux {
  public:
    using CompilerCache::getFiles;
    using CompilerCacheMockLinux::CompilerCacheMockLinux;
    using CompilerCacheMockLinux::getCachedFiles;

    bool getFiles(const std::string &startPath, const std::function<bool(const std::string_view &)> &filter, std::vector<ElementsStruct> &foundFiles) override {
        getFilesCalled++;

        foundFiles = {
            ElementsStruct{3, MemoryConstants::megaByte / 4, "file1" + config.cacheFileExtension},
            ElementsStruct{6, MemoryConstants::megaByte / 4, "file2" + config.cacheFileExtension},
            ElementsStruct{1, MemoryConstants::megaByte / 4, "file3" + config.cacheFileExtension},
            ElementsStruct{2, MemoryConstants::megaByte / 4, "file4" + config.cacheFileExtension}};

        return true;
    }

    int getFilesCalled = 0;
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetFilesWorksAndClCacheThenGetCachedFilesReturnsTrue) {
    CompilerCacheMockGetCachedFilesWorksLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> cachedFiles;
    EXPECT_TRUE(cache.getCachedFiles(cachedFiles));
    EXPECT_EQ(cachedFiles.size(), 4u);
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetFilesWorksAndL0CacheThenGetCachedFilesReturnsTrue) {
    CompilerCacheMockGetCachedFilesWorksLinux cache({true, ".l0_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> cachedFiles;
    EXPECT_TRUE(cache.getCachedFiles(cachedFiles));
    EXPECT_EQ(cachedFiles.size(), 4u);
}

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileWithFileCreationThenFdIsSetAndProperSizeIsSet) {
    CompilerCacheMockGetCachedFilesWorksLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpenWithMode)> openWithModeBackup(&NEO::SysCalls::sysCallsOpenWithMode, LockConfigFileAndReadSize::openWithMode);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, LockConfigFileAndReadSize::open);

    UnifiedHandle configFileDescriptor{0};
    size_t directory = 0;

    cache.lockConfigFileAndReadSize("config.file", configFileDescriptor, directory);

    EXPECT_EQ(std::get<int>(configFileDescriptor), 1);
    EXPECT_EQ(cache.getFilesCalled, 1);
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

TEST(CompilerCacheTests, GivenCacheBinaryWhenBinarySizeIsOverCacheLimitThenEarlyReturnFalse) {
    const size_t cacheSize = MemoryConstants::megaByte;
    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", cacheSize});

    auto result = cache.cacheBinary("fileHash", "123456", cacheSize * 2);
    EXPECT_FALSE(result);
}

namespace PWriteCallsCountedAndDirSizeWritten {
size_t pWriteCalled = 0u;
size_t dirSize = 0u;
decltype(NEO::SysCalls::sysCallsPwrite) mockPwrite = [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
    pWriteCalled++;
    memcpy(&dirSize, buf, sizeof(dirSize));
    return 0;
};
} // namespace PWriteCallsCountedAndDirSizeWritten

class CompilerCacheEvictionTestsMockLinux : public CompilerCache {
  public:
    CompilerCacheEvictionTestsMockLinux(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createCacheDirectories;
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

    bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) override {
        createUniqueTempFileAndWriteDataCalled++;
        return createUniqueTempFileAndWriteDataResult;
    }
    size_t createUniqueTempFileAndWriteDataCalled = 0u;
    bool createUniqueTempFileAndWriteDataResult = true;

    bool createCacheDirectories(const std::string &cacheFileName) override {
        createCacheDirectoriesCalled++;
        return createCacheDirectoriesResult;
    }
    size_t createCacheDirectoriesCalled = 0u;
    bool createCacheDirectoriesResult = true;

    bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) override {
        renameTempFileBinaryToProperNameCalled++;
        return renameTempFileBinaryToProperNameResult;
    }
    size_t renameTempFileBinaryToProperNameCalled = 0u;
    bool renameTempFileBinaryToProperNameResult = true;

    bool evictCache(uint64_t &bytesEvicted) override {
        bytesEvicted = evictCacheBytesEvicted;
        return evictCacheResult;
    }
    uint64_t evictCacheBytesEvicted = 0u;
    bool evictCacheResult = true;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        lockConfigFileAndReadSizeCalled++;
        std::get<int>(fd) = lockConfigFileAndReadSizeFd;
        directorySize = lockConfigFileAndReadSizeDirSize;
        return;
    }
    size_t lockConfigFileAndReadSizeCalled = 0u;
    int lockConfigFileAndReadSizeFd = -1;
    size_t lockConfigFileAndReadSizeDirSize = 0u;
};

TEST(CompilerCacheTests, GivenCacheDirectoryFilledToTheLimitWhenNewBinaryFitsAfterEvictionThenWriteCacheAndUpdateConfigAndReturnTrue) {
    const size_t cacheSize = 10;
    CompilerCacheEvictionTestsMockLinux cache({true, ".cl_cache", "/home/cl_cache/", cacheSize});

    cache.lockConfigFileAndReadSizeFd = 1;
    cache.lockConfigFileAndReadSizeDirSize = 6;

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = cacheSize / 3;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pWriteBackup(&NEO::SysCalls::sysCallsPwrite, PWriteCallsCountedAndDirSizeWritten::mockPwrite);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::pWriteCalled)> pWriteCalledBackup(&PWriteCallsCountedAndDirSizeWritten::pWriteCalled, 0);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::dirSize)> dirSizeBackup(&PWriteCallsCountedAndDirSizeWritten::dirSize, 0);

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    const size_t expectedDirectorySize = 6 - (cacheSize / 3) + binarySize;

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, PWriteCallsCountedAndDirSizeWritten::pWriteCalled);

    EXPECT_EQ(expectedDirectorySize, PWriteCallsCountedAndDirSizeWritten::dirSize);
}

TEST(CompilerCacheTests, GivenCacheBinaryWhenBinaryDoesntFitAfterEvictionThenWriteToConfigAndReturnFalse) {
    const size_t cacheSize = 10;
    CompilerCacheEvictionTestsMockLinux cache({true, ".cl_cache", "/home/cl_cache/", cacheSize});
    cache.lockConfigFileAndReadSizeFd = 1;
    cache.lockConfigFileAndReadSizeDirSize = 9;

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = cacheSize / 3;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pWriteBackup(&NEO::SysCalls::sysCallsPwrite, PWriteCallsCountedAndDirSizeWritten::mockPwrite);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::pWriteCalled)> pWriteCalledBackup(&PWriteCallsCountedAndDirSizeWritten::pWriteCalled, 0);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::dirSize)> dirSizeBackup(&PWriteCallsCountedAndDirSizeWritten::dirSize, 0);

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    const size_t expectedDirectorySize = 9 - (cacheSize / 3);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, PWriteCallsCountedAndDirSizeWritten::pWriteCalled);

    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);

    EXPECT_EQ(expectedDirectorySize, PWriteCallsCountedAndDirSizeWritten::dirSize);
}

TEST(CompilerCacheTests, GivenCacheDirectoryFilledToTheLimitWhenNoBytesHaveBeenEvictedAndNewBinaryDoesntFitAfterEvictionThenDontWriteToConfigAndReturnFalse) {
    const size_t cacheSize = 10;
    CompilerCacheEvictionTestsMockLinux cache({true, ".cl_cache", "/home/cl_cache/", cacheSize});
    cache.lockConfigFileAndReadSizeFd = 1;
    cache.lockConfigFileAndReadSizeDirSize = 9;

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = 0;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pWriteBackup(&NEO::SysCalls::sysCallsPwrite, PWriteCallsCountedAndDirSizeWritten::mockPwrite);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::pWriteCalled)> pWriteCalledBackup(&PWriteCallsCountedAndDirSizeWritten::pWriteCalled, 0);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::dirSize)> dirSizeBackup(&PWriteCallsCountedAndDirSizeWritten::dirSize, 0);

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);

    EXPECT_EQ(0u, PWriteCallsCountedAndDirSizeWritten::pWriteCalled);
    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
}

TEST(CompilerCacheTests, GivenCacheBinaryWhenEvictCacheFailsThenReturnFalse) {
    const size_t cacheSize = 10;
    CompilerCacheEvictionTestsMockLinux cache({true, ".cl_cache", "/home/cl_cache/", cacheSize});
    cache.lockConfigFileAndReadSizeFd = 1;
    cache.lockConfigFileAndReadSizeDirSize = 5;

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    cache.evictCacheResult = false;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pWriteBackup(&NEO::SysCalls::sysCallsPwrite, PWriteCallsCountedAndDirSizeWritten::mockPwrite);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::pWriteCalled)> pWriteCalledBackup(&PWriteCallsCountedAndDirSizeWritten::pWriteCalled, 0);
    VariableBackup<decltype(PWriteCallsCountedAndDirSizeWritten::dirSize)> dirSizeBackup(&PWriteCallsCountedAndDirSizeWritten::dirSize, 0);

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);

    EXPECT_EQ(0u, PWriteCallsCountedAndDirSizeWritten::pWriteCalled);
    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
}

class CompilerCacheFailingLockConfigFileAndReadSizeLinux : public CompilerCache {
  public:
    CompilerCacheFailingLockConfigFileAndReadSizeLinux(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::lockConfigFileAndReadSize;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) override {
        std::get<int>(fd) = -1;
        return;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenLockConfigFileFailThenCacheBinaryReturnsFalse) {
    CompilerCacheFailingLockConfigFileAndReadSizeLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnTrueIfAnotherProcessCreateCacheFile : public CompilerCache {
  public:
    CompilerCacheLinuxReturnTrueIfAnotherProcessCreateCacheFile(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::lockConfigFileAndReadSize;

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
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;

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

class CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateCacheDirectoriesFailed : public CompilerCache {
  public:
    CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateCacheDirectoriesFailed(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createCacheDirectories;
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

    bool createCacheDirectories(const std::string &cacheFile) override {
        return false;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenCreateCacheDirectoriesFailsThenCacheBinaryReturnsFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return -1; });

    CompilerCacheLinuxReturnFalseOnCacheBinaryIfCreateCacheDirectoriesFailed cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.cacheBinary("config.file", "1", 1));
}

class CompilerCacheLinuxReturnTrueOnCacheBinary : public CompilerCache {
  public:
    CompilerCacheLinuxReturnTrueOnCacheBinary(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createCacheDirectories;
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

    bool createCacheDirectories(const std::string &cacheFile) override {
        return true;
    }
};

TEST(CompilerCacheTests, GivenCompilerCacheWhenAllFunctionsSuccedThenCacheBinaryReturnsTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int { return 0; });

    CompilerCacheLinuxReturnTrueOnCacheBinary cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.cacheBinary("config.file", "1", 1));
}

namespace NonExistingPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    return -1;
}
} // namespace NonExistingPathIsSet

TEST(CompilerCacheHelper, GivenNonExistingPathWhenCheckDefaultCacheDirSettingsThenFalseIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, NonExistingPathIsSet::statMock);

    std::string cacheDir = "";
    EXPECT_FALSE(checkDefaultCacheDirSettings(cacheDir, envReader));
}

namespace XDGEnvPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("xdg/directory/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}

} // namespace XDGEnvPathIsSet

TEST(CompilerCacheHelper, GivenXdgCachePathSetWhenCheckDefaultCacheDirSettingsThenProperPathIsReturned) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, XDGEnvPathIsSet::statMock);

    std::string cacheDir = "";

    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomeEnvPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("home/directory/.cache/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("home/directory/.cache/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}
} // namespace HomeEnvPathIsSet

TEST(CompilerCacheHelper, GivenHomeCachePathSetWhenCheckDefaultCacheDirSettingsThenProperDirectoryIsSet) {
    NEO::EnvironmentVariableReader envReader;

    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSet::statMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace XdgPathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        return -1;
    }

    if (filePath.find("xdg/directory/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndOtherProcessCreatesPath::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndOtherProcessCreatesPath::mkdirMock);
    VariableBackup<bool> mkdirCalledBackup(&XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled, mkdirCalledTemp);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
    EXPECT_TRUE(XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled);
}

namespace XdgPathIsSetAndNeedToCreate {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        return -1;
    }

    if (filePath.find("xdg/directory") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndNeedToCreate::statMock);
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndNeedToCreate::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomePathIsSetAndNeedToCreate {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("home/directory/.cache/neo_compiler_cache") != filePath.npos) {
        return -1;
    }

    if (filePath.find("home/directory/.cache/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, HomePathIsSetAndNeedToCreate::statMock);
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, HomePathIsSetAndNeedToCreate::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomePathIsSetAndNeedToCreate::mkdirMock);

    std::string cacheDir = "";
    EXPECT_TRUE(checkDefaultCacheDirSettings(cacheDir, envReader));
    EXPECT_EQ(cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace HomePathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("home/directory/.cache/neo_compiler_cache") != filePath.npos) {
        return -1;
    }

    if (filePath.find("home/directory/.cache/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statMockBackup(&NEO::SysCalls::sysCallsStat, HomePathIsSetAndOtherProcessCreatesPath::statMock);
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

namespace GetFilesWithFilter {
size_t cachedFileIdx;
std::vector<ElementsStruct> *mockCachedFiles;
bool returnedSubdir;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x1);
    }
    if (strcmp(path, "/home/cl_cache/f") == 0) {
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x2);
    }
    if (strcmp(path, "/home/cl_cache/f/i") == 0) {
        cachedFileIdx = 0;
        return reinterpret_cast<DIR *>(0x3);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "f", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x2)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "i", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x3)) {
        if (cachedFileIdx >= mockCachedFiles->size()) {
            return nullptr;
        }
        const ElementsStruct &el = (*mockCachedFiles)[cachedFileIdx];
        std::string fname = el.path.substr(el.path.find_last_of('/') + 1);
        strncpy(entry.d_name, fname.c_str(), sizeof(entry.d_name) - 1);
        entry.d_type = DT_REG;
        cachedFileIdx++;
        return &entry;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1) ||
        dirp == reinterpret_cast<DIR *>(0x2) ||
        dirp == reinterpret_cast<DIR *>(0x3)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/f" || path == "/home/cl_cache/f/i") {
        buf->st_mode = S_IFDIR;
        return 0;
    }
    for (const auto &el : *mockCachedFiles) {
        if (el.path == path) {
            buf->st_mode = S_IFREG;
            buf->st_size = el.fileSize;
            buf->st_atime = el.lastAccessTime;
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetFilesWithFilter

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetFilesIsCalledWithFilterThenOnlyMatchingFilesAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesWithFilter::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesWithFilter::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesWithFilter::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesWithFilter::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;
    GetFilesWithFilter::mockCachedFiles = new std::vector<ElementsStruct>{
        ElementsStruct{3, 100, "/home/cl_cache/f/i/file1.cl_cache"},
        ElementsStruct{6, 200, "/home/cl_cache/f/i/file2.cl_cache"},
        ElementsStruct{1, 300, "/home/cl_cache/f/i/file3.txt"}};

    auto filter = [](const std::string_view &path) -> bool {
        return path.ends_with(".cl_cache");
    };

    EXPECT_TRUE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
    EXPECT_EQ(foundFiles.size(), 2u);
    EXPECT_EQ(foundFiles[0].path, "/home/cl_cache/f/i/file1.cl_cache");
    EXPECT_EQ(foundFiles[1].path, "/home/cl_cache/f/i/file2.cl_cache");

    delete GetFilesWithFilter::mockCachedFiles;
}

namespace GetFilesReaddirFails {
size_t readdirCallCount;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        return reinterpret_cast<DIR *>(0x1);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (readdirCallCount == 0) {
            readdirCallCount++;
            strncpy(entry.d_name, "file1.cl_cache", sizeof(entry.d_name) - 1);
            entry.d_type = DT_REG;
            return &entry;
        } else {
            errno = EACCES;
            return nullptr;
        }
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/file1.cl_cache") {
        buf->st_mode = S_IFREG;
        buf->st_size = 100;
        buf->st_atime = 1;
        return 0;
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetFilesReaddirFails

TEST(CompilerCacheTests, GivenCompilerCacheWhenReaddirFailsWithErrorThenGetFilesReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesReaddirFails::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesReaddirFails::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesReaddirFails::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesReaddirFails::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;
    GetFilesReaddirFails::readdirCallCount = 0;

    auto filter = [](const std::string_view &path) -> bool {
        return true;
    };

    EXPECT_FALSE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
    EXPECT_EQ(foundFiles.size(), 1u);
    EXPECT_EQ(GetFilesReaddirFails::readdirCallCount, 1u);
}

namespace GetFilesMaxDepthExceeded {
int currentDirLevel;
bool returnedSubdir;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        currentDirLevel = 0;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x1);
    }
    if (strcmp(path, "/home/cl_cache/a") == 0) {
        currentDirLevel = 1;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x2);
    }
    if (strcmp(path, "/home/cl_cache/a/b") == 0) {
        currentDirLevel = 2;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x3);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "a", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x2)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "b", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x3)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "c", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1) ||
        dirp == reinterpret_cast<DIR *>(0x2) ||
        dirp == reinterpret_cast<DIR *>(0x3)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/a" || path == "/home/cl_cache/a/b" || path == "/home/cl_cache/a/b/c") {
        buf->st_mode = S_IFDIR;
        return 0;
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetFilesMaxDepthExceeded

TEST(CompilerCacheTests, GivenCompilerCacheWhenDirectoryDepthExceedsMaxCacheDepthThenDeeperDirectoriesAreNotTraversed) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesMaxDepthExceeded::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesMaxDepthExceeded::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesMaxDepthExceeded::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesMaxDepthExceeded::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;

    auto filter = [](const std::string_view &path) -> bool {
        return true;
    };

    EXPECT_TRUE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
    EXPECT_EQ(foundFiles.size(), 0u);
}

namespace GetFilesNonRegularFile {
int currentDirLevel;
size_t entryIndex;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        currentDirLevel = 0;
        entryIndex = 0;
        return reinterpret_cast<DIR *>(0x1);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (entryIndex == 0) {
            entryIndex++;
            strncpy(entry.d_name, "symlink.cl_cache", sizeof(entry.d_name) - 1);
            entry.d_type = DT_LNK;
            return &entry;
        } else if (entryIndex == 1) {
            entryIndex++;
            strncpy(entry.d_name, "regularfile.cl_cache", sizeof(entry.d_name) - 1);
            entry.d_type = DT_REG;
            return &entry;
        }
        return nullptr;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/symlink.cl_cache") {
        buf->st_mode = S_IFLNK;
        buf->st_size = 100;
        buf->st_atime = 1;
        return 0;
    }
    if (path == "/home/cl_cache/regularfile.cl_cache") {
        buf->st_mode = S_IFREG;
        buf->st_size = 200;
        buf->st_atime = 2;
        return 0;
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetFilesNonRegularFile

TEST(CompilerCacheTests, GivenCompilerCacheWhenNonRegularFilesAreEncounteredThenOnlyRegularFilesAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesNonRegularFile::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesNonRegularFile::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesNonRegularFile::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesNonRegularFile::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;

    auto filter = [](const std::string_view &path) -> bool {
        return true;
    };

    EXPECT_TRUE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
    EXPECT_EQ(foundFiles.size(), 1u);
    EXPECT_EQ(foundFiles[0].path, "/home/cl_cache/regularfile.cl_cache");
}

namespace GetCachedFilesMultipleExtensions {
size_t cachedFileIdx;
std::vector<ElementsStruct> *mockCachedFiles;
int currentDirLevel;
bool returnedSubdir;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        currentDirLevel = 0;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x1);
    }
    if (strcmp(path, "/home/cl_cache/f") == 0) {
        currentDirLevel = 1;
        returnedSubdir = false;
        return reinterpret_cast<DIR *>(0x2);
    }
    if (strcmp(path, "/home/cl_cache/f/i") == 0) {
        currentDirLevel = 2;
        cachedFileIdx = 0;
        return reinterpret_cast<DIR *>(0x3);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "f", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x2)) {
        if (!returnedSubdir) {
            returnedSubdir = true;
            strncpy(entry.d_name, "i", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        }
        return nullptr;
    }
    if (dirp == reinterpret_cast<DIR *>(0x3)) {
        if (cachedFileIdx >= mockCachedFiles->size()) {
            return nullptr;
        }
        const ElementsStruct &el = (*mockCachedFiles)[cachedFileIdx];
        std::string fname = el.path.substr(el.path.find_last_of('/') + 1);
        strncpy(entry.d_name, fname.c_str(), sizeof(entry.d_name) - 1);
        entry.d_type = DT_REG;
        cachedFileIdx++;
        return &entry;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1) ||
        dirp == reinterpret_cast<DIR *>(0x2) ||
        dirp == reinterpret_cast<DIR *>(0x3)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/f" || path == "/home/cl_cache/f/i") {
        buf->st_mode = S_IFDIR;
        return 0;
    }
    for (const auto &el : *mockCachedFiles) {
        if (el.path == path) {
            buf->st_mode = S_IFREG;
            buf->st_size = el.fileSize;
            buf->st_atime = el.lastAccessTime;
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetCachedFilesMultipleExtensions

TEST(CompilerCacheTests, GivenCompilerCacheWhenGetCachedFilesWithMultipleFileTypesThenOnlyCacheFilesAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetCachedFilesMultipleExtensions::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetCachedFilesMultipleExtensions::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetCachedFilesMultipleExtensions::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetCachedFilesMultipleExtensions::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> cachedFiles;
    GetCachedFilesMultipleExtensions::mockCachedFiles = new std::vector<ElementsStruct>{
        ElementsStruct{3, 100, "/home/cl_cache/f/i/file1.cl_cache"},
        ElementsStruct{6, 200, "/home/cl_cache/f/i/file2.l0_cache"},
        ElementsStruct{1, 300, "/home/cl_cache/f/i/file3.txt"}};

    EXPECT_TRUE(cache.getCachedFiles(cachedFiles));
    EXPECT_EQ(cachedFiles.size(), 2u);
    EXPECT_EQ(cachedFiles[0].path, "/home/cl_cache/f/i/file1.cl_cache");
    EXPECT_EQ(cachedFiles[1].path, "/home/cl_cache/f/i/file2.l0_cache");

    delete GetCachedFilesMultipleExtensions::mockCachedFiles;
}

namespace GetFilesSkipsDotDirectories {
size_t entryIndex;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        entryIndex = 0;
        return reinterpret_cast<DIR *>(0x1);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (entryIndex == 0) {
            entryIndex++;
            strncpy(entry.d_name, ".", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        } else if (entryIndex == 1) {
            entryIndex++;
            strncpy(entry.d_name, "..", sizeof(entry.d_name) - 1);
            entry.d_type = DT_DIR;
            return &entry;
        } else if (entryIndex == 2) {
            entryIndex++;
            strncpy(entry.d_name, "validfile.cl_cache", sizeof(entry.d_name) - 1);
            entry.d_type = DT_REG;
            return &entry;
        }
        return nullptr;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path == "/home/cl_cache/validfile.cl_cache") {
        buf->st_mode = S_IFREG;
        buf->st_size = 100;
        buf->st_atime = 1;
        return 0;
    }
    errno = ENOENT;
    return -1;
}
} // namespace GetFilesSkipsDotDirectories

TEST(CompilerCacheTests, GivenCompilerCacheWhenReaddirReturnsDotAndDotDotThenTheyAreSkipped) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesSkipsDotDirectories::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesSkipsDotDirectories::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesSkipsDotDirectories::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesSkipsDotDirectories::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;

    auto filter = [](const std::string_view &path) -> bool {
        return true;
    };

    EXPECT_TRUE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
    EXPECT_EQ(foundFiles.size(), 1u);
    EXPECT_EQ(foundFiles[0].path, "/home/cl_cache/validfile.cl_cache");
}

namespace GetFilesStatFails {
size_t entryIndex;

DIR *mockOpendir(const char *path) {
    if (strcmp(path, "/home/cl_cache/") == 0) {
        entryIndex = 0;
        return reinterpret_cast<DIR *>(0x1);
    }
    return nullptr;
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        if (entryIndex == 0) {
            entryIndex++;
            strncpy(entry.d_name, "somefile.cl_cache", sizeof(entry.d_name) - 1);
            entry.d_type = DT_REG;
            return &entry;
        }
        return nullptr;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    if (dirp == reinterpret_cast<DIR *>(0x1)) {
        return 0;
    }
    return -1;
}

int mockStat(const std::string &path, struct stat *buf) {
    errno = EACCES;
    return -1;
}
} // namespace GetFilesStatFails

TEST(CompilerCacheTests, GivenCompilerCacheWhenStatFailsThenGetFilesReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsOpendir)> opendirBackup(&SysCalls::sysCallsOpendir, GetFilesStatFails::mockOpendir);
    VariableBackup<decltype(SysCalls::sysCallsReaddir)> readdirBackup(&SysCalls::sysCallsReaddir, GetFilesStatFails::mockReaddir);
    VariableBackup<decltype(SysCalls::sysCallsClosedir)> closedirBackup(&SysCalls::sysCallsClosedir, GetFilesStatFails::mockClosedir);
    VariableBackup<decltype(SysCalls::sysCallsStat)> statBackup(&SysCalls::sysCallsStat, GetFilesStatFails::mockStat);

    CompilerCacheMockLinux cache({true, ".cl_cache", "/home/cl_cache/", MemoryConstants::megaByte});

    std::vector<ElementsStruct> foundFiles;

    auto filter = [](const std::string_view &path) -> bool {
        return true;
    };

    EXPECT_FALSE(cache.getFiles("/home/cl_cache/", filter, foundFiles));
}
