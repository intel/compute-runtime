/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_compiler_cache.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include "os_inc.h"

#include <array>
#include <list>
#include <memory>

using namespace NEO;

TEST(HashGeneration, givenMisalignedBufferWhenPassedToUpdateFunctionThenProperPtrDataIsUsed) {
    Hash hash;
    auto originalPtr = alignedMalloc(1024, MemoryConstants::pageSize);

    memset(originalPtr, 0xFF, 1024);
    char *misalignedPtr = (char *)originalPtr;
    misalignedPtr++;

    //values really used
    misalignedPtr[0] = 1;
    misalignedPtr[1] = 2;
    misalignedPtr[2] = 3;
    misalignedPtr[3] = 4;
    misalignedPtr[4] = 5;
    //values not used should be ommitted
    misalignedPtr[5] = 6;
    misalignedPtr[6] = 7;

    hash.update(misalignedPtr, 3);
    auto hash1 = hash.finish();

    hash.reset();
    hash.update(misalignedPtr, 4);
    auto hash2 = hash.finish();

    hash.reset();
    hash.update(misalignedPtr, 5);
    auto hash3 = hash.finish();

    hash.reset();
    hash.update(misalignedPtr, 6);
    auto hash4 = hash.finish();

    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash1, hash4);
    EXPECT_NE(hash2, hash3);
    EXPECT_NE(hash2, hash4);
    EXPECT_NE(hash3, hash4);

    auto value2 = hash.getValue(misalignedPtr, 0);
    EXPECT_EQ(0u, value2);

    alignedFree(originalPtr);
}

TEST(HashGeneration, givenMisalignedBufferWithSizeOneWhenPassedToUpdateFunctionThenProperPtrDataIsUsed) {
    Hash hash;
    auto originalPtr = alignedMalloc(1024, MemoryConstants::pageSize);

    memset(originalPtr, 0xFF, 1024);
    char *misalignedPtr = (char *)originalPtr;
    misalignedPtr++;

    //values really used
    misalignedPtr[0] = 1;
    //values not used should be ommitted
    misalignedPtr[1] = 2;
    misalignedPtr[2] = 3;
    misalignedPtr[3] = 4;
    misalignedPtr[4] = 5;
    misalignedPtr[5] = 6;
    misalignedPtr[6] = 7;

    hash.update(misalignedPtr, 1);

    auto value = hash.finish();
    EXPECT_EQ(0x088350e6600f29c2u, value);

    alignedFree(originalPtr);
}

TEST(CompilerCacheHashTests, WhenHashingThenResultIsDeterministic) {
    Hash hash;

    std::list<uint64_t> hashes;
    char data[4] = "aBc";

    for (size_t i = 0; i <= strlen(data); i++) {
        hash.reset();
        hash.update(data, i);
        auto res = hash.finish();

        for (auto &in : hashes) {
            EXPECT_NE(in, res) << "failed: " << i << " bytes";
        }
        hashes.push_back(res);

        // hash once again to make sure results are the same
        hash.reset();
        hash.update(data, i);
        auto res2 = hash.finish();
        EXPECT_EQ(res, res2);
    }
}

TEST(CompilerCacheHashTests, GivenCompilingOptionsWhenGettingCacheThenCorrectCacheIsReturned) {
    static const size_t bufSize = 64;
    HardwareInfo hwInfo = *defaultHwInfo;

    std::set<std::string> hashes;

    PLATFORM p1 = {(PRODUCT_FAMILY)1};
    PLATFORM p2 = {(PRODUCT_FAMILY)2};
    const PLATFORM *platforms[] = {&p1, &p2};
    FeatureTable s1;
    FeatureTable s2;
    s1.flags.ftrSVM = true;
    s2.flags.ftrSVM = false;
    const FeatureTable *skus[] = {&s1, &s2};
    WorkaroundTable w1;
    WorkaroundTable w2;
    w1.flags.waDoNotUseMIReportPerfCount = true;
    w2.flags.waDoNotUseMIReportPerfCount = false;
    const WorkaroundTable *was[] = {&w1, &w2};

    std::array<std::string, 4> inputArray = {{std::string(""),
                                              std::string("12345678901234567890123456789012"),
                                              std::string("12345678910234567890123456789012"),
                                              std::string("12345678901234567891023456789012")}};

    std::array<std::string, 3> optionsArray = {{std::string(""),
                                                std::string("--some --options"),
                                                std::string("--some --different --options")}};

    std::array<std::string, 3> internalOptionsArray = {{std::string(""),
                                                        std::string("--some --options"),
                                                        std::string("--some --different --options")}};

    std::unique_ptr<char> buf1(new char[bufSize]);
    std::unique_ptr<char> buf2(new char[bufSize]);
    std::unique_ptr<char> buf3(new char[bufSize]);
    std::unique_ptr<char> buf4(new char[bufSize]);

    ArrayRef<char> src;
    ArrayRef<char> apiOptions;
    ArrayRef<char> internalOptions;

    CompilerCache cache(CompilerCacheConfig{});

    for (auto platform : platforms) {
        hwInfo.platform = *platform;

        for (auto sku : skus) {
            hwInfo.featureTable = *sku;

            for (auto wa : was) {
                hwInfo.workaroundTable = *wa;

                for (size_t i1 = 0; i1 < inputArray.size(); i1++) {
                    strcpy_s(buf1.get(), bufSize, inputArray[i1].c_str());
                    src = ArrayRef<char>(buf1.get(), strlen(buf1.get()));
                    for (size_t i2 = 0; i2 < optionsArray.size(); i2++) {
                        strcpy_s(buf2.get(), bufSize, optionsArray[i2].c_str());
                        apiOptions = ArrayRef<char>(buf2.get(), strlen(buf2.get()));
                        for (size_t i3 = 0; i3 < internalOptionsArray.size(); i3++) {
                            strcpy_s(buf3.get(), bufSize, internalOptionsArray[i3].c_str());
                            internalOptions = ArrayRef<char>(buf3.get(), strlen(buf3.get()));

                            std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions);

                            if (hashes.find(hash) != hashes.end()) {
                                FAIL() << "failed: " << i1 << ":" << i2 << ":" << i3;
                            }
                            hashes.emplace(hash);
                        }
                    }
                }
            }
        }
    }

    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions);
    std::string hash2 = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions);
    EXPECT_STREQ(hash.c_str(), hash2.c_str());
}

TEST(CompilerCacheTests, GivenBinaryCacheWhenDebugFlagIsSetThenTraceFilesAreCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.BinaryCacheTrace.set(true);

    static struct VerifyData {
        bool matched;
        const char *pattern;
    } verifyData[] = {
        {false, "---- input ----"},
        {false, "---- options ----"},
        {false, "---- internal options ----"},
        {false, "---- platform ----"},
        {false, "---- feature table ----"},
        {false, "---- workaround table ----"}};

    static std::list<std::string> *openListPtr;
    auto openList = std::make_unique<std::list<std::string>>(2);

    VariableBackup<std::list<std::string> *> openListBkp(&openListPtr, openList.get());

    // reset global state
    for (size_t idx = 0; idx < sizeof(verifyData) / sizeof(verifyData[0]); idx++) {
        verifyData[idx].matched = false;
    }
    openList->clear();

    VariableBackup<NEO::IoFunctions::fopenFuncPtr> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        openListPtr->push_back(filename);
        return IoFunctions::mockFopenReturned;
    });

    VariableBackup<NEO::IoFunctions::vfprintfFuncPtr> mockVFprintf(&NEO::IoFunctions::vfprintfPtr, [](FILE *fp, const char *formatStr, va_list) -> int {
        for (size_t idx = 0; idx < sizeof(verifyData) / sizeof(verifyData[0]); idx++) {
            if (strncmp(formatStr, verifyData[idx].pattern, strlen(verifyData[idx].pattern))) {
                verifyData[idx].matched = true;
            }
        }
        return 0;
    });

    HardwareInfo hwInfo = *defaultHwInfo;
    ArrayRef<char> src;
    ArrayRef<char> apiOptions;
    ArrayRef<char> internalOptions;
    CompilerCache cache(CompilerCacheConfig{});
    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions);

    for (size_t idx = 0; idx < sizeof(verifyData) / sizeof(verifyData[0]); idx++) {
        EXPECT_TRUE(verifyData[idx].matched);
    }
    EXPECT_EQ(openList->size(), static_cast<size_t>(2));
    std::string traceFile = PATH_SEPARATOR + hash + ".trace";
    std::string inputFile = PATH_SEPARATOR + hash + ".input";
    EXPECT_NE(std::find(openList->begin(), openList->end(), traceFile), openList->end());
    EXPECT_NE(std::find(openList->begin(), openList->end(), inputFile), openList->end());

    openList->clear();
}

TEST(CompilerCacheTests, GivenBinaryCacheWhenDebugFlagIsSetAndOpenFailesThenNoCloseOccurs) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.BinaryCacheTrace.set(true);

    VariableBackup<decltype(IoFunctions::mockFopenReturned)> retValBkp(&IoFunctions::mockFopenReturned, reinterpret_cast<FILE *>(0x0));

    // reset global state
    IoFunctions::mockFopenCalled = 0;
    IoFunctions::mockFcloseCalled = 0;
    IoFunctions::mockVfptrinfCalled = 0;
    IoFunctions::mockFwriteCalled = 0;

    HardwareInfo hwInfo = *defaultHwInfo;
    ArrayRef<char> src;
    ArrayRef<char> apiOptions;
    ArrayRef<char> internalOptions;
    CompilerCache cache(CompilerCacheConfig{});
    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions);

    EXPECT_EQ(IoFunctions::mockFopenCalled, 2u);
    EXPECT_EQ(IoFunctions::mockFcloseCalled, 0u);
    EXPECT_EQ(IoFunctions::mockVfptrinfCalled, 0u);
    EXPECT_EQ(IoFunctions::mockFwriteCalled, 0u);
}

TEST(CompilerCacheTests, GivenEmptyBinaryWhenCachingThenBinaryIsNotCached) {
    CompilerCache cache(CompilerCacheConfig{});
    bool ret = cache.cacheBinary("some_hash", nullptr, 12u);
    EXPECT_FALSE(ret);

    const char *tmp1 = "Data";
    ret = cache.cacheBinary("some_hash", tmp1, 0u);
    EXPECT_FALSE(ret);
}

TEST(CompilerCacheTests, GivenNonExistantConfigWhenLoadingFromCacheThenNullIsReturned) {
    CompilerCache cache(CompilerCacheConfig{});
    size_t size;
    auto ret = cache.loadCachedBinary("----do-not-exists----", size);
    EXPECT_EQ(nullptr, ret);
    EXPECT_EQ(0U, size);
}

TEST(CompilerInterfaceCachedTests, GivenNoCachedBinaryWhenBuildingThenErrorIsReturned) {
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";

    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::unique_ptr<CompilerCacheMock> cache(new CompilerCacheMock());
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));

    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, err);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST(CompilerInterfaceCachedTests, GivenCachedBinaryWhenBuildingThenSuccessIsReturned) {
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::unique_ptr<CompilerCacheMock> cache(new CompilerCacheMock());
    cache->loadResult = true;
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));

    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST(CompilerInterfaceCachedTests, givenKernelWithoutIncludesAndBinaryInCacheWhenCompilationRequestedThenFCLIsNotCalled) {
    MockDevice device{};
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    // we force both compilers to fail compilation request
    // at the end we expect CL_SUCCESS which means compilation ends in cache
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::unique_ptr<CompilerCacheMock> cache(new CompilerCacheMock());
    cache->loadResult = true;
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto retVal = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, retVal);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST(CompilerInterfaceCachedTests, givenKernelWithIncludesAndBinaryInCacheWhenCompilationRequestedThenFCLIsCalled) {
    MockDevice device{};
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"file.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    std::unique_ptr<CompilerCacheMock> cache(new CompilerCacheMock());
    cache->loadResult = true;
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto retVal = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, retVal);

    gEnvironment->fclPopDebugVars();
}
