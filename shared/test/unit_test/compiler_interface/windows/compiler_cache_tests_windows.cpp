/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/test.h"

#include "elements_struct.h"

#include <cstring>
#include <string>

namespace NEO {

class CompilerCacheMockWindows : public CompilerCache {
  public:
    CompilerCacheMockWindows(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createCacheDirectories;
    using CompilerCache::createStats;
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::getCachedFilePath;
    using CompilerCache::getCachedFiles;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::maxCacheDepth;
    using CompilerCache::renameTempFileBinaryToProperName;
    using CompilerCache::updateAllStats;
    using CompilerCache::updateStats;

    bool createUniqueTempFileAndWriteData(char *tmpFilePath, const char *pBinary, size_t binarySize) override {
        createUniqueTempFileAndWriteDataCalled++;
        if (callBaseCreateUniqueTempFileAndWriteData) {
            return CompilerCache::createUniqueTempFileAndWriteData(tmpFilePath, pBinary, binarySize);
        }
        createUniqueTempFileAndWriteDataTmpFilePath = tmpFilePath;
        return createUniqueTempFileAndWriteDataResult;
    }

    bool callBaseCreateUniqueTempFileAndWriteData = true;
    size_t createUniqueTempFileAndWriteDataCalled = 0u;
    bool createUniqueTempFileAndWriteDataResult = true;
    std::string createUniqueTempFileAndWriteDataTmpFilePath = "";

    bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) override {
        renameTempFileBinaryToProperNameCalled++;
        if (callBaseRenameTempFileBinaryToProperName) {
            CompilerCache::renameTempFileBinaryToProperName(oldName, kernelFileHash);
        }
        renameTempFileBinaryToProperNameCacheFilePath = kernelFileHash;
        return renameTempFileBinaryToProperNameResult;
    }

    bool callBaseRenameTempFileBinaryToProperName = true;
    size_t renameTempFileBinaryToProperNameCalled = 0u;
    bool renameTempFileBinaryToProperNameResult = true;
    std::string renameTempFileBinaryToProperNameCacheFilePath = "";

    bool evictCache(uint64_t &bytesEvicted) override {
        evictCacheCalled++;
        if (callBaseEvictCache) {
            return CompilerCache::evictCache(bytesEvicted);
        }
        bytesEvicted = evictCacheBytesEvicted;
        return evictCacheResult;
    }

    bool callBaseEvictCache = true;
    size_t evictCacheCalled = 0u;
    bool evictCacheResult = true;
    uint64_t evictCacheBytesEvicted = 0u;

    void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &handle, size_t &directorySize) override {
        lockConfigFileAndReadSizeCalled++;
        if (callBaseLockConfigFileAndReadSize) {
            return CompilerCache::lockConfigFileAndReadSize(configFilePath, handle, directorySize);
        }
        std::get<void *>(handle) = lockConfigFileAndReadSizeHandle;
        directorySize = lockConfigFileAndReadSizeDirSize;
    }

    bool callBaseLockConfigFileAndReadSize = true;
    size_t lockConfigFileAndReadSizeCalled = 0u;
    void *lockConfigFileAndReadSizeHandle = INVALID_HANDLE_VALUE;
    size_t lockConfigFileAndReadSizeDirSize = 0u;

    bool createCacheDirectories(const std::string &cacheFile) override {
        createCacheDirectoriesCalled++;
        if (callBaseCreateCacheDirectories) {
            return CompilerCache::createCacheDirectories(cacheFile);
        }
        return createCacheDirectoriesResult;
    }

    bool callBaseCreateCacheDirectories = true;
    size_t createCacheDirectoriesCalled = 0u;
    bool createCacheDirectoriesResult = true;

    bool createStats(const std::string &configFile) override {
        createStatsCalled++;
        if (callBaseCreateStats) {
            return CompilerCache::createStats(configFile);
        }
        return createStatsResult;
    }

    bool callBaseCreateStats = true;
    size_t createStatsCalled = 0u;
    bool createStatsResult = true;

    bool updateStats(const std::string &statsPath, bool cacheHit) override {
        updateStatsCalled++;
        if (callBaseUpdateStats) {
            return CompilerCache::updateStats(statsPath, cacheHit);
        }
        updateStatsStatsPath = statsPath;
        updateStatsCacheHit = cacheHit;
        return updateStatsResult;
    }

    bool callBaseUpdateStats = true;
    size_t updateStatsCalled = 0u;
    bool updateStatsResult = true;
    std::string updateStatsStatsPath = "";
    bool updateStatsCacheHit = true;

    bool updateAllStats(const std::string &cacheFile, bool cacheHit) override {
        updateAllStatsCalled++;
        if (callBaseUpdateAllStats) {
            return CompilerCache::updateAllStats(cacheFile, cacheHit);
        }
        updateAllStatsCacheFile = cacheFile;
        updateAllStatsCacheHit = cacheHit;
        return updateAllStatsResult;
    }

    bool callBaseUpdateAllStats = true;
    size_t updateAllStatsCalled = 0u;
    bool updateAllStatsResult = true;
    std::string updateAllStatsCacheFile = "";
    bool updateAllStatsCacheHit = true;

    std::string getCachedFilePath(const std::string &cacheFile) override {
        getCachedFilePathCalled++;
        if (callBaseGetCachedFilePath) {
            return CompilerCache::getCachedFilePath(cacheFile);
        }
        getCachedFilePathCacheFile = cacheFile;
        return getCachedFilePathResult;
    }

    bool callBaseGetCachedFilePath = true;
    size_t getCachedFilePathCalled = 0u;
    std::string getCachedFilePathCacheFile = "";
    std::string getCachedFilePathResult = "somePath\\cl_cache\\f\\i\\file.cl_cache";
};

TEST(CompilerCacheHelper, GivenHomeEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperDirectoryIsReturned) {
    NEO::EnvironmentVariableReader envReader;
    std::string cacheDir = "";
    EXPECT_FALSE(checkDefaultCacheDirSettings(cacheDir, envReader));
}

namespace SysCalls {
extern size_t getLastErrorCalled;
extern const size_t getLastErrorResultCount;
extern DWORD getLastErrorResults[];
extern BOOL getLastErrorConstantResult;

extern size_t getTempFileNameACalled;
extern UINT getTempFileNameAResult;

extern BOOL moveFileExAResult;

extern size_t lockFileExCalled;
extern BOOL lockFileExResult;

extern size_t unlockFileExCalled;
extern BOOL unlockFileExResult;

extern size_t createFileACalled;
extern const size_t createFileAResultsCount;
extern HANDLE createFileAResults[];

extern size_t deleteFileACalled;
extern BOOL deleteFileAResult;
extern char deleteFiles[][256];

extern bool callBaseReadFile;
extern BOOL readFileResult;
extern size_t readFileCalled;
extern const size_t readFileBufferDataSize;
extern char readFileBufferData[];
extern DWORD readFileNumberOfBytesRead;

extern size_t writeFileCalled;
extern BOOL writeFileResult;
extern const size_t writeFileBufferSize;
extern char writeFileBuffer[];
extern DWORD writeFileNumberOfBytesWritten;

extern size_t findFirstFileACalled;
extern const size_t findFirstFileAResultsCount;
extern HANDLE findFirstFileAResults[];
extern WIN32_FIND_DATAA findFirstFileAFfd[];

extern size_t findNextFileACalled;
extern BOOL findNextFileAResult;
extern const size_t findNextFileAFileDataCount;
extern WIN32_FIND_DATAA findNextFileAFileData[];

extern size_t findCloseCalled;

extern size_t getFileAttributesCalled;
extern DWORD getFileAttributesResult;

extern size_t setFilePointerCalled;
extern DWORD setFilePointerResult;

extern size_t createDirectoryACalled;
extern BOOL createDirectoryAResult;
extern std::unordered_map<std::string, DWORD> pathAttributes;

extern size_t removeDirectoryACalled;
extern BOOL removeDirectoryAResult;
extern BOOL erasePathAttributes;
} // namespace SysCalls

struct CompilerCacheWindowsTest : public ::testing::Test {
    CompilerCacheWindowsTest()
        : getLastErrorCalledBackup(&SysCalls::getLastErrorCalled),
          getLastErrorConstantResult(&SysCalls::getLastErrorConstantResult),
          closeHandleCalledBackup(&SysCalls::closeHandleCalled),
          getTempFileNameACalled(&SysCalls::getTempFileNameACalled),
          getTempFileNameAResult(&SysCalls::getTempFileNameAResult),
          lockFileExCalledBackup(&SysCalls::lockFileExCalled),
          lockFileExResultBackup(&SysCalls::lockFileExResult),
          unlockFileExCalledBackup(&SysCalls::unlockFileExCalled),
          unlockFileExResultBackup(&SysCalls::unlockFileExResult),
          createFileACalledBackup(&SysCalls::createFileACalled),
          deleteFileACalledBackup(&SysCalls::deleteFileACalled),
          callBaseReadFileBackup(&SysCalls::callBaseReadFile),
          readFileResultBackup(&SysCalls::readFileResult),
          readFileCalledBackup(&SysCalls::readFileCalled),
          readFileNumberOfBytesReadBackup(&SysCalls::readFileNumberOfBytesRead),
          writeFileCalledBackup(&SysCalls::writeFileCalled),
          writeFileResultBackup(&SysCalls::writeFileResult),
          writeFileNumberOfBytesWrittenBackup(&SysCalls::writeFileNumberOfBytesWritten),
          findFirstFileACalledBackup(&SysCalls::findFirstFileACalled),
          findNextFileACalledBackup(&SysCalls::findNextFileACalled),
          getFileAttributesCalledBackup(&SysCalls::getFileAttributesCalled),
          getFileAttributesResultBackup(&SysCalls::getFileAttributesResult),
          setFilePointerCalledBackup(&SysCalls::setFilePointerCalled),
          setFilePointerResultBackup(&SysCalls::setFilePointerResult),
          createDirectoryACalledBackup(&SysCalls::createDirectoryACalled),
          createDirectoryAResultBackup(&SysCalls::createDirectoryAResult),
          removeDirectoryACalledBackup(&SysCalls::removeDirectoryACalled),
          removeDirectoryAResultBackup(&SysCalls::removeDirectoryAResult),
          erasePathAttributesBackup(&SysCalls::erasePathAttributes),
          findCloseCalledBackup(&SysCalls::findCloseCalled),
          findNextFileAResultBackup(&SysCalls::findNextFileAResult),
          moveFileExAResultBackup(&SysCalls::moveFileExAResult),
          deleteFileAResultBackup(&SysCalls::deleteFileAResult) {}

    void SetUp() override {
        SysCalls::closeHandleCalled = 0u;
        SysCalls::getTempFileNameACalled = 0u;
        SysCalls::lockFileExCalled = 0u;
        SysCalls::unlockFileExCalled = 0u;
        SysCalls::createFileACalled = 0u;
        SysCalls::deleteFileACalled = 0u;
        SysCalls::readFileCalled = 0u;
        SysCalls::readFileNumberOfBytesRead = 0u;
        SysCalls::writeFileCalled = 0u;
        SysCalls::findNextFileACalled = 0u;
        SysCalls::getFileAttributesCalled = 0u;
        SysCalls::setFilePointerCalled = 0u;
        SysCalls::createDirectoryACalled = 0u;
        SysCalls::removeDirectoryACalled = 0u;
        SysCalls::findCloseCalled = 0u;
        SysCalls::findFirstFileACalled = 0u;
        SysCalls::getLastErrorCalled = 0u;
        SysCalls::getLastErrorConstantResult = FALSE;
    }

    void TearDown() override {
        for (size_t i = 0; i < SysCalls::findNextFileAFileDataCount; i++) {
            memset(&SysCalls::findNextFileAFileData[i], 0, sizeof(SysCalls::findNextFileAFileData[i]));
        }
        for (size_t i = 0; i < SysCalls::deleteFileACalled; i++) {
            memset(SysCalls::deleteFiles[i], 0, sizeof(SysCalls::deleteFiles[0]));
        }
        for (size_t i = 0; i < SysCalls::createFileAResultsCount; i++) {
            SysCalls::createFileAResults[i] = nullptr;
        }
        for (size_t i = 0; i < SysCalls::getLastErrorResultCount; i++) {
            SysCalls::getLastErrorResults[i] = 0;
        }
        for (size_t i = 0; i < SysCalls::findFirstFileAResultsCount; i++) {
            SysCalls::findFirstFileAResults[i] = INVALID_HANDLE_VALUE;
            memset(&SysCalls::findFirstFileAFfd[i], 0, sizeof(WIN32_FIND_DATAA));
        }
        memset(SysCalls::writeFileBuffer, 0, SysCalls::writeFileBufferSize);
        memset(SysCalls::readFileBufferData, 0, SysCalls::readFileBufferDataSize);
        SysCalls::pathAttributes.clear();
    }

  protected:
    VariableBackup<size_t> getLastErrorCalledBackup;
    VariableBackup<BOOL> getLastErrorConstantResult;
    VariableBackup<size_t> closeHandleCalledBackup;
    VariableBackup<size_t> getTempFileNameACalled;
    VariableBackup<UINT> getTempFileNameAResult;
    VariableBackup<size_t> lockFileExCalledBackup;
    VariableBackup<BOOL> lockFileExResultBackup;
    VariableBackup<size_t> unlockFileExCalledBackup;
    VariableBackup<BOOL> unlockFileExResultBackup;
    VariableBackup<size_t> createFileACalledBackup;
    VariableBackup<size_t> deleteFileACalledBackup;
    VariableBackup<bool> callBaseReadFileBackup;
    VariableBackup<BOOL> readFileResultBackup;
    VariableBackup<size_t> readFileCalledBackup;
    VariableBackup<DWORD> readFileNumberOfBytesReadBackup;
    VariableBackup<size_t> writeFileCalledBackup;
    VariableBackup<BOOL> writeFileResultBackup;
    VariableBackup<DWORD> writeFileNumberOfBytesWrittenBackup;
    VariableBackup<size_t> findFirstFileACalledBackup;
    VariableBackup<size_t> findNextFileACalledBackup;
    VariableBackup<size_t> getFileAttributesCalledBackup;
    VariableBackup<DWORD> getFileAttributesResultBackup;
    VariableBackup<size_t> setFilePointerCalledBackup;
    VariableBackup<DWORD> setFilePointerResultBackup;
    VariableBackup<size_t> createDirectoryACalledBackup;
    VariableBackup<BOOL> createDirectoryAResultBackup;
    VariableBackup<size_t> removeDirectoryACalledBackup;
    VariableBackup<BOOL> removeDirectoryAResultBackup;
    VariableBackup<BOOL> erasePathAttributesBackup;
    VariableBackup<size_t> findCloseCalledBackup;
    VariableBackup<BOOL> findNextFileAResultBackup;
    VariableBackup<BOOL> moveFileExAResultBackup;
    VariableBackup<BOOL> deleteFileAResultBackup;
};

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateDirectoryAWorksThenCreateCacheDirectoriesReturnsTrue) {
    SysCalls::createDirectoryAResult = TRUE;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    cache.callBaseCreateStats = FALSE;

    EXPECT_TRUE(cache.createCacheDirectories("file.cl_cache"));
    EXPECT_EQ(3u, SysCalls::createDirectoryACalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateDirectoryAAlreadyExistsThenCreateCacheDirectoriesReturnsTrue) {
    SysCalls::createDirectoryAResult = FALSE;
    SysCalls::getLastErrorResults[0] = ERROR_ALREADY_EXISTS;
    SysCalls::getLastErrorResults[1] = ERROR_ALREADY_EXISTS;
    SysCalls::getLastErrorResults[2] = ERROR_ALREADY_EXISTS;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    cache.callBaseCreateStats = FALSE;

    EXPECT_TRUE(cache.createCacheDirectories("file.cl_cache"));
    EXPECT_EQ(3u, SysCalls::createDirectoryACalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateDirectoryAFailsThenCreateCacheDirectoriesReturnsFalse) {
    SysCalls::createDirectoryAResult = FALSE;
    SysCalls::getLastErrorResults[0] = ERROR_INVALID_FUNCTION;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createCacheDirectories("somePath\\cl_cache\\file.cl_cache"));
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenUpdateStatsFailsThenCreateCacheDirectoriesContinues) {
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    cache.callBaseCreateStats = FALSE;
    cache.createStatsResult = FALSE;

    EXPECT_TRUE(cache.createCacheDirectories("somePath\\cl_cache\\file.cl_cache"));
    EXPECT_EQ(3u, SysCalls::createDirectoryACalled);
    EXPECT_EQ(3u, cache.createStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenStatsAreDisabledThenCreateCacheDirectoriesDoesNotCreateStats) {
    SysCalls::createDirectoryAResult = TRUE;

    CompilerCacheMockWindows cache({true, false, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    cache.callBaseCreateStats = FALSE;

    EXPECT_TRUE(cache.createCacheDirectories("file.cl_cache"));
    EXPECT_EQ(3u, SysCalls::createDirectoryACalled);
    EXPECT_EQ(0u, cache.createStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateStatsThenReturnsTrue) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.createStats("somePath\\cl_cache\\stats"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateNewFailsWithFileExistsButOpenExistingSucceedsThenCreateStatsReturnsTrue) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::getLastErrorResults[0] = ERROR_FILE_EXISTS;
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));

    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(2u, SysCalls::createFileACalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateNewFailsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_ACCESS_DENIED;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::lockFileExCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateNewAndOpenExistingFailThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_FILE_EXISTS;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(2u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::lockFileExCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenLockFileExFailsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = FALSE;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(0u, SysCalls::readFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileReturnsSizeAndVersionMatchesThenCreateStatsReturnsTrue) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::readFileResult = TRUE;

    SysCalls::writeFileResult = FALSE;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileReturnsSizeAndVersionDoesntMatchAndWriteFileFailsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::writeFileResult = FALSE;
    SysCalls::writeFileNumberOfBytesWritten = 0u;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileReturnsSizeAndVersionDoesntMatchAndWriteFileDoesntWriteWholeStatsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = 0u;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileReturnsZeroAndWriteFileDoesntWriteWholeStatsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = 0;
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = 0u;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenSetFilePointerFailsThenCreateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::setFilePointerResult = INVALID_SET_FILE_POINTER;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.createStats("somePath\\cl_cache\\stats"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenStatsVersionMatchesThenUpdateStatsReturnsTrueAndAddsHitOrMiss) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion;
    readStats.hits = 3;
    readStats.misses = 1;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));

    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    auto writeStats = reinterpret_cast<CacheStats *>(SysCalls::writeFileBuffer);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.updateStats("somePath\\cl_cache\\stats", true));
    EXPECT_EQ(readStats.hits + 1, writeStats->hits);
    EXPECT_EQ(readStats.misses, writeStats->misses);
    EXPECT_EQ(readStats.version, writeStats->version);

    readStats.hits = 3;
    readStats.misses = 1;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));

    EXPECT_TRUE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(readStats.hits, writeStats->hits);
    EXPECT_EQ(readStats.misses + 1, writeStats->misses);
    EXPECT_EQ(readStats.version, writeStats->version);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenStatsVersionMatchesAndCreateFileAFailsWithFileNotFoundButCreateStatsWorksThenUpdateStatsReturnsTrueAndAddsHitOrMiss) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::createFileAResults[2] = reinterpret_cast<HANDLE>(0x5678);
    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;
    SysCalls::lockFileExResult = TRUE;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion;
    readStats.hits = 3;
    readStats.misses = 1;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));

    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    auto writeStats = reinterpret_cast<CacheStats *>(SysCalls::writeFileBuffer);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.updateStats("somePath\\cl_cache\\stats", true));
    EXPECT_EQ(readStats.hits + 1, writeStats->hits);
    EXPECT_EQ(readStats.misses, writeStats->misses);
    EXPECT_EQ(1u, cache.createStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenStatsVersionDoesntMatchThenUpdateStatsReturnsTrueAndResetsStatsAndAddsHitOrMiss) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    readStats.hits = 3;
    readStats.misses = 1;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));

    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    auto writeStats = reinterpret_cast<CacheStats *>(SysCalls::writeFileBuffer);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_TRUE(cache.updateStats("somePath\\cl_cache\\stats", true));
    EXPECT_EQ(1u, writeStats->hits);
    EXPECT_EQ(0u, writeStats->misses);
    EXPECT_EQ(CompilerCache::cacheVersion, writeStats->version);

    EXPECT_TRUE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(0u, writeStats->hits);
    EXPECT_EQ(1u, writeStats->misses);
    EXPECT_EQ(CompilerCache::cacheVersion, writeStats->version);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenSetFilePointerFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    CacheStats readStats{};
    readStats.version = CompilerCache::cacheVersion + 1;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readStats, sizeof(CacheStats));
    SysCalls::setFilePointerResult = INVALID_SET_FILE_POINTER;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateFileAFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_ACCESS_DENIED;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::lockFileExCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateFileAFailsWithFileNotFoundAndCreateStatsFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[1] = ERROR_ACCESS_DENIED;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(2u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::lockFileExCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenCreateFileAFailsWithFileNotFoundAndCreateStatsWorksButSecondOpenFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::createFileAResults[2] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;
    SysCalls::lockFileExResult = TRUE;
    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = sizeof(CacheStats);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(3u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenLockFileExFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = FALSE;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(0u, SysCalls::readFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileFailsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = FALSE;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenReadFileDoesntReadWholeStatsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = 0;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenWriteFileReturnsFalseThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::writeFileResult = FALSE;

    CacheStats readCacheStats{};
    readCacheStats.version = CompilerCache::cacheVersion;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readCacheStats, sizeof(CacheStats));

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenWriteFileDoesntWriteWholeStatsThenUpdateStatsReturnsFalse) {
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;
    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::writeFileResult = TRUE;
    SysCalls::writeFileNumberOfBytesWritten = 0u;

    CacheStats readCacheStats{};
    readCacheStats.version = CompilerCache::cacheVersion;
    SysCalls::readFileNumberOfBytesRead = sizeof(CacheStats);
    memcpy(SysCalls::readFileBufferData, &readCacheStats, sizeof(CacheStats));

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});

    EXPECT_FALSE(cache.updateStats("somePath\\cl_cache\\stats", false));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenClCacheThenGetCachedFilesReturnsTrue) {
    WIN32_FIND_DATAA filesData[4];

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (int i = 0; i < 2; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "dir%d", i);
        filesData[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    for (int i = 2; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%d.cl_cache", i);
        filesData[i].dwFileAttributes = 0;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    SysCalls::getLastErrorResults[1] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[2] = ERROR_FILE_NOT_FOUND;
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(5u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(2u, cacheFiles.size());
    EXPECT_EQ(0, strcmp(cacheFiles[0].path.c_str(), "somePath\\cl_cache\\file_3.cl_cache"));
    EXPECT_EQ(0, strcmp(cacheFiles[1].path.c_str(), "somePath\\cl_cache\\file_2.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenL0CacheThenGetCachedFilesReturnsTrue) {
    WIN32_FIND_DATAA filesData[4];

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (int i = 0; i < 2; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "dir%d", i);
        filesData[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    for (int i = 2; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%d.l0_cache", i);
        filesData[i].dwFileAttributes = 0;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    SysCalls::getLastErrorResults[1] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[2] = ERROR_FILE_NOT_FOUND;
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);

    CompilerCacheMockWindows cache({true, true, ".l0_cache", "somePath\\l0_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(5u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(2u, cacheFiles.size());
    EXPECT_EQ(0, strcmp(cacheFiles[0].path.c_str(), "somePath\\l0_cache\\file_3.l0_cache"));
    EXPECT_EQ(0, strcmp(cacheFiles[1].path.c_str(), "somePath\\l0_cache\\file_2.l0_cache"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenOclocCacheThenGetCachedFilesReturnsTrue) {
    WIN32_FIND_DATAA filesData[4];

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (int i = 0; i < 2; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "dir%d", i);
        filesData[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    for (int i = 2; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%d.ocloc_cache", i);
        filesData[i].dwFileAttributes = 0;
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = 100;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    SysCalls::getLastErrorResults[1] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[2] = ERROR_FILE_NOT_FOUND;
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);

    CompilerCacheMockWindows cache({true, true, ".ocloc_cache", "somePath\\ocloc_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(5u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(2u, cacheFiles.size());
    EXPECT_EQ(0, strcmp(cacheFiles[0].path.c_str(), "somePath\\ocloc_cache\\file_3.ocloc_cache"));
    EXPECT_EQ(0, strcmp(cacheFiles[1].path.c_str(), "somePath\\ocloc_cache\\file_2.ocloc_cache"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenFindFirstFileAFailsThenGetCachedFilesReturnsFalse) {
    SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_INVALID_FUNCTION;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_FALSE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(0u, SysCalls::findNextFileACalled);
    EXPECT_EQ(0u, SysCalls::findCloseCalled);
    EXPECT_EQ(0u, cacheFiles.size());
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenFindFirstFileAErrorFileNotFoundThenGetCachedFilesReturnsTrue) {
    SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(0u, SysCalls::findNextFileACalled);
    EXPECT_EQ(0u, SysCalls::findCloseCalled);
    EXPECT_EQ(0u, cacheFiles.size());
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenFindNextFileAErrorNoMoreFilesThenGetCachedFilesReturnsTrue) {
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::findNextFileAResult = FALSE;
    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(1u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(0u, cacheFiles.size());
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenFindNextFileAFailsThenGetCachedFilesReturnsFalse) {
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::getLastErrorResults[0] = ERROR_INVALID_FUNCTION;
    SysCalls::findNextFileAResult = FALSE;

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_FALSE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(1u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(0u, cacheFiles.size());
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWhenFileNameNotValidThenGetCachedFilesSkipsThisFile) {
    WIN32_FIND_DATAA filesData[4];
    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastWriteTime.dwLowDateTime = 2u;

    snprintf(filesData[0].cFileName, MAX_PATH, "file_0.cl_cache");
    filesData[0].dwFileAttributes = 0;
    filesData[0].nFileSizeHigh = 0;
    filesData[0].nFileSizeLow = 100;
    filesData[0].ftLastAccessTime.dwHighDateTime = 0;
    SysCalls::findNextFileAFileData[0] = filesData[0];

    snprintf(filesData[1].cFileName, MAX_PATH, "invalid_file_name.txt");
    filesData[1].dwFileAttributes = 0;
    filesData[1].nFileSizeHigh = 0;
    filesData[1].nFileSizeLow = 100;
    filesData[1].ftLastAccessTime.dwHighDateTime = 0;
    SysCalls::findNextFileAFileData[1] = filesData[1];

    snprintf(filesData[2].cFileName, MAX_PATH, ".");
    filesData[2].dwFileAttributes = 0;
    filesData[2].nFileSizeHigh = 0;
    filesData[2].nFileSizeLow = 100;
    filesData[2].ftLastAccessTime.dwHighDateTime = 0;
    SysCalls::findNextFileAFileData[2] = filesData[2];

    snprintf(filesData[3].cFileName, MAX_PATH, "..");
    filesData[3].dwFileAttributes = 0;
    filesData[3].nFileSizeHigh = 0;
    filesData[3].nFileSizeLow = 100;
    filesData[3].ftLastAccessTime.dwHighDateTime = 0;
    SysCalls::findNextFileAFileData[3] = filesData[3];

    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);

    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", MemoryConstants::megaByte});
    std::vector<ElementsStruct> cacheFiles;

    EXPECT_TRUE(cache.getCachedFiles(cacheFiles));
    EXPECT_EQ(5u, SysCalls::findNextFileACalled);
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
    EXPECT_EQ(1u, cacheFiles.size());
    EXPECT_EQ(0, strcmp(cacheFiles[0].path.c_str(), "somePath\\cl_cache\\file_0.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWithOneMegabyteWhenEvictCacheIsCalledThenDeleteTwoOldestFiles) {
    WIN32_FIND_DATAA filesData[4];
    DWORD cacheFileSize = (MemoryConstants::megaByte / 6) + 10;

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (size_t i = 0; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = 0;

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});
    auto &deletedFiles = SysCalls::deleteFiles;

    uint64_t bytesEvicted{0u};
    auto result = cache.evictCache(bytesEvicted);

    EXPECT_TRUE(result);
    EXPECT_EQ(2u, SysCalls::deleteFileACalled);
    EXPECT_EQ(0, strcmp(deletedFiles[0], "somePath\\cl_cache\\file_3.cl_cache"));
    EXPECT_EQ(0, strcmp(deletedFiles[1], "somePath\\cl_cache\\file_1.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, GivenCompilerCacheWithOneMegabyteAnd3CacheFilesAnd1DirectoryWhenEvictCacheIsCalledThenDeleteTwoOldestFilesSkippingDirectory) {
    WIN32_FIND_DATAA filesData[4];
    DWORD cacheFileSize = (MemoryConstants::megaByte / 6) + 10;

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u; // Directory
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    StackVec<DWORD, 4> fileAttributes = {0, FILE_ATTRIBUTE_DIRECTORY, 0, 0};

    for (size_t i = 0; i < 4; i++) {
        if (i != 1) {
            snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        } else {
            snprintf(filesData[i].cFileName, MAX_PATH, "someDir");
        }
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = fileAttributes[i];

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::findFirstFileAResults[1] = INVALID_HANDLE_VALUE;

    SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;
    SysCalls::getLastErrorResults[1] = ERROR_FILE_NOT_FOUND;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});
    auto &deletedFiles = SysCalls::deleteFiles;

    uint64_t bytesEvicted{0u};
    auto result = cache.evictCache(bytesEvicted);

    EXPECT_TRUE(result);
    EXPECT_EQ(2u, SysCalls::deleteFileACalled);
    EXPECT_EQ(0, strcmp(deletedFiles[0], "somePath\\cl_cache\\file_3.cl_cache"));
    EXPECT_EQ(0, strcmp(deletedFiles[1], "somePath\\cl_cache\\file_0.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, givenEvictCacheWhenFileSearchFailedThenDebugMessageWithErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    uint64_t bytesEvicted{0u};

    StreamCapture capture;
    capture.captureStderr();
    cache.evictCache(bytesEvicted);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: File search failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(0u, SysCalls::deleteFileACalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigThenConfigFileIsLockedAndRead) {
    const size_t readCacheDirSize = 840 * MemoryConstants::kiloByte;
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(size_t);
    memcpy(SysCalls::readFileBufferData, &readCacheDirSize, sizeof(size_t));

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);

    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);

    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(readCacheDirSize, directorySize);

    EXPECT_EQ(0u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigWhenLockFailsThenErrorIsPrintedAndFunctionExits) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = FALSE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Lock config file failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);

    EXPECT_EQ(0u, SysCalls::readFileCalled);

    EXPECT_EQ(0u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigWhenSetFilePointerFailsThenErrorIsPrintedAndFileIsUnlockedAndClosed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::setFilePointerResult = 8;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: File pointer move failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(INVALID_HANDLE_VALUE, std::get<void *>(configFileHandle));
    EXPECT_EQ(0u, directorySize);

    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(1u, SysCalls::setFilePointerCalled);

    EXPECT_EQ(0u, SysCalls::readFileCalled);

    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigWhenReadFileFailsThenErrorIsPrintedAndFileIsUnlockedAndClosed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = FALSE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Read config failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);

    EXPECT_EQ(1u, SysCalls::readFileCalled);

    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigFailsDueToFileNotFoundThenCreateNewConfigFileAndCountDirectorySize) {
    const size_t readCacheDirSize = 840 * MemoryConstants::kiloByte;
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(size_t);
    memcpy(SysCalls::readFileBufferData, &readCacheDirSize, sizeof(size_t));

    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);

    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[1] = ERROR_NO_MORE_FILES;

    WIN32_FIND_DATAA filesData[4];
    DWORD cacheFileSize = (MemoryConstants::megaByte / 6) + 10;

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (size_t i = 0; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = 0;

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }
    const size_t expectedDirectorySize = 4 * cacheFileSize;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);

    EXPECT_EQ(2u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);

    EXPECT_EQ(expectedDirectorySize, directorySize);

    EXPECT_EQ(0u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigFailsAndCreateNewConfigFailsAndSecondTimeOpenExistingConfigSucceedsThenPrintErrorMessageFromCreateConfigFail) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    const size_t readCacheDirSize = 840 * MemoryConstants::kiloByte;
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[2] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(size_t);
    memcpy(SysCalls::readFileBufferData, &readCacheDirSize, sizeof(size_t));

    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    WIN32_FIND_DATAA filesData[4];
    DWORD cacheFileSize = (MemoryConstants::megaByte / 6) + 10;

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (size_t i = 0; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = 0;

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Create config file failed! error code:");

    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(3u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);

    EXPECT_EQ(readCacheDirSize, directorySize);

    EXPECT_EQ(1u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenOpenExistingConfigFailsThenPrintErrorMessageAndEarlyReturn) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    const size_t readCacheDirSize = 840 * MemoryConstants::kiloByte;
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::callBaseReadFile = false;
    SysCalls::readFileResult = TRUE;
    SysCalls::readFileNumberOfBytesRead = sizeof(size_t);
    memcpy(SysCalls::readFileBufferData, &readCacheDirSize, sizeof(size_t));

    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND + 1;

    WIN32_FIND_DATAA filesData[4];
    DWORD cacheFileSize = (MemoryConstants::megaByte / 6) + 10;

    filesData[0].ftLastAccessTime.dwLowDateTime = 6u;
    filesData[1].ftLastAccessTime.dwLowDateTime = 4u;
    filesData[2].ftLastAccessTime.dwLowDateTime = 8u;
    filesData[3].ftLastAccessTime.dwLowDateTime = 2u;

    for (size_t i = 0; i < 4; i++) {
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = 0;

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Open config file failed! error code:");

    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(INVALID_HANDLE_VALUE, std::get<void *>(configFileHandle));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::lockFileExCalled);
    EXPECT_EQ(0u, SysCalls::readFileCalled);
    EXPECT_EQ(0u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, directorySize);
}

TEST_F(CompilerCacheWindowsTest, givenLockConfigFileAndReadSizeWhenGetCachedFilesFailsThenPrintErrorMessageAndEarlyReturn) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    SysCalls::createFileAResults[1] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::createFileAResults[2] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::lockFileExResult = TRUE;

    SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;

    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;
    SysCalls::getLastErrorResults[1] = 0;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    StreamCapture capture;
    capture.captureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Get cached files failed! error code:");

    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));

    EXPECT_EQ(INVALID_HANDLE_VALUE, std::get<void *>(configFileHandle));
    EXPECT_EQ(2u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::lockFileExCalled);
    EXPECT_EQ(0u, SysCalls::readFileCalled);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, directorySize);
}

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenCreateAndWriteTempFileSucceedsThenBinaryIsWritten) {
    SysCalls::getTempFileNameAResult = 6u;
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::writeFileResult = TRUE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(strlen(binary));
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));

    EXPECT_EQ(0, strcmp(SysCalls::writeFileBuffer, binary));
    EXPECT_EQ(1u, SysCalls::getTempFileNameACalled);
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenGetTempFileNameFailsThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getTempFileNameAResult = 0u;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    StreamCapture capture;
    capture.captureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Creating temporary file name failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(1u, SysCalls::getTempFileNameACalled);
    EXPECT_EQ(0u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenCreateFileFailsThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getTempFileNameAResult = 6u;
    SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    StreamCapture capture;
    capture.captureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Creating temporary file failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(1u, SysCalls::getTempFileNameACalled);
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenWriteFileFailsThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getTempFileNameAResult = 6u;
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::writeFileResult = FALSE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    StreamCapture capture;
    capture.captureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Writing to temporary file failed! error code:");
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(1u, SysCalls::getTempFileNameACalled);
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenWriteFileBytesWrittenMismatchesThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getTempFileNameAResult = 6u;
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::writeFileResult = TRUE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(strlen(binary)) - 1;
    StreamCapture capture;
    capture.captureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = capture.getCapturedStderr();

    std::stringstream expectedStderrSubstr;
    expectedStderrSubstr << "[Cache failure]: Writing to temporary file failed! Incorrect number of bytes written: ";
    expectedStderrSubstr << SysCalls::writeFileNumberOfBytesWritten << " vs " << strlen(binary);

    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr.str()));
    EXPECT_EQ(1u, SysCalls::getTempFileNameACalled);
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(CompilerCacheWindowsTest, givenEmptyBinaryAndOrBinarySizeWhenCacheBinaryThenEarlyReturnFalse) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const std::string kernelFileHash = "7e3291364d8df42";
    auto result = cache.cacheBinary(kernelFileHash, nullptr, 10);
    EXPECT_FALSE(result);

    result = cache.cacheBinary(kernelFileHash, "123456", 0);
    EXPECT_FALSE(result);

    result = cache.cacheBinary(kernelFileHash, nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenBinarySizeIsOverCacheLimitThenEarlyReturnFalse) {
    const size_t cacheSize = MemoryConstants::megaByte;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    auto result = cache.cacheBinary("7e3291364d8df42", "123456", cacheSize * 2);
    EXPECT_FALSE(result);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenAllStepsSuccessThenReturnTrue) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    cache.callBaseCreateCacheDirectories = false;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t));
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(0, strcmp(cache.createUniqueTempFileAndWriteDataTmpFilePath.c_str(), "somePath\\cl_cache\\cl_cache.XXXXXX"));
    EXPECT_EQ(0, strcmp(cache.renameTempFileBinaryToProperNameCacheFilePath.c_str(), "somePath\\cl_cache\\7\\e\\7e3291364d8df42.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenCacheAlreadyExistsThenDoNotCreateCacheAndReturnTrue) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES - 1;
    SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND + 1;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenWriteToConfigFileFailsThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;
    SysCalls::writeFileResult = FALSE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    cache.callBaseCreateCacheDirectories = false;
    cache.createCacheDirectoriesResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t));

    StreamCapture capture;
    capture.captureStderr();
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Writing to config file failed! error code:");

    EXPECT_TRUE(result);
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenWriteFileBytesWrittenMismatchesThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    cache.callBaseCreateCacheDirectories = false;
    cache.createCacheDirectoriesResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t)) - 1;

    StreamCapture capture;
    capture.captureStderr();
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);
    auto capturedStderr = capture.getCapturedStderr();

    std::stringstream expectedStderrSubstr;
    expectedStderrSubstr << "[Cache failure]: Writing to config file failed! Incorrect number of bytes written: ";
    expectedStderrSubstr << SysCalls::writeFileNumberOfBytesWritten << " vs " << static_cast<DWORD>(sizeof(size_t));

    EXPECT_TRUE(result);
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr.str()));
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenLockConfigFileAndReadSizeFailsThenEarlyReturnFalse) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = INVALID_HANDLE_VALUE;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
}

TEST_F(CompilerCacheWindowsTest, givenCacheDirectoryFilledToTheLimitWhenNewBinaryFitsAfterEvictionThenWriteCacheAndUpdateConfigAndReturnTrue) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);
    cache.lockConfigFileAndReadSizeDirSize = 6;

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = cacheSize / 3;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    cache.callBaseCreateCacheDirectories = false;
    cache.createCacheDirectoriesResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t));
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(0, strcmp(cache.createUniqueTempFileAndWriteDataTmpFilePath.c_str(), "somePath\\cl_cache\\cl_cache.XXXXXX"));
    EXPECT_EQ(0, strcmp(cache.renameTempFileBinaryToProperNameCacheFilePath.c_str(), "somePath\\cl_cache\\7\\e\\7e3291364d8df42.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenEvictCacheFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);
    cache.lockConfigFileAndReadSizeDirSize = 5;

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = false;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenBinaryDoesntFitAfterEvictionThenWriteToConfigAndReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);
    cache.lockConfigFileAndReadSizeDirSize = 9;

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = cacheSize / 3;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);

    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheDirectoryFilledToTheLimitWhenNoBytesHaveBeenEvictedAndNewBinaryDoesntFitAfterEvictionThenDontWriteToConfigAndReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);
    cache.lockConfigFileAndReadSizeDirSize = 9;

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;
    cache.evictCacheBytesEvicted = 0;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);

    EXPECT_EQ(0u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenCreateUniqueTempFileAndWriteDataFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = false;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(0u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenRenameTempFileBinaryToProperNameFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = false;

    cache.callBaseCreateCacheDirectories = false;
    cache.createCacheDirectoriesResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(1u, SysCalls::deleteFileACalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenCreateCacheDirectoriesFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    cache.callBaseCreateCacheDirectories = false;
    cache.createCacheDirectoriesResult = false;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);

    EXPECT_FALSE(result);
    EXPECT_EQ(1u, SysCalls::unlockFileExCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.createCacheDirectoriesCalled);
    EXPECT_EQ(0u, cache.renameTempFileBinaryToProperNameCalled);
}

TEST_F(CompilerCacheWindowsTest, givenFindFirstFileASuccessWhenGetFileModificationTimeThenFindCloseIsCalled) {
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    [[maybe_unused]] auto modificationTime = NEO::getFileModificationTime("C:\\Users\\user1\\file1.txt");
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
}

TEST_F(CompilerCacheWindowsTest, givenFindFirstFileASuccessWhenGetFileSizeThenFindCloseIsCalled) {
    SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    [[maybe_unused]] auto fileSize = NEO::getFileSize("C:\\Users\\user1\\file1.txt");
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenStatsDisabledThenLoadCachedBinaryDoesntCallUpdateAllStats) {
    char fileData[4] = {'a', 'b', 'c', 'd'};

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, NEO::IoFunctions::mockFopen);
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> fseekBackup(&NEO::IoFunctions::fseekPtr, NEO::IoFunctions::mockFseek);
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> ftellBackup(&NEO::IoFunctions::ftellPtr, NEO::IoFunctions::mockFtell);
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> freadBackup(&NEO::IoFunctions::freadPtr, NEO::IoFunctions::mockFread);
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> fcloseBackup(&NEO::IoFunctions::fclosePtr, NEO::IoFunctions::mockFclose);

    VariableBackup<decltype(NEO::IoFunctions::mockFopenReturned)> fopenReturnedBackup(&NEO::IoFunctions::mockFopenReturned, reinterpret_cast<FILE *>(0x1));
    VariableBackup<decltype(NEO::IoFunctions::mockFtellReturn)> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, 4);
    VariableBackup<decltype(NEO::IoFunctions::mockFreadReturn)> freadReturnBackup(&NEO::IoFunctions::mockFreadReturn, 4);
    VariableBackup<decltype(NEO::IoFunctions::mockFreadBuffer)> freadBufferBackup(&NEO::IoFunctions::mockFreadBuffer, fileData);

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, false, ".cl_cache", "somePath\\cl_cache", cacheSize});
    size_t cachedBinarySize = 0;

    EXPECT_NE(nullptr, cache.loadCachedBinary("file", cachedBinarySize));
    EXPECT_NE(0u, cachedBinarySize);
    EXPECT_EQ(0u, cache.updateAllStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenStatsEnabledThenLoadCachedBinaryCallsUpdateAllStats) {
    char fileData[4] = {'a', 'b', 'c', 'd'};

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, NEO::IoFunctions::mockFopen);
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> fseekBackup(&NEO::IoFunctions::fseekPtr, NEO::IoFunctions::mockFseek);
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> ftellBackup(&NEO::IoFunctions::ftellPtr, NEO::IoFunctions::mockFtell);
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> freadBackup(&NEO::IoFunctions::freadPtr, NEO::IoFunctions::mockFread);
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> fcloseBackup(&NEO::IoFunctions::fclosePtr, NEO::IoFunctions::mockFclose);

    VariableBackup<decltype(NEO::IoFunctions::mockFopenReturned)> fopenReturnedBackup(&NEO::IoFunctions::mockFopenReturned, reinterpret_cast<FILE *>(0x1));
    VariableBackup<decltype(NEO::IoFunctions::mockFtellReturn)> ftellReturnBackup(&NEO::IoFunctions::mockFtellReturn, 4);
    VariableBackup<decltype(NEO::IoFunctions::mockFreadReturn)> freadReturnBackup(&NEO::IoFunctions::mockFreadReturn, 4);
    VariableBackup<decltype(NEO::IoFunctions::mockFreadBuffer)> freadBufferBackup(&NEO::IoFunctions::mockFreadBuffer, fileData);

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});
    cache.callBaseUpdateAllStats = false;

    size_t cachedBinarySize = 0;

    EXPECT_NE(nullptr, cache.loadCachedBinary("file", cachedBinarySize));
    EXPECT_NE(0u, cachedBinarySize);
    EXPECT_EQ(1u, cache.updateAllStatsCalled);
    EXPECT_TRUE(cache.updateAllStatsCacheHit);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenGetCachedFileFailsThenLoadCachedBinaryReturnsNullptr) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});
    size_t cachedBinarySize = 1;

    EXPECT_EQ(nullptr, cache.loadCachedBinary("f", cachedBinarySize));
    EXPECT_EQ(0u, cachedBinarySize);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenStatsDisabledAndCacheMissThenLoadCachedBinaryDoesntCallUpdateAllStats) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return nullptr; });

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, false, ".cl_cache", "somePath\\cl_cache", cacheSize});
    size_t cachedBinarySize = 1;

    EXPECT_EQ(nullptr, cache.loadCachedBinary("file", cachedBinarySize));
    EXPECT_EQ(0u, cachedBinarySize);
    EXPECT_EQ(0u, cache.updateAllStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenUpdateAllStatsThenCallUpdateStatsForEachDirectory) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseUpdateStats = false;
    cache.updateStatsResult = true;

    EXPECT_TRUE(cache.updateAllStats("file.cl_cache", true));

    EXPECT_EQ(static_cast<size_t>(cache.maxCacheDepth + 1), cache.updateStatsCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCompilerCacheWhenUpdateStatsFailsThenUpdateAllStatsFails) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseUpdateStats = false;
    cache.updateStatsResult = false;

    EXPECT_FALSE(cache.updateAllStats("file.cl_cache", true));

    EXPECT_EQ(1u, cache.updateStatsCalled);
}

} // namespace NEO
