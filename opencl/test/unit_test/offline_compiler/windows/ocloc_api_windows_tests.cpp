/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/test.h"

#include <cstring>

namespace NEO {
namespace SysCalls {
extern size_t createFileACalled;
extern const size_t createFileAResultsCount;
extern HANDLE createFileAResults[];

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

extern size_t lockFileExCalled;
extern BOOL lockFileExResult;

extern size_t unlockFileExCalled;
extern BOOL unlockFileExResult;

extern size_t closeHandleCalled;

extern size_t setFilePointerCalled;
extern DWORD setFilePointerResult;

extern size_t getLastErrorCalled;
extern const size_t getLastErrorResultCount;
extern DWORD getLastErrorResults[];
extern BOOL getLastErrorConstantResult;
} // namespace SysCalls
} // namespace NEO

struct OclocApiWindowsCacheTest : public ::testing::Test {
    OclocApiWindowsCacheTest()
        : createFileACalledBackup(&NEO::SysCalls::createFileACalled),
          callBaseReadFileBackup(&NEO::SysCalls::callBaseReadFile),
          readFileResultBackup(&NEO::SysCalls::readFileResult),
          readFileCalledBackup(&NEO::SysCalls::readFileCalled),
          readFileNumberOfBytesReadBackup(&NEO::SysCalls::readFileNumberOfBytesRead),
          writeFileCalledBackup(&NEO::SysCalls::writeFileCalled),
          writeFileResultBackup(&NEO::SysCalls::writeFileResult),
          writeFileNumberOfBytesWrittenBackup(&NEO::SysCalls::writeFileNumberOfBytesWritten),
          findFirstFileACalledBackup(&NEO::SysCalls::findFirstFileACalled),
          findNextFileACalledBackup(&NEO::SysCalls::findNextFileACalled),
          findNextFileAResultBackup(&NEO::SysCalls::findNextFileAResult),
          findCloseCalledBackup(&NEO::SysCalls::findCloseCalled),
          lockFileExCalledBackup(&NEO::SysCalls::lockFileExCalled),
          lockFileExResultBackup(&NEO::SysCalls::lockFileExResult),
          unlockFileExCalledBackup(&NEO::SysCalls::unlockFileExCalled),
          unlockFileExResultBackup(&NEO::SysCalls::unlockFileExResult),
          closeHandleCalledBackup(&NEO::SysCalls::closeHandleCalled),
          setFilePointerCalledBackup(&NEO::SysCalls::setFilePointerCalled),
          setFilePointerResultBackup(&NEO::SysCalls::setFilePointerResult),
          getLastErrorCalledBackup(&NEO::SysCalls::getLastErrorCalled),
          getLastErrorConstantResultBackup(&NEO::SysCalls::getLastErrorConstantResult) {}

    void SetUp() override {
        for (size_t i = 0; i < NEO::SysCalls::createFileAResultsCount; i++) {
            createFileAResultsBackup[i] = NEO::SysCalls::createFileAResults[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::getLastErrorResultCount; i++) {
            getLastErrorResultsBackup[i] = NEO::SysCalls::getLastErrorResults[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::findFirstFileAResultsCount; i++) {
            findFirstFileAResultsBackup[i] = NEO::SysCalls::findFirstFileAResults[i];
            findFirstFileAFfdBackup[i] = NEO::SysCalls::findFirstFileAFfd[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::findNextFileAFileDataCount; i++) {
            findNextFileAFileDataBackup[i] = NEO::SysCalls::findNextFileAFileData[i];
        }
        memcpy(readFileBufferDataBackup, NEO::SysCalls::readFileBufferData, NEO::SysCalls::readFileBufferDataSize);
        memcpy(writeFileBufferBackup, NEO::SysCalls::writeFileBuffer, NEO::SysCalls::writeFileBufferSize);
    }

    void TearDown() override {
        for (size_t i = 0; i < NEO::SysCalls::createFileAResultsCount; i++) {
            NEO::SysCalls::createFileAResults[i] = createFileAResultsBackup[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::getLastErrorResultCount; i++) {
            NEO::SysCalls::getLastErrorResults[i] = getLastErrorResultsBackup[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::findFirstFileAResultsCount; i++) {
            NEO::SysCalls::findFirstFileAResults[i] = findFirstFileAResultsBackup[i];
            NEO::SysCalls::findFirstFileAFfd[i] = findFirstFileAFfdBackup[i];
        }
        for (size_t i = 0; i < NEO::SysCalls::findNextFileAFileDataCount; i++) {
            NEO::SysCalls::findNextFileAFileData[i] = findNextFileAFileDataBackup[i];
        }
        memcpy(NEO::SysCalls::readFileBufferData, readFileBufferDataBackup, NEO::SysCalls::readFileBufferDataSize);
        memcpy(NEO::SysCalls::writeFileBuffer, writeFileBufferBackup, NEO::SysCalls::writeFileBufferSize);
    }

  protected:
    VariableBackup<size_t> createFileACalledBackup;
    VariableBackup<bool> callBaseReadFileBackup;
    VariableBackup<BOOL> readFileResultBackup;
    VariableBackup<size_t> readFileCalledBackup;
    VariableBackup<DWORD> readFileNumberOfBytesReadBackup;
    VariableBackup<size_t> writeFileCalledBackup;
    VariableBackup<BOOL> writeFileResultBackup;
    VariableBackup<DWORD> writeFileNumberOfBytesWrittenBackup;
    VariableBackup<size_t> findFirstFileACalledBackup;
    VariableBackup<size_t> findNextFileACalledBackup;
    VariableBackup<BOOL> findNextFileAResultBackup;
    VariableBackup<size_t> findCloseCalledBackup;
    VariableBackup<size_t> lockFileExCalledBackup;
    VariableBackup<BOOL> lockFileExResultBackup;
    VariableBackup<size_t> unlockFileExCalledBackup;
    VariableBackup<BOOL> unlockFileExResultBackup;
    VariableBackup<size_t> closeHandleCalledBackup;
    VariableBackup<size_t> setFilePointerCalledBackup;
    VariableBackup<DWORD> setFilePointerResultBackup;
    VariableBackup<size_t> getLastErrorCalledBackup;
    VariableBackup<BOOL> getLastErrorConstantResultBackup;

    HANDLE createFileAResultsBackup[4]{};
    DWORD getLastErrorResultsBackup[4]{};
    HANDLE findFirstFileAResultsBackup[4]{};
    WIN32_FIND_DATAA findFirstFileAFfdBackup[4]{};
    WIN32_FIND_DATAA findNextFileAFileDataBackup[4]{};
    char readFileBufferDataBackup[32]{};
    char writeFileBufferBackup[32]{};
};

TEST_F(OclocApiWindowsCacheTest, GivenCacheCommandWithShowStatsAndNoStatsFileThenSuccessIsReturned) {
    NEO::SysCalls::createFileAResults[0] = INVALID_HANDLE_VALUE;
    NEO::SysCalls::getLastErrorConstantResult = TRUE;
    NEO::SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    const char *argv[] = {
        "ocloc",
        "cache",
        "-show-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("No stats found in cache directory."));
}

TEST_F(OclocApiWindowsCacheTest, GivenCacheCommandWithVerboseAndShowStatsAndNoStatsFileThenSuccessIsReturned) {
    NEO::SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    NEO::SysCalls::getLastErrorConstantResult = TRUE;
    NEO::SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    const char *argv[] = {
        "ocloc",
        "cache",
        "-verbose",
        "-show-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("Reading cache statistics from path:"));
    EXPECT_NE(std::string::npos, output.find("No stats found in cache directories."));
}

TEST_F(OclocApiWindowsCacheTest, GivenCacheCommandWithZeroStatsArgAndNoStatsFileThenSuccessIsReturnedWithInfoMessage) {
    NEO::SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    NEO::SysCalls::getLastErrorConstantResult = TRUE;
    NEO::SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    const char *argv[] = {
        "ocloc",
        "cache",
        "-zero-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("No stats found to reset."));
}

TEST_F(OclocApiWindowsCacheTest, GivenCacheCommandWithVerboseAndZeroStatsThenVerboseMessageIsPrinted) {
    NEO::SysCalls::findFirstFileAResults[0] = INVALID_HANDLE_VALUE;
    NEO::SysCalls::getLastErrorConstantResult = TRUE;
    NEO::SysCalls::getLastErrorResults[0] = ERROR_FILE_NOT_FOUND;

    const char *argv[] = {
        "ocloc",
        "cache",
        "-verbose",
        "-zero-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("Resetting cache statistics in path:"));
}

TEST_F(OclocApiWindowsCacheTest, GivenNonZeroStatsWhenCacheCommandZeroStatsThenShowStatsReturnsZeroed) {
    NEO::SysCalls::callBaseReadFile = false;

    NEO::CacheStats initialStats = {42, 17, NEO::CompilerCache::cacheVersion};
    NEO::CacheStats currentStats = initialStats;

    auto setupReadForStats = [&currentStats]() {
        static_assert(sizeof(NEO::CacheStats) <= 32, "CacheStats must fit in readFileBufferData");
        memcpy(NEO::SysCalls::readFileBufferData, &currentStats, sizeof(NEO::CacheStats));
        NEO::SysCalls::readFileNumberOfBytesRead = sizeof(NEO::CacheStats);
        NEO::SysCalls::readFileResult = TRUE;
    };

    auto setupFindForStatsFile = []() {
        NEO::SysCalls::findFirstFileACalled = 0;
        NEO::SysCalls::findNextFileACalled = 0;

        memset(&NEO::SysCalls::findFirstFileAFfd[0], 0, sizeof(WIN32_FIND_DATAA));
        strncpy(NEO::SysCalls::findFirstFileAFfd[0].cFileName, "stats", MAX_PATH - 1);
        NEO::SysCalls::findFirstFileAFfd[0].dwFileAttributes = 0;
        NEO::SysCalls::findFirstFileAFfd[0].nFileSizeLow = sizeof(NEO::CacheStats);
        NEO::SysCalls::findFirstFileAResults[0] = reinterpret_cast<HANDLE>(0x1);

        NEO::SysCalls::findNextFileAResult = FALSE;
    };

    {
        NEO::SysCalls::createFileACalled = 0;
        NEO::SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1);
        setupReadForStats();

        const char *argv[] = {"ocloc", "cache", "-dir", "C:\\test\\cache", "-show-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS);
        EXPECT_NE(std::string::npos, output.find("42"));
        EXPECT_NE(std::string::npos, output.find("17"));
    }

    {
        NEO::SysCalls::createFileACalled = 0;
        NEO::SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1);
        NEO::SysCalls::writeFileResult = TRUE;
        NEO::SysCalls::writeFileNumberOfBytesWritten = sizeof(NEO::CacheStats);
        NEO::SysCalls::setFilePointerResult = 0;
        setupFindForStatsFile();

        NEO::SysCalls::getLastErrorConstantResult = TRUE;
        NEO::SysCalls::getLastErrorResults[0] = ERROR_NO_MORE_FILES;

        const char *argv[] = {"ocloc", "cache", "-dir", "C:\\test\\cache", "-zero-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);

        EXPECT_EQ(retVal, OCLOC_SUCCESS);

        NEO::CacheStats writtenStats{};
        memcpy(&writtenStats, NEO::SysCalls::writeFileBuffer, sizeof(NEO::CacheStats));

        EXPECT_EQ(0u, writtenStats.hits);
        EXPECT_EQ(0u, writtenStats.misses);
        EXPECT_EQ(NEO::CompilerCache::cacheVersion, writtenStats.version);

        currentStats = writtenStats;
    }

    {
        NEO::SysCalls::createFileACalled = 0;
        NEO::SysCalls::createFileAResults[0] = reinterpret_cast<HANDLE>(0x1);
        NEO::SysCalls::readFileCalled = 0;
        setupReadForStats();

        const char *argv[] = {"ocloc", "cache", "-dir", "C:\\test\\cache", "-show-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS);
        EXPECT_EQ(std::string::npos, output.find("42"));
        EXPECT_EQ(std::string::npos, output.find("17"));
        EXPECT_NE(std::string::npos, output.find("0.00%"));
    }
}
