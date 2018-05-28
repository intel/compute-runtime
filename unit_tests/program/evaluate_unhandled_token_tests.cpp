/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gtest/gtest.h"
#include "runtime/program/program.h"

extern GFXCORE_FAMILY renderCoreFamily;

template <typename ContainerT, typename TokenT>
inline void PushBackToken(ContainerT &container, const TokenT &token) {
    container.insert(container.end(), reinterpret_cast<const typename ContainerT::value_type *>(&token),
                     reinterpret_cast<const typename ContainerT::value_type *>(&token) + sizeof(token));
}

struct MockProgramRecordUnhandledTokens : OCLRT::Program {
    bool allowUnhandledTokens;
    mutable int lastUnhandledTokenFound;

    using Program::Program;

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
    using PT = MockProgramRecordUnhandledTokens;
    std::unique_ptr<PT> prog;
    cl_int errorCode = CL_INVALID_BINARY;
    prog.reset(OCLRT::Program::createFromGenBinary<PT>(nullptr,
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
        kernBinHeader.CheckSum = 0;
        kernBinHeader.ShaderHashCode = 0;
        kernBinHeader.KernelNameSize = static_cast<uint32_t>(kernelName.size());
        kernBinHeader.PatchListSize = 0;
        kernBinHeader.KernelHeapSize = 0;
        kernBinHeader.GeneralStateHeapSize = 0;
        kernBinHeader.DynamicStateHeapSize = 0;
        kernBinHeader.SurfaceStateHeapSize = 0;
        kernBinHeader.KernelUnpaddedSize = 0;

        if (false == addUnhandledKernelScopePatchToken) {
            PushBackToken(ret, kernBinHeader);
            ret.insert(ret.end(), kernelName.begin(), kernelName.end());
        } else {
            kernBinHeader.PatchListSize = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
            PushBackToken(ret, kernBinHeader);
            ret.insert(ret.end(), kernelName.begin(), kernelName.end());

            iOpenCL::SPatchItemHeader unhandledToken = {};
            unhandledToken.Size = static_cast<uint32_t>(sizeof(iOpenCL::SPatchItemHeader));
            unhandledToken.Token = static_cast<uint32_t>(unhandledTokenId);
            PushBackToken(ret, unhandledToken);
        }
    }

    return ret;
}

constexpr int32_t unhandledTokenId = iOpenCL::NUM_PATCH_TOKENS;

TEST(EvaluateUnhandledToken, ByDefaultSkippingOfUnhandledTokensInUnitTestsIsSafe) {
    MockProgramRecordUnhandledTokens program;
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
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    EXPECT_EQ(unhandledTokenId, lastUnhandledTokenFound);
}
