/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/execution_environment/execution_environment.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "runtime/program/create.inl"
#include "runtime/program/program.h"

#include "gtest/gtest.h"

using namespace NEO;

extern GFXCORE_FAMILY renderCoreFamily;

template <typename ContainerT, typename TokenT>
inline void PushBackToken(ContainerT &container, const TokenT &token) {
    container.insert(container.end(), reinterpret_cast<const typename ContainerT::value_type *>(&token),
                     reinterpret_cast<const typename ContainerT::value_type *>(&token) + sizeof(token));
}

struct MockProgramRecordUnhandledTokens : public Program {
    bool allowUnhandledTokens;
    mutable int lastUnhandledTokenFound;

    MockProgramRecordUnhandledTokens(ExecutionEnvironment &executionEnvironment) : Program(executionEnvironment) {}
    MockProgramRecordUnhandledTokens(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltinKernel) : Program(executionEnvironment, context, isBuiltinKernel) {}

    bool isSafeToSkipUnhandledToken(unsigned int token) const override {
        lastUnhandledTokenFound = static_cast<int>(token);
        return allowUnhandledTokens;
    }

    bool getDefaultIsSafeToSkipUnhandledToken() const {
        return Program::isSafeToSkipUnhandledToken(iOpenCL::NUM_PATCH_TOKENS);
    }
};

inline cl_int GetDecodeErrorCode(const std::vector<char> &binary, bool allowUnhandledTokens,
                                 int defaultUnhandledTokenId, int &foundUnhandledTokenId) {
    NEO::ExecutionEnvironment executionEnvironment;
    using PT = MockProgramRecordUnhandledTokens;
    std::unique_ptr<PT> prog;
    cl_int errorCode = CL_INVALID_BINARY;
    prog.reset(NEO::Program::createFromGenBinary<PT>(executionEnvironment,
                                                     nullptr,
                                                     binary.data(),
                                                     binary.size(),
                                                     false, &errorCode));
    prog->allowUnhandledTokens = allowUnhandledTokens;
    prog->lastUnhandledTokenFound = defaultUnhandledTokenId;
    auto ret = prog->processGenBinary();
    foundUnhandledTokenId = prog->lastUnhandledTokenFound;
    return ret;
};

inline std::vector<char> CreateBinary(bool addUnhandledProgramScopePatchToken, bool addUnhandledKernelScopePatchToken,
                                      int32_t unhandledTokenId = static_cast<int32_t>(iOpenCL::NUM_PATCH_TOKENS)) {
    std::vector<char> ret;

    if (addUnhandledProgramScopePatchToken && addUnhandledKernelScopePatchToken) {
        return {};
    }

    if (addUnhandledProgramScopePatchToken) {
        PatchTokensTestData::ValidProgramWithConstantSurface programWithUnhandledToken;

        iOpenCL::SPatchItemHeader &unhandledToken = *programWithUnhandledToken.constSurfMutable;
        unhandledToken.Size += programWithUnhandledToken.constSurfMutable->InlineDataSize;
        unhandledToken.Token = static_cast<uint32_t>(unhandledTokenId);
        ret.assign(reinterpret_cast<char *>(programWithUnhandledToken.storage.data()),
                   reinterpret_cast<char *>(programWithUnhandledToken.storage.data() + programWithUnhandledToken.storage.size()));
    } else if (addUnhandledKernelScopePatchToken) {
        PatchTokensTestData::ValidProgramWithKernelAndArg programWithKernelWithUnhandledToken;
        iOpenCL::SPatchItemHeader &unhandledToken = *programWithKernelWithUnhandledToken.arg0InfoMutable;
        unhandledToken.Token = static_cast<uint32_t>(unhandledTokenId);
        programWithKernelWithUnhandledToken.recalcTokPtr();
        ret.assign(reinterpret_cast<char *>(programWithKernelWithUnhandledToken.storage.data()),
                   reinterpret_cast<char *>(programWithKernelWithUnhandledToken.storage.data() + programWithKernelWithUnhandledToken.storage.size()));
    } else {
        PatchTokensTestData::ValidProgramWithKernel regularProgramTokens;
        ret.assign(reinterpret_cast<char *>(regularProgramTokens.storage.data()), reinterpret_cast<char *>(regularProgramTokens.storage.data() + regularProgramTokens.storage.size()));
    }

    return ret;
}

constexpr int32_t unhandledTokenId = iOpenCL::NUM_PATCH_TOKENS;

TEST(EvaluateUnhandledToken, ByDefaultSkippingOfUnhandledTokensInUnitTestsIsSafe) {
    ExecutionEnvironment executionEnvironment;
    MockProgramRecordUnhandledTokens program(executionEnvironment);
    EXPECT_TRUE(program.getDefaultIsSafeToSkipUnhandledToken());
}

TEST(EvaluateUnhandledToken, WhenDecodingProgramBinaryIfAllTokensAreSupportedThenDecodingSucceeds) {
    int lastUnhandledTokenFound = -1;
    auto retVal = GetDecodeErrorCode(CreateBinary(false, false), false, -7, lastUnhandledTokenFound);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(-7, lastUnhandledTokenFound);
}

TEST(EvaluateUnhandledToken, WhenDecodingProgramBinaryIfUnhandledTokenIsFoundAndIsSafeToSkipThenDecodingSucceeds) {
    int lastUnhandledTokenFound = -1;
    auto retVal = GetDecodeErrorCode(CreateBinary(true, false, unhandledTokenId), true, -7, lastUnhandledTokenFound);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(unhandledTokenId, lastUnhandledTokenFound);
}

TEST(EvaluateUnhandledToken, WhenDecodingProgramBinaryIfUnhandledTokenIsFoundAndIsUnsafeToSkipThenDecodingFails) {
    int lastUnhandledTokenFound = -1;
    auto retVal = GetDecodeErrorCode(CreateBinary(true, false, unhandledTokenId), false, -7, lastUnhandledTokenFound);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_EQ(unhandledTokenId, lastUnhandledTokenFound);
}

TEST(EvaluateUnhandledToken, WhenDecodingKernelBinaryIfUnhandledTokenIsFoundAndIsSafeToSkipThenDecodingSucceeds) {
    int lastUnhandledTokenFound = -1;
    auto retVal = GetDecodeErrorCode(CreateBinary(false, true, unhandledTokenId), true, -7, lastUnhandledTokenFound);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(unhandledTokenId, lastUnhandledTokenFound);
}

TEST(EvaluateUnhandledToken, WhenDecodingKernelBinaryIfUnhandledTokenIsFoundAndIsUnsafeToSkipThenDecodingFails) {
    int lastUnhandledTokenFound = -1;
    auto retVal = GetDecodeErrorCode(CreateBinary(false, true, unhandledTokenId), false, -7, lastUnhandledTokenFound);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_EQ(unhandledTokenId, lastUnhandledTokenFound);
}
