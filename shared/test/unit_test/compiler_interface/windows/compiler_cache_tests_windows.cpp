/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include <cstring>
#include <string>

namespace NEO {

class CompilerCacheMockWindows : public CompilerCache {
  public:
    CompilerCacheMockWindows(const CompilerCacheConfig &config) : CompilerCache(config) {}
    using CompilerCache::createUniqueTempFileAndWriteData;
    using CompilerCache::evictCache;
    using CompilerCache::lockConfigFileAndReadSize;
    using CompilerCache::renameTempFileBinaryToProperName;

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
};

TEST(CompilerCacheHelper, GivenHomeEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperDirectoryIsReturned) {
    NEO::EnvironmentVariableReader envReader;
    std::string cacheDir = "";
    EXPECT_FALSE(checkDefaultCacheDirSettings(cacheDir, envReader));
}

namespace SysCalls {
extern DWORD getLastErrorResult;

extern size_t closeHandleCalled;

extern size_t getTempFileNameACalled;
extern UINT getTempFileNameAResult;

extern size_t lockFileExCalled;
extern BOOL lockFileExResult;

extern size_t unlockFileExCalled;
extern BOOL unlockFileExResult;

extern size_t createFileACalled;
extern const size_t createFileAResultsCount;
extern HANDLE createFileAResults[];

extern size_t deleteFileACalled;
extern char deleteFiles[][256];

extern bool callBaseReadFile;
extern BOOL readFileResult;
extern size_t readFileCalled;
extern size_t readFileBufferData;

extern size_t writeFileCalled;
extern BOOL writeFileResult;
extern const size_t writeFileBufferSize;
extern char writeFileBuffer[];
extern DWORD writeFileNumberOfBytesWritten;

extern HANDLE findFirstFileAResult;

extern size_t findNextFileACalled;
extern const size_t findNextFileAFileDataCount;
extern WIN32_FIND_DATAA findNextFileAFileData[];

extern size_t findCloseCalled;

extern size_t getFileAttributesCalled;
extern DWORD getFileAttributesResult;

extern size_t setFilePointerCalled;
extern DWORD setFilePointerResult;
} // namespace SysCalls

struct CompilerCacheWindowsTest : public ::testing::Test {
    CompilerCacheWindowsTest()
        : getLastErrorResultBackup(&SysCalls::getLastErrorResult),
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
          readFileBufferDataBackup(&SysCalls::readFileBufferData),
          writeFileCalledBackup(&SysCalls::writeFileCalled),
          writeFileResultBackup(&SysCalls::writeFileResult),
          writeFileNumberOfBytesWrittenBackup(&SysCalls::writeFileNumberOfBytesWritten),
          findFirstFileAResultBackup(&SysCalls::findFirstFileAResult),
          findNextFileACalledBackup(&SysCalls::findNextFileACalled),
          getFileAttributesCalledBackup(&SysCalls::getFileAttributesCalled),
          getFileAttributesResultBackup(&SysCalls::getFileAttributesResult),
          setFilePointerCalledBackup(&SysCalls::setFilePointerCalled),
          setFilePointerResultBackup(&SysCalls::setFilePointerResult) {}

    void SetUp() override {
        SysCalls::closeHandleCalled = 0u;
        SysCalls::getTempFileNameACalled = 0u;
        SysCalls::lockFileExCalled = 0u;
        SysCalls::unlockFileExCalled = 0u;
        SysCalls::createFileACalled = 0u;
        SysCalls::deleteFileACalled = 0u;
        SysCalls::readFileCalled = 0u;
        SysCalls::writeFileCalled = 0u;
        SysCalls::findNextFileACalled = 0u;
        SysCalls::getFileAttributesCalled = 0u;
        SysCalls::setFilePointerCalled = 0u;
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
        memset(SysCalls::writeFileBuffer, 0, SysCalls::writeFileBufferSize);
    }

  protected:
    VariableBackup<DWORD> getLastErrorResultBackup;
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
    VariableBackup<size_t> readFileBufferDataBackup;
    VariableBackup<size_t> writeFileCalledBackup;
    VariableBackup<BOOL> writeFileResultBackup;
    VariableBackup<DWORD> writeFileNumberOfBytesWrittenBackup;
    VariableBackup<HANDLE> findFirstFileAResultBackup;
    VariableBackup<size_t> findNextFileACalledBackup;
    VariableBackup<size_t> getFileAttributesCalledBackup;
    VariableBackup<DWORD> getFileAttributesResultBackup;
    VariableBackup<size_t> setFilePointerCalledBackup;
    VariableBackup<DWORD> setFilePointerResultBackup;
};

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

    SysCalls::findFirstFileAResult = reinterpret_cast<HANDLE>(0x1234);
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});
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
        snprintf(filesData[i].cFileName, MAX_PATH, "file_%zu.cl_cache", i);
        filesData[i].nFileSizeHigh = 0;
        filesData[i].nFileSizeLow = cacheFileSize;
        filesData[i].ftLastAccessTime.dwHighDateTime = 0;
        filesData[i].dwFileAttributes = fileAttributes[i];

        SysCalls::findNextFileAFileData[i] = filesData[i];
    }

    SysCalls::findFirstFileAResult = reinterpret_cast<HANDLE>(0x1234);
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});
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

    SysCalls::findFirstFileAResult = INVALID_HANDLE_VALUE;
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    uint64_t bytesEvicted{0u};

    ::testing::internal::CaptureStderr();
    cache.evictCache(bytesEvicted);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    SysCalls::readFileBufferData = readCacheDirSize;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    ::testing::internal::CaptureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    ::testing::internal::CaptureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;

    ::testing::internal::CaptureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    SysCalls::readFileBufferData = readCacheDirSize;

    SysCalls::getLastErrorResult = ERROR_FILE_NOT_FOUND;

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    SysCalls::readFileBufferData = readCacheDirSize;

    SysCalls::getLastErrorResult = ERROR_FILE_NOT_FOUND;

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    ::testing::internal::CaptureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    SysCalls::readFileBufferData = readCacheDirSize;

    SysCalls::getLastErrorResult = ERROR_FILE_NOT_FOUND + 1;

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    UnifiedHandle configFileHandle{nullptr};
    size_t directorySize = 0u;
    ::testing::internal::CaptureStderr();
    cache.lockConfigFileAndReadSize("somePath\\cl_cache\\config.file", configFileHandle, directorySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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

TEST_F(CompilerCacheWindowsTest, givenCreateUniqueTempFileAndWriteDataWhenCreateAndWriteTempFileSucceedsThenBinaryIsWritten) {
    SysCalls::getTempFileNameAResult = 6u;
    SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1234);
    SysCalls::writeFileResult = TRUE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    ::testing::internal::CaptureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    ::testing::internal::CaptureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    ::testing::internal::CaptureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    const char *binary = "12345";
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(strlen(binary)) - 1;
    ::testing::internal::CaptureStderr();
    char tmpFileName[] = "somePath\\cl_cache\\TMP.XXXXXX";
    cache.createUniqueTempFileAndWriteData(tmpFileName, binary, strlen(binary));
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    auto result = cache.cacheBinary("7e3291364d8df42", "123456", cacheSize * 2);
    EXPECT_FALSE(result);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenAllStepsSuccessThenReturnTrue) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

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
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(0, strcmp(cache.createUniqueTempFileAndWriteDataTmpFilePath.c_str(), "somePath\\cl_cache\\cl_cache.XXXXXX"));
    EXPECT_EQ(0, strcmp(cache.renameTempFileBinaryToProperNameCacheFilePath.c_str(), "somePath\\cl_cache\\7e3291364d8df42.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenCacheAlreadyExistsThenDoNotCreateCacheAndReturnTrue) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES - 1;
    SysCalls::getLastErrorResult = ERROR_FILE_NOT_FOUND + 1;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenWriteToConfigFileFailsThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;
    SysCalls::writeFileResult = FALSE;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t));

    ::testing::internal::CaptureStderr();
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

    std::string expectedStderrSubstr("[Cache failure]: Writing to config file failed! error code:");

    EXPECT_TRUE(result);
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr));
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenWriteFileBytesWrittenMismatchesThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = true;

    const std::string kernelFileHash = "7e3291364d8df42";
    const char *binary = "123456";
    const size_t binarySize = strlen(binary);
    SysCalls::writeFileNumberOfBytesWritten = static_cast<DWORD>(sizeof(size_t)) - 1;

    ::testing::internal::CaptureStderr();
    auto result = cache.cacheBinary(kernelFileHash, binary, binarySize);
    auto capturedStderr = ::testing::internal::GetCapturedStderr();

    std::stringstream expectedStderrSubstr;
    expectedStderrSubstr << "[Cache failure]: Writing to config file failed! Incorrect number of bytes written: ";
    expectedStderrSubstr << SysCalls::writeFileNumberOfBytesWritten << " vs " << static_cast<DWORD>(sizeof(size_t));

    EXPECT_TRUE(result);
    EXPECT_TRUE(hasSubstr(capturedStderr, expectedStderrSubstr.str()));
    EXPECT_EQ(1u, cache.lockConfigFileAndReadSizeCalled);
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
    EXPECT_EQ(1u, cache.createUniqueTempFileAndWriteDataCalled);
    EXPECT_EQ(1u, cache.renameTempFileBinaryToProperNameCalled);
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenLockConfigFileAndReadSizeFailsThenEarlyReturnFalse) {
    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    EXPECT_EQ(1u, SysCalls::writeFileCalled);
    EXPECT_EQ(0, strcmp(cache.createUniqueTempFileAndWriteDataTmpFilePath.c_str(), "somePath\\cl_cache\\cl_cache.XXXXXX"));
    EXPECT_EQ(0, strcmp(cache.renameTempFileBinaryToProperNameCacheFilePath.c_str(), "somePath\\cl_cache\\7e3291364d8df42.cl_cache"));
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenEvictCacheFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenBinaryDoesntFitAfterEvictionThenWriteToConfigAndReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
}

TEST_F(CompilerCacheWindowsTest, givenCacheDirectoryFilledToTheLimitWhenNoBytesHaveBeenEvictedAndNewBinaryDoesntFitAfterEvictionThenDontWriteToConfigAndReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = 10u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenCreateUniqueTempFileAndWriteDataFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

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
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST_F(CompilerCacheWindowsTest, givenCacheBinaryWhenRenameTempFileBinaryToProperNameFailsThenReturnFalse) {
    SysCalls::getFileAttributesResult = INVALID_FILE_ATTRIBUTES;

    const size_t cacheSize = MemoryConstants::megaByte - 2u;
    CompilerCacheMockWindows cache({true, ".cl_cache", "somePath\\cl_cache", cacheSize});

    cache.callBaseLockConfigFileAndReadSize = false;
    cache.lockConfigFileAndReadSizeHandle = reinterpret_cast<HANDLE>(0x1234);

    cache.callBaseEvictCache = false;
    cache.evictCacheResult = true;

    cache.callBaseCreateUniqueTempFileAndWriteData = false;
    cache.createUniqueTempFileAndWriteDataResult = true;

    cache.callBaseRenameTempFileBinaryToProperName = false;
    cache.renameTempFileBinaryToProperNameResult = false;

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
    EXPECT_EQ(1u, SysCalls::deleteFileACalled);
    EXPECT_EQ(0u, SysCalls::writeFileCalled);
}

TEST(CompilerCacheHelperWindowsTest, givenFindFirstFileASuccessWhenGetFileModificationTimeThenFindCloseIsCalled) {
    VariableBackup<HANDLE> findFirstFileAResultBackup(&SysCalls::findFirstFileAResult, reinterpret_cast<HANDLE>(0x1234));
    VariableBackup<size_t> findCloseCalledBackup(&SysCalls::findCloseCalled, 0u);
    [[maybe_unused]] auto modificationTime = NEO::getFileModificationTime("C:\\Users\\user1\\file1.txt");
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
}

TEST(CompilerCacheHelperWindowsTest, givenFindFirstFileASuccessWhenGetFileSizeThenFindCloseIsCalled) {
    VariableBackup<HANDLE> findFirstFileAResultBackup(&SysCalls::findFirstFileAResult, reinterpret_cast<HANDLE>(0x1234));
    VariableBackup<size_t> findCloseCalledBackup(&SysCalls::findCloseCalled, 0u);
    [[maybe_unused]] auto fileSize = NEO::getFileSize("C:\\Users\\user1\\file1.txt");
    EXPECT_EQ(1u, SysCalls::findCloseCalled);
}

} // namespace NEO
