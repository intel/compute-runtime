/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_compiler_cache.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
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

    // values really used
    misalignedPtr[0] = 1;
    misalignedPtr[1] = 2;
    misalignedPtr[2] = 3;
    misalignedPtr[3] = 4;
    misalignedPtr[4] = 5;
    // values not used should be ommitted
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

    // values really used
    misalignedPtr[0] = 1;
    // values not used should be ommitted
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

    std::array<std::string, 4> igcRevisions = {{std::string(""),
                                                std::string("0000000000000000000000000000000000000000"),
                                                std::string("0000000000000000000000000000000000000001"),
                                                std::string("abcdef1234567890abcdef123456789000000000")}};

    const size_t igcLibSize = 304297;
    const time_t igcLibMTime = 167594873;
    const size_t igcLibSizes[] = {0, 1, 1024, igcLibSize};
    const time_t igcLibMTimes[] = {0, 102, igcLibMTime};

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
    w1.flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    w2.flags.wa4kAlignUVOffsetNV12LinearSurface = false;
    const WorkaroundTable *was[] = {&w1, &w2};
    const uint32_t ipVersionValues[] = {1, 2};

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

    std::unique_ptr<char[]> buf0(new char[bufSize]);
    std::unique_ptr<char[]> buf1(new char[bufSize]);
    std::unique_ptr<char[]> buf2(new char[bufSize]);
    std::unique_ptr<char[]> buf3(new char[bufSize]);

    ArrayRef<char> igcRevision;

    ArrayRef<char> src;
    ArrayRef<char> apiOptions;
    ArrayRef<char> internalOptions;

    CompilerCache cache(CompilerCacheConfig{});

    strcpy_s(buf0.get(), bufSize, igcRevisions[0].c_str());
    igcRevision = ArrayRef<char>(buf0.get(), strlen(buf0.get()));
    strcpy_s(buf1.get(), bufSize, inputArray[0].c_str());
    src = ArrayRef<char>(buf1.get(), strlen(buf1.get()));
    strcpy_s(buf2.get(), bufSize, optionsArray[0].c_str());
    apiOptions = ArrayRef<char>(buf2.get(), strlen(buf2.get()));
    strcpy_s(buf3.get(), bufSize, internalOptionsArray[0].c_str());
    internalOptions = ArrayRef<char>(buf3.get(), strlen(buf3.get()));
    auto libSize = igcLibSizes[0];
    auto libMTime = igcLibMTimes[0];
    hwInfo.platform = *platforms[0];
    hwInfo.featureTable = *skus[0];
    hwInfo.workaroundTable = *was[0];
    hwInfo.ipVersion.architecture = ipVersionValues[0];
    hwInfo.ipVersion.release = ipVersionValues[0];
    hwInfo.ipVersion.revision = ipVersionValues[0];

    auto verifyHash = [&]() -> void {
        std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, libSize, libMTime);

        ASSERT_TRUE(hashes.find(hash) == hashes.end());
        hashes.emplace(hash);
    };
    verifyHash();

    for (size_t i = 1; i < igcRevisions.size(); i++) {
        strcpy_s(buf0.get(), bufSize, igcRevisions[i].c_str());
        igcRevision = ArrayRef<char>(buf0.get(), strlen(buf0.get()));
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(igcLibSizes); i++) {
        libSize = igcLibSizes[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(igcLibMTimes); i++) {
        libMTime = igcLibMTimes[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(platforms); i++) {
        hwInfo.platform = *platforms[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(skus); i++) {
        hwInfo.featureTable = *skus[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(was); i++) {
        hwInfo.workaroundTable = *was[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(ipVersionValues); i++) {
        hwInfo.ipVersion.architecture = ipVersionValues[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(ipVersionValues); i++) {
        hwInfo.ipVersion.release = ipVersionValues[i];
        verifyHash();
    }
    for (size_t i = 1; i < arrayCount(ipVersionValues); i++) {
        hwInfo.ipVersion.revision = ipVersionValues[i];
        verifyHash();
    }
    for (size_t i = 1; i < inputArray.size(); i++) {
        strcpy_s(buf1.get(), bufSize, inputArray[i].c_str());
        src = ArrayRef<char>(buf1.get(), strlen(buf1.get()));
        verifyHash();
    }
    for (size_t i = 1; i < optionsArray.size(); i++) {
        strcpy_s(buf2.get(), bufSize, optionsArray[i].c_str());
        apiOptions = ArrayRef<char>(buf2.get(), strlen(buf2.get()));
        verifyHash();
    }
    for (size_t i = 1; i < internalOptionsArray.size(); i++) {
        strcpy_s(buf3.get(), bufSize, internalOptionsArray[i].c_str());
        internalOptions = ArrayRef<char>(buf3.get(), strlen(buf3.get()));
        verifyHash();
    }

    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
    std::string hash2 = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
    EXPECT_STREQ(hash.c_str(), hash2.c_str());
}

TEST(CompilerCacheTests, GivenBinaryCacheWhenDebugFlagIsSetThenTraceFilesAreCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.BinaryCacheTrace.set(true);

    static struct VerifyData {
        bool matched;
        const char *pattern;
    } verifyData[] = {
        {false, "---- igcRevision ----"},
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
    ArrayRef<char> revision;
    size_t libSize = 0;
    time_t libMTime = 0;
    CompilerCache cache(CompilerCacheConfig{});
    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), revision, libSize, libMTime);

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
    debugManager.flags.BinaryCacheTrace.set(true);

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
    ArrayRef<char> revision;
    size_t libSize = 0;
    time_t libMTime = 0;
    CompilerCache cache(CompilerCacheConfig{});
    std::string hash = cache.getCachedFileName(hwInfo, src, apiOptions, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), revision, libSize, libMTime);

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
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
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
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, err);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST(CompilerInterfaceCachedTests, GivenCachedBinaryWhenBuildingThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();

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
    cache->config.enabled = true;
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));

    TranslationOutput translationOutput;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

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
    cache->config.enabled = true;
    auto compilerInterface = std::unique_ptr<CompilerInterface>(CompilerInterface::createInstance(std::move(cache), true));
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto retVal = compilerInterface->build(device, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, retVal);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, retVal);

    gEnvironment->fclPopDebugVars();
}

class CompilerInterfaceOclElfCacheTest : public ::testing::Test, public CompilerCacheHelper {
  public:
    using CompilerCacheHelper::processPackedCacheBinary;

    void SetUp() override {
        hwInfoBackup.reset(new VariableBackup<HardwareInfo>(defaultHwInfo.get()));
        std::unique_ptr<CompilerCacheMock> cache(new CompilerCacheMock());
        cache->config.enabled = true;
        compilerInterface = std::make_unique<MockCompilerInterface>();
        bool initRet = compilerInterface->initialize(std::move(cache), true);
        ASSERT_TRUE(initRet);

        auto compilerProductHelper = CompilerProductHelper::create(defaultHwInfo->platform.eProductFamily);
        compilerProductHelper->adjustHwInfoForIgc(*defaultHwInfo);

        mockCompilerCache = static_cast<CompilerCacheMock *>(compilerInterface->cache.get());

        fclDebugVars.fileName = gEnvironment->fclGetMockFile();
        gEnvironment->fclPushDebugVars(fclDebugVars);

        igcFclDebugVarsForceBuildFailure.forceBuildFailure = true;

        igcDebugVarsDeviceBinary.fileName = gEnvironment->igcGetMockFile();
        igcDebugVarsDeviceBinary.forceBuildFailure = false;
        igcDebugVarsDeviceBinary.binaryToReturn = patchtokensProgram.storage.data();
        igcDebugVarsDeviceBinary.binaryToReturnSize = patchtokensProgram.storage.size();

        igcDebugVarsInvalidDeviceBinary.fileName = gEnvironment->igcGetMockFile();
        igcDebugVarsInvalidDeviceBinary.forceBuildFailure = false;
        igcDebugVarsInvalidDeviceBinary.binaryToReturn = invalidBinary.data();
        igcDebugVarsInvalidDeviceBinary.binaryToReturnSize = invalidBinary.size();

        igcDebugVarsDeviceBinaryDebugData.fileName = gEnvironment->igcGetMockFile();
        igcDebugVarsDeviceBinaryDebugData.forceBuildFailure = false;
        igcDebugVarsDeviceBinaryDebugData.binaryToReturn = patchtokensProgram.storage.data();
        igcDebugVarsDeviceBinaryDebugData.binaryToReturnSize = patchtokensProgram.storage.size();
        igcDebugVarsDeviceBinaryDebugData.debugDataToReturn = debugDataToReturn.data();
        igcDebugVarsDeviceBinaryDebugData.debugDataToReturnSize = debugDataToReturn.size();
    }

    void TearDown() override {
        gEnvironment->fclPopDebugVars();
    }

    bool isPackedOclElf(const std::string &data) {
        ArrayRef<const uint8_t> binary(reinterpret_cast<const uint8_t *>(data.data()), data.length());
        return isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(binary);
    }

    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcFclDebugVarsForceBuildFailure;
    MockCompilerDebugVars igcDebugVarsDeviceBinary;
    MockCompilerDebugVars igcDebugVarsInvalidDeviceBinary;
    MockCompilerDebugVars igcDebugVarsDeviceBinaryDebugData;
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    std::string debugDataToReturn = "dbgdata";
    std::string invalidBinary = "abcdefg";

    std::unique_ptr<MockCompilerInterface> compilerInterface;
    CompilerCacheMock *mockCompilerCache;
    std::unique_ptr<VariableBackup<HardwareInfo>> hwInfoBackup;
};

TEST_F(CompilerInterfaceOclElfCacheTest, givenIncorrectBinaryCausingPackDeviceBinaryToReturnEmptyVectorWhenPackAndCacheBinaryThenBinaryIsNotStoredInCache) {
    TranslationOutput outputFromCompilation;

    outputFromCompilation.deviceBinary.mem = makeCopy<char>(reinterpret_cast<const char *>(patchtokensProgram.storage.data()), patchtokensProgram.storage.size());
    outputFromCompilation.deviceBinary.size = patchtokensProgram.storage.size();

    std::string incorrectIr = "intermediateRepresentation";
    outputFromCompilation.intermediateRepresentation.mem = makeCopy(incorrectIr.c_str(), incorrectIr.length());
    outputFromCompilation.intermediateRepresentation.size = incorrectIr.length();

    auto incorrectIrBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(outputFromCompilation.intermediateRepresentation.mem.get()), outputFromCompilation.intermediateRepresentation.size);
    ASSERT_FALSE(NEO::isSpirVBitcode(incorrectIrBinary));
    ASSERT_FALSE(NEO::isLlvmBitcode(incorrectIrBinary));

    MockDevice device;
    CompilerCacheHelper::packAndCacheBinary(*mockCompilerCache, "some_hash", NEO::getTargetDevice(device.getRootDeviceEnvironment()), outputFromCompilation);

    EXPECT_EQ(0u, mockCompilerCache->hashToBinaryMap.size());
}

TEST_F(CompilerInterfaceOclElfCacheTest, givenEmptyTranslationOutputWhenProcessPackedCacheBinaryThenDeviceBinaryAndDebugDataAndIrAreCorrectlyStored) {
    TranslationOutput outputFromCompilation;

    outputFromCompilation.deviceBinary.mem = makeCopy<char>(reinterpret_cast<const char *>(patchtokensProgram.storage.data()), patchtokensProgram.storage.size());
    outputFromCompilation.deviceBinary.size = patchtokensProgram.storage.size();

    const char *debugData = "dbgdata";
    outputFromCompilation.debugData.mem = makeCopy(debugData, strlen(debugData));
    outputFromCompilation.debugData.size = strlen(debugData);

    std::string ir = NEO::spirvMagic.str() + "intermediateRepresentation";
    outputFromCompilation.intermediateRepresentation.mem = makeCopy(ir.c_str(), ir.length());
    outputFromCompilation.intermediateRepresentation.size = ir.length();

    MockDevice device;
    CompilerCacheHelper::packAndCacheBinary(*mockCompilerCache, "some_hash", NEO::getTargetDevice(device.getRootDeviceEnvironment()), outputFromCompilation);

    auto cachedBinary = mockCompilerCache->hashToBinaryMap.begin()->second;
    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(cachedBinary.c_str()), cachedBinary.length());

    TranslationOutput emptyTranslationOutput;
    CompilerCacheHelper::processPackedCacheBinary(archive, emptyTranslationOutput, device);

    EXPECT_EQ(0, memcmp(outputFromCompilation.deviceBinary.mem.get(), emptyTranslationOutput.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(0, memcmp(outputFromCompilation.debugData.mem.get(), emptyTranslationOutput.debugData.mem.get(), outputFromCompilation.debugData.size));
    EXPECT_EQ(0, memcmp(outputFromCompilation.intermediateRepresentation.mem.get(), emptyTranslationOutput.intermediateRepresentation.mem.get(), outputFromCompilation.intermediateRepresentation.size));
}

TEST_F(CompilerInterfaceOclElfCacheTest, givenNonEmptyTranslationOutputWhenProcessPackedCacheBinaryThenNonEmptyContainersAreNotOverwritten) {
    TranslationOutput outputFromCompilation;

    outputFromCompilation.deviceBinary.mem = makeCopy<char>(reinterpret_cast<const char *>(patchtokensProgram.storage.data()), patchtokensProgram.storage.size());
    outputFromCompilation.deviceBinary.size = patchtokensProgram.storage.size();

    const char *debugData = "dbgdata";
    outputFromCompilation.debugData.mem = makeCopy(debugData, strlen(debugData));
    outputFromCompilation.debugData.size = strlen(debugData);

    std::string ir = NEO::spirvMagic.str() + "intermediateRepresentation";
    outputFromCompilation.intermediateRepresentation.mem = makeCopy(ir.c_str(), ir.length());
    outputFromCompilation.intermediateRepresentation.size = ir.length();

    MockDevice device;
    CompilerCacheHelper::packAndCacheBinary(*mockCompilerCache, "some_hash", NEO::getTargetDevice(device.getRootDeviceEnvironment()), outputFromCompilation);

    auto cachedBinary = mockCompilerCache->hashToBinaryMap.begin()->second;
    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(cachedBinary.c_str()), cachedBinary.length());

    TranslationOutput nonEmptyTranslationOutput;

    const char *existingDeviceBinary = "existingDeviceBinary";
    nonEmptyTranslationOutput.deviceBinary.mem = makeCopy(existingDeviceBinary, strlen(existingDeviceBinary));
    const char *existingDebugData = "existingDebugData";
    nonEmptyTranslationOutput.debugData.mem = makeCopy(existingDebugData, strlen(existingDebugData));
    const char *existingIr = "existingIr";
    nonEmptyTranslationOutput.intermediateRepresentation.mem = makeCopy(existingIr, strlen(existingIr));

    CompilerCacheHelper::processPackedCacheBinary(archive, nonEmptyTranslationOutput, device);

    EXPECT_EQ(0, memcmp(nonEmptyTranslationOutput.deviceBinary.mem.get(), existingDeviceBinary, strlen(existingDeviceBinary)));
    EXPECT_EQ(0, memcmp(nonEmptyTranslationOutput.debugData.mem.get(), existingDebugData, strlen(existingDebugData)));
    EXPECT_EQ(0, memcmp(nonEmptyTranslationOutput.intermediateRepresentation.mem.get(), existingIr, strlen(existingIr)));
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenKernelWithIncludesWhenBuildingThenPackBinaryOnCacheSaveAndUnpackBinaryOnLoadFromCache) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsDeviceBinary);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCompilation.debugData.mem.get());

    EXPECT_EQ(1u, mockCompilerCache->hashToBinaryMap.size());
    EXPECT_TRUE(isPackedOclElf(mockCompilerCache->hashToBinaryMap.begin()->second));

    gEnvironment->igcPopDebugVars();

    // we force igc to fail compilation request
    // at the end we expect CL_SUCCESS which means compilation ends in cache
    gEnvironment->igcPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCache.deviceBinary.mem.get(), outputFromCache.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCache.debugData.mem.get());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenKernelWithIncludesWhenLoadedCacheDoesNotUnpackCorrectlyThenDoNotEndInCacheAndContinueCompilation) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsInvalidDeviceBinary);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(invalidBinary.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCompilation.debugData.mem.get());

    gEnvironment->igcPopDebugVars();

    // we force igc to fail compilation request
    // at the end we expect buildFailure which means compilation does not end with loaded cache but continues in igc
    gEnvironment->igcPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, err);

    EXPECT_EQ(nullptr, outputFromCache.deviceBinary.mem.get());
    EXPECT_EQ(nullptr, outputFromCache.debugData.mem.get());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenKernelWithIncludesAndDebugDataWhenBuildingThenPackBinaryOnCacheSaveAndUnpackBinaryOnLoadFromCache) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsDeviceBinaryDebugData);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(0, std::strncmp(debugDataToReturn.c_str(), outputFromCompilation.debugData.mem.get(), debugDataToReturn.size()));

    EXPECT_EQ(1u, mockCompilerCache->hashToBinaryMap.size());
    EXPECT_TRUE(isPackedOclElf(mockCompilerCache->hashToBinaryMap.begin()->second));

    gEnvironment->igcPopDebugVars();

    // we force igc to fail compilation request
    // at the end we expect CL_SUCCESS which means compilation ends in cache
    gEnvironment->igcPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCache.deviceBinary.mem.get(), outputFromCache.deviceBinary.size));
    EXPECT_EQ(0, std::strncmp(debugDataToReturn.c_str(), outputFromCache.debugData.mem.get(), debugDataToReturn.size()));

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenBinaryWhenBuildingThenPackBinaryOnCacheSaveAndUnpackBinaryOnLoadFromCache) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsDeviceBinary);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCompilation.debugData.mem.get());

    EXPECT_EQ(1u, mockCompilerCache->hashToBinaryMap.size());
    EXPECT_TRUE(isPackedOclElf(mockCompilerCache->hashToBinaryMap.begin()->second));

    gEnvironment->igcPopDebugVars();

    // we force fcl to fail compilation request
    // at the end we expect CL_SUCCESS which means compilation ends in cache
    gEnvironment->fclPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCache.deviceBinary.mem.get(), outputFromCache.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCache.debugData.mem.get());

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenBinaryWhenLoadedCacheDoesNotUnpackCorrectlyThenDoNotEndInCacheAndContinueCompilation) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsInvalidDeviceBinary);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(invalidBinary.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(nullptr, outputFromCompilation.debugData.mem.get());

    gEnvironment->igcPopDebugVars();

    // we force fcl to fail compilation request
    // at the end we expect buildFailure which means compilation does not end with loaded cache but continues in fcl
    gEnvironment->fclPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceOclElfCacheTest, GivenBinaryAndDebugDataWhenBuildingThenPackBinaryOnCacheSaveAndUnpackBinaryOnLoadFromCache) {
    USE_REAL_FILE_SYSTEM();

    gEnvironment->igcPushDebugVars(igcDebugVarsDeviceBinaryDebugData);

    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    TranslationOutput outputFromCompilation;
    MockDevice device;
    auto err = compilerInterface->build(device, inputArgs, outputFromCompilation);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCompilation.deviceBinary.mem.get(), outputFromCompilation.deviceBinary.size));
    EXPECT_EQ(0, std::strncmp(debugDataToReturn.c_str(), outputFromCompilation.debugData.mem.get(), debugDataToReturn.size()));

    EXPECT_EQ(1u, mockCompilerCache->hashToBinaryMap.size());
    EXPECT_TRUE(isPackedOclElf(mockCompilerCache->hashToBinaryMap.begin()->second));

    gEnvironment->igcPopDebugVars();

    // we force fcl to fail compilation request
    // at the end we expect CL_SUCCESS which means compilation ends in cache
    gEnvironment->fclPushDebugVars(igcFclDebugVarsForceBuildFailure);

    TranslationOutput outputFromCache;
    err = compilerInterface->build(device, inputArgs, outputFromCache);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), outputFromCache.deviceBinary.mem.get(), outputFromCache.deviceBinary.size));
    EXPECT_EQ(0, std::strncmp(debugDataToReturn.c_str(), outputFromCache.debugData.mem.get(), debugDataToReturn.size()));

    gEnvironment->fclPopDebugVars();
}

class CompilerCacheHelperWhitelistedTest : public ::testing::Test, public CompilerCacheHelper {
  public:
    using CompilerCacheHelper::whitelistedIncludes;

    bool isValidIncludeFormat(const std::string_view &entry) {
        size_t spacePos = entry.find(' ');
        if (spacePos == std::string_view::npos) {
            return false;
        }

        std::string_view firstWord = entry.substr(0, spacePos);
        if (firstWord != "#include") {
            return false;
        }

        std::string_view secondPart = entry.substr(spacePos + 1);
        if (secondPart.empty()) {
            return false;
        }

        return true;
    }
};

TEST_F(CompilerCacheHelperWhitelistedTest, GivenWhitelistedIncludesWhenCheckingFormatThenExpectValidIncludeFormat) {
    for (const auto &entry : whitelistedIncludes) {
        EXPECT_TRUE(isValidIncludeFormat(entry))
            << "Invalid format in whitelisted include: " << entry;
    }
}

TEST_F(CompilerCacheHelperWhitelistedTest, GivenWhitelistedIncludesWhenCheckingContentsThenExpectCorrectEntries) {
    std::vector<std::string_view> expectedEntries{
        "#include <cm/cm.h>",
        "#include <cm/cmtl.h>"};

    for (const auto &expectedEntry : expectedEntries) {
        auto it = std::find(whitelistedIncludes.begin(), whitelistedIncludes.end(), expectedEntry);
        EXPECT_NE(it, whitelistedIncludes.end())
            << "Expected entry '" << expectedEntry << "' not found in whitelistedIncludes.";
    }

    EXPECT_EQ(whitelistedIncludes.size(), expectedEntries.size())
        << "whitelistedIncludes contains unexpected entries.";
}

class CompilerCacheHelperMockedWhitelistedIncludesTests : public ::testing::Test, public CompilerCacheHelper {
  public:
    using CompilerCacheHelper::whitelistedIncludes;

    CompilerCacheHelperMockedWhitelistedIncludesTests() : backup(&whitelistedIncludes, {"#include <whitelisted>"}) {}

    void SetUp() override {}
    void TearDown() override {}

    VariableBackup<decltype(whitelistedIncludes)> backup;
};

TEST_F(CompilerCacheHelperMockedWhitelistedIncludesTests, GivenSourceWithWhitelistedIncludeWhenCheckingWhitelistedIncludesThenReturnsTrue) {
    const StackVec<const char *, 6> sources = {
        "__kernel void k() {}",                                                 // no includes
        "#include <whitelisted>\n__kernel void k() {}",                         // whitelisted include
        "",                                                                     // empty source
        "    #include <whitelisted>\n__kernel void k() {}",                     // leading spaces
        "\t#include <whitelisted>\n__kernel void k() {}",                       // leading tabs
        "#include <whitelisted>\n#include <whitelisted>\n__kernel void k() {}", // multiple whitelisted
        "\n#include <whitelisted>\n__kernel void k() {}",                       // leading newline
        "\r\n#include <whitelisted>\n__kernel void k() {}"                      // leading carriage return + newline
    };

    for (const auto &source : sources) {
        ArrayRef<const char> sourceRef(source, strlen(source));
        EXPECT_TRUE(CompilerCacheHelper::validateIncludes(sourceRef, CompilerCacheHelper::whitelistedIncludes))
            << "Failed for source: " << source;
    }
}

TEST_F(CompilerCacheHelperMockedWhitelistedIncludesTests, GivenSourceWithNonWhitelistedIncludeWhenCheckingWhitelistedIncludesThenReturnsFalse) {
    const StackVec<const char *, 3> sources = {
        "#include <unknown/unknown.h>\n__kernel void k() {}",                         // non-whitelisted include
        "#include <whitelisted>\n#include <unknown/unknown.h>\n__kernel void k() {}", // mixed includes - first whitelisted, second non-whitelisted
        "#include <unknown/unknown.h>\n#include <whitelisted>\n__kernel void k() {}"  // mixed includes - first non-whitelisted, second whitelisted
    };

    for (const auto &source : sources) {
        ArrayRef<const char> sourceRef(source, strlen(source));
        EXPECT_FALSE(CompilerCacheHelper::validateIncludes(sourceRef, CompilerCacheHelper::whitelistedIncludes))
            << "Failed for source: " << source;
    }
}

TEST_F(CompilerCacheHelperMockedWhitelistedIncludesTests, GivenDisabledOrNullCacheWhenGettingCachingModeThenReturnsCachingModeNone) {
    CompilerCache cache(CompilerCacheConfig{false, "", "", 0});
    const StackVec<std::pair<const char *, IGC::CodeType::CodeType_t>, 3> testCases = {
        {"#include <whitelisted>\n__kernel void k() {}", IGC::CodeType::oclC},      // disabled cache
        {"__kernel void k() {}", IGC::CodeType::oclC},                              // disabled cache, no includes
        {"#include <unknown/unknown.h>\n__kernel void k() {}", IGC::CodeType::oclC} // disabled cache, non-whitelisted include
    };

    for (const auto &testCase : testCases) {
        ArrayRef<const char> sourceRef(testCase.first, strlen(testCase.first));
        EXPECT_EQ(CachingMode::none, CompilerCacheHelper::getCachingMode(&cache, testCase.second, sourceRef))
            << "Failed for source: " << testCase.first;
    }

    EXPECT_EQ(CachingMode::none, CompilerCacheHelper::getCachingMode(nullptr, IGC::CodeType::oclC, ArrayRef<const char>()))
        << "Failed for nullptr CompilerCache";
}

TEST_F(CompilerCacheHelperMockedWhitelistedIncludesTests, GivenEnabledCacheAndOclCWithWhitelistedIncludesWhenGettingCachingModeThenReturnsCachingModeDirect) {
    CompilerCache cache(CompilerCacheConfig{true, "", "", 0});
    const StackVec<std::pair<const char *, IGC::CodeType::CodeType_t>, 2> testCases = {
        {"__kernel void k() {}", IGC::CodeType::oclC},                        // no includes
        {"#include <whitelisted>\n__kernel void k() {}", IGC::CodeType::oclC} // only whitelisted include
    };

    for (const auto &testCase : testCases) {
        ArrayRef<const char> sourceRef(testCase.first, strlen(testCase.first));
        EXPECT_EQ(CachingMode::direct, CompilerCacheHelper::getCachingMode(&cache, testCase.second, sourceRef))
            << "Failed for source: " << testCase.first;
    }
}

TEST_F(CompilerCacheHelperMockedWhitelistedIncludesTests, GivenEnabledCacheAndNonWhitelistedIncludesOrNonOclCWhenGettingCachingModeThenReturnsCachingModePreProcess) {
    CompilerCache cache(CompilerCacheConfig{true, "", "", 0});
    const StackVec<std::pair<const char *, IGC::CodeType::CodeType_t>, 6> testCases = {
        {"#include <unknown/unknown.h>\n__kernel void k() {}", IGC::CodeType::oclC},                         // non-whitelisted include
        {"#include <whitelisted>\n#include <unknown/unknown.h>\n__kernel void k() {}", IGC::CodeType::oclC}, // mixed includes
        {"#include <unknown/unknown.h>\n#include <whitelisted>\n__kernel void k() {}", IGC::CodeType::oclC}, // mixed includes different order
        {"__kernel void k() {}", IGC::CodeType::spirV},                                                      // non-oclC type
        {"#include <whitelisted>\n__kernel void k() {}", IGC::CodeType::spirV},                              // non-oclC type with whitelisted include
        {"#include <unknown/unknown.h>\n__kernel void k() {}", IGC::CodeType::spirV}                         // non-oclC type with non-whitelisted include
    };

    for (const auto &testCase : testCases) {
        ArrayRef<const char> sourceRef(testCase.first, strlen(testCase.first));
        EXPECT_EQ(CachingMode::preProcess, CompilerCacheHelper::getCachingMode(&cache, testCase.second, sourceRef))
            << "Failed for source: " << testCase.first;
    }
}
