/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "runtime/execution_environment/execution_environment.h"
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

    {
        iOpenCL::SProgramBinaryHeader progBinHeader = {};
        progBinHeader.Magic = iOpenCL::MAGIC_CL;
        progBinHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
        progBinHeader.Device = renderCoreFamily;
        progBinHeader.GPUPointerSizeInBytes = 8;
        progBinHeader.NumberOfKernels = 1;
        progBinHeader.SteppingId = 0;
        progBinHeader.PatchListSize = 0;
        if (false == addUnhandledProgramScopePatchToken) {
            PushBackToken(ret, progBinHeader);
        } else {
            progBinHeader.PatchListSize = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
            PushBackToken(ret, progBinHeader);

            iOpenCL::SPatchItemHeader unhandledToken = {};
            unhandledToken.Size = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
            unhandledToken.Token = static_cast<uint32_t>(unhandledTokenId);
            PushBackToken(ret, unhandledToken);
        }
    }

    {
        std::string kernelName = "testKernel";
        while (kernelName.size() % 4 != 0) {
            // pad with \0 to get 4-byte size alignment of kernelName
            kernelName.push_back('\0');
        }
        iOpenCL::SKernelBinaryHeaderCommon kernBinHeader = {};
        kernBinHeader.CheckSum = 0U;
        kernBinHeader.ShaderHashCode = 0;
        kernBinHeader.KernelNameSize = static_cast<uint32_t>(kernelName.size());
        kernBinHeader.PatchListSize = 0;
        kernBinHeader.KernelHeapSize = 0;
        kernBinHeader.GeneralStateHeapSize = 0;
        kernBinHeader.DynamicStateHeapSize = 0;
        kernBinHeader.SurfaceStateHeapSize = 0;
        kernBinHeader.KernelUnpaddedSize = 0;

        auto headerOffset = ret.size();
        PushBackToken(ret, kernBinHeader);
        ret.insert(ret.end(), kernelName.begin(), kernelName.end());
        uint32_t patchListSize = 0;
        if (addUnhandledKernelScopePatchToken) {
            iOpenCL::SPatchItemHeader unhandledToken = {};
            unhandledToken.Size = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
            unhandledToken.Token = static_cast<uint32_t>(unhandledTokenId);
            PushBackToken(ret, unhandledToken);
            patchListSize = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
        }
        iOpenCL::SKernelBinaryHeaderCommon *kernHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(ret.data() + headerOffset);
        kernHeader->PatchListSize = patchListSize;
        auto kernelData = reinterpret_cast<const uint8_t *>(kernHeader);
        kernHeader->CheckSum = NEO::PatchTokenBinary::calcKernelChecksum(ArrayRef<const uint8_t>(kernelData, reinterpret_cast<const uint8_t *>(&*ret.rbegin()) + 1));
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
