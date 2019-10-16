/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/hash.h"
#include "core/helpers/string.h"
#include "runtime/compiler_interface/binary_cache.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/hw_info.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"

#include <array>
#include <list>
#include <memory>

using namespace NEO;
using namespace std;

class BinaryCacheFixture

{
  public:
    void SetUp() {
        cache = new BinaryCache;
    }

    void TearDown() {
        delete cache;
    }
    BinaryCache *cache;
};

class BinaryCacheMock : public BinaryCache {
  public:
    BinaryCacheMock() {
    }

    bool cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize) override {
        cacheInvoked++;
        return cacheResult;
    }

    std::unique_ptr<char[]> loadCachedBinary(const std::string kernelFileHash, size_t &cachedBinarySize) override {
        return loadResult ? std::unique_ptr<char[]>{new char[1]} : nullptr;
    }

    bool cacheResult = false;
    uint32_t cacheInvoked = 0u;
    bool loadResult = false;
};

class CompilerInterfaceCachedFixture : public DeviceFixture {
  public:
    void SetUp() {
        DeviceFixture::SetUp();
        pCompilerInterface = pDevice->getExecutionEnvironment()->getCompilerInterface();
        ASSERT_NE(pCompilerInterface, nullptr);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    CompilerInterface *pCompilerInterface;
};

typedef Test<BinaryCacheFixture> BinaryCacheHashTests;
typedef Test<BinaryCacheFixture> BinaryCacheTests;
typedef Test<CompilerInterfaceCachedFixture> CompilerInterfaceCachedTests;

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

TEST_F(BinaryCacheHashTests, hashShortBuffers) {
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

TEST_F(BinaryCacheHashTests, testUnique) {
    static const size_t bufSize = 64;
    TranslationInput args{IGC::CodeType::undefined, IGC::CodeType::undefined};
    HardwareInfo hwInfo;

    std::set<std::string> hashes;

    PLATFORM p1 = {(PRODUCT_FAMILY)1};
    PLATFORM p2 = {(PRODUCT_FAMILY)2};
    const PLATFORM *platforms[] = {&p1, &p2};
    FeatureTable s1;
    FeatureTable s2;
    s1.ftrSVM = true;
    s2.ftrSVM = false;
    const FeatureTable *skus[] = {&s1, &s2};
    WorkaroundTable w1;
    WorkaroundTable w2;
    w1.waDoNotUseMIReportPerfCount = true;
    w2.waDoNotUseMIReportPerfCount = false;
    const WorkaroundTable *was[] = {&w1, &w2};

    std::array<std::string, 4> input = {{std::string(""),
                                         std::string("12345678901234567890123456789012"),
                                         std::string("12345678910234567890123456789012"),
                                         std::string("12345678901234567891023456789012")}};

    std::array<std::string, 3> options = {{std::string(""),
                                           std::string("--some --options"),
                                           std::string("--some --different --options")}};

    std::array<std::string, 3> internalOptions = {{std::string(""),
                                                   std::string("--some --options"),
                                                   std::string("--some --different --options")}};

    std::array<std::string, 1> tracingOptions = {{
        std::string(""),
        //        std::string("--some --options"),
        //        std::string("--some --different --options")
    }};

    std::unique_ptr<char> buf1(new char[bufSize]);
    std::unique_ptr<char> buf2(new char[bufSize]);
    std::unique_ptr<char> buf3(new char[bufSize]);
    std::unique_ptr<char> buf4(new char[bufSize]);

    for (auto platform : platforms) {
        hwInfo.platform = *platform;

        for (auto sku : skus) {
            hwInfo.featureTable = *sku;

            for (auto wa : was) {
                hwInfo.workaroundTable = *wa;

                for (size_t i1 = 0; i1 < input.size(); i1++) {
                    strcpy_s(buf1.get(), bufSize, input[i1].c_str());
                    args.src = ArrayRef<char>(buf1.get(), strlen(buf1.get()));
                    for (size_t i2 = 0; i2 < options.size(); i2++) {
                        strcpy_s(buf2.get(), bufSize, options[i2].c_str());
                        args.apiOptions = ArrayRef<char>(buf2.get(), strlen(buf2.get()));
                        for (size_t i3 = 0; i3 < internalOptions.size(); i3++) {
                            strcpy_s(buf3.get(), bufSize, internalOptions[i3].c_str());
                            args.internalOptions = ArrayRef<char>(buf3.get(), strlen(buf3.get()));
                            for (size_t i4 = 0; i4 < tracingOptions.size(); i4++) {
                                strcpy_s(buf4.get(), bufSize, tracingOptions[i4].c_str());
                                args.tracingOptions = buf4.get();
                                args.tracingOptionsCount = static_cast<uint32_t>(strlen(buf4.get()));

                                string hash = cache->getCachedFileName(hwInfo, args.src, args.apiOptions, args.internalOptions);

                                if (hashes.find(hash) != hashes.end()) {
                                    FAIL() << "failed: " << i1 << ":" << i2 << ":" << i3 << ":" << i4;
                                }
                                hashes.emplace(hash);
                            }
                        }
                    }
                }
            }
        }
    }

    string hash = cache->getCachedFileName(hwInfo, args.src, args.apiOptions, args.internalOptions);
    string hash2 = cache->getCachedFileName(hwInfo, args.src, args.apiOptions, args.internalOptions);
    EXPECT_STREQ(hash.c_str(), hash2.c_str());
}

TEST_F(BinaryCacheTests, doNotCacheEmpty) {
    bool ret = cache->cacheBinary("some_hash", nullptr, 12u);
    EXPECT_FALSE(ret);

    const char *tmp1 = "Data";
    ret = cache->cacheBinary("some_hash", tmp1, 0u);
    EXPECT_FALSE(ret);
}

TEST_F(BinaryCacheTests, loadNotFound) {
    size_t size;
    auto ret = cache->loadCachedBinary("----do-not-exists----", size);
    EXPECT_EQ(nullptr, ret);
    EXPECT_EQ(0U, size);
}

TEST_F(BinaryCacheTests, cacheThenLoad) {
    static const char *hash = "SOME_HASH";
    std::unique_ptr<char> data(new char[32]);
    for (size_t i = 0; i < 32; i++)
        data.get()[i] = static_cast<char>(i);

    bool ret = cache->cacheBinary(hash, static_cast<const char *>(data.get()), 32);
    EXPECT_TRUE(ret);

    size_t size;
    auto loadedBin = cache->loadCachedBinary(hash, size);
    EXPECT_NE(nullptr, loadedBin);
    EXPECT_NE(0U, size);
}

TEST_F(CompilerInterfaceCachedTests, canInjectCache) {
    std::unique_ptr<BinaryCache> cache(new BinaryCache());
    auto res1 = pCompilerInterface->replaceBinaryCache(cache.get());
    auto res2 = pCompilerInterface->replaceBinaryCache(res1);

    EXPECT_NE(res1, res2);
    EXPECT_EQ(res2, cache.get());
}
TEST_F(CompilerInterfaceCachedTests, notCachedAndIgcFailed) {
    BinaryCacheMock cache;
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

    auto res1 = pCompilerInterface->replaceBinaryCache(&cache);

    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, err);

    pCompilerInterface->replaceBinaryCache(res1);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceCachedTests, wasCached) {
    BinaryCacheMock cache;
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

    auto res1 = pCompilerInterface->replaceBinaryCache(&cache);
    cache.loadResult = true;
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);

    pCompilerInterface->replaceBinaryCache(res1);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceCachedTests, builtThenCached) {
    BinaryCacheMock cache;
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"header.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    gEnvironment->igcPushDebugVars(igcDebugVars);

    auto res1 = pCompilerInterface->replaceBinaryCache(&cache);
    TranslationOutput translationOutput = {};
    inputArgs.allowCaching = true;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_EQ(1u, cache.cacheInvoked);

    pCompilerInterface->replaceBinaryCache(res1);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceCachedTests, givenKernelWithoutIncludesAndBinaryInCacheWhenCompilationRequestedThenFCLIsNotCalled) {
    MockContext context(pDevice, true);
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);
    BinaryCacheMock cache;
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

    auto res = pCompilerInterface->replaceBinaryCache(&cache);
    cache.loadResult = true;
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto retVal = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, retVal);

    pCompilerInterface->replaceBinaryCache(res);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceCachedTests, givenKernelWithIncludesAndBinaryInCacheWhenCompilationRequestedThenFCLIsCalled) {
    MockContext context(pDevice, true);
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);
    BinaryCacheMock cache;
    TranslationInput inputArgs{IGC::CodeType::oclC, IGC::CodeType::oclGenBin};

    auto src = "#include \"file.h\"\n__kernel k() {}";
    inputArgs.src = ArrayRef<const char>(src, strlen(src));

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    auto res = pCompilerInterface->replaceBinaryCache(&cache);
    cache.loadResult = true;
    TranslationOutput translationOutput;
    inputArgs.allowCaching = true;
    auto retVal = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, retVal);

    pCompilerInterface->replaceBinaryCache(res);

    gEnvironment->fclPopDebugVars();
}
