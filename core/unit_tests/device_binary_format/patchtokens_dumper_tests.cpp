/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/device_binary_format/patchtokens_dumper.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "test.h"

#include <sstream>

TEST(ProgramDumper, GivenEmptyProgramThenProperlyCreatesDumpStringWithWarnig) {
    NEO::PatchTokenBinary::ProgramFromPatchtokens emptyProgram = {};
    emptyProgram.decodeStatus = NEO::PatchTokenBinary::DecoderError::Undefined;
    std::string generated = NEO::PatchTokenBinary::asString(emptyProgram);
    const char *expected =
        R"===(Program of size : 0 in undefined status
WARNING : Program header is missing
Program-scope tokens section size : 0
Kernels section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());

    emptyProgram.decodeStatus = NEO::PatchTokenBinary::DecoderError::InvalidBinary;
    generated = NEO::PatchTokenBinary::asString(emptyProgram);
    expected =
        R"===(Program of size : 0 with invalid binary
WARNING : Program header is missing
Program-scope tokens section size : 0
Kernels section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());

    emptyProgram.decodeStatus = NEO::PatchTokenBinary::DecoderError::Success;
    generated = NEO::PatchTokenBinary::asString(emptyProgram);
    expected =
        R"===(Program of size : 0 decoded successfully
WARNING : Program header is missing
Program-scope tokens section size : 0
Kernels section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());
}

TEST(KernelDumper, GivenEmptyKernelThenProperlyCreatesDumpStringWithWarnig) {
    NEO::PatchTokenBinary::KernelFromPatchtokens emptyKernel = {};
    emptyKernel.decodeStatus = NEO::PatchTokenBinary::DecoderError::Undefined;
    std::string generated = NEO::PatchTokenBinary::asString(emptyKernel);
    const char *expected =
        R"===(Kernel of size : 0  in undefined status
WARNING : Kernel header is missing
Kernel-scope tokens section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());

    emptyKernel.decodeStatus = NEO::PatchTokenBinary::DecoderError::InvalidBinary;
    generated = NEO::PatchTokenBinary::asString(emptyKernel);
    expected =
        R"===(Kernel of size : 0  with invalid binary
WARNING : Kernel header is missing
Kernel-scope tokens section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());

    emptyKernel.decodeStatus = NEO::PatchTokenBinary::DecoderError::Success;
    generated = NEO::PatchTokenBinary::asString(emptyKernel);
    expected =
        R"===(Kernel of size : 0  decoded successfully
WARNING : Kernel header is missing
Kernel-scope tokens section size : 0
)===";
    EXPECT_STREQ(expected, generated.c_str());
}

TEST(KernelArgDumper, GivenEmptyKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens emptyKernelArg = {};
    std::string generated = NEO::PatchTokenBinary::asString(emptyKernelArg, "");
    const char *expected =
        R"===(Kernel argument of type unspecified
)===";
    EXPECT_STREQ(expected, generated.c_str());
}

TEST(ProgramDumper, GivenProgramWithPatchtokensThenProperlyCreatesDump) {
    using namespace iOpenCL;
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer progWithConst = {};
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer progWithGlobal = {};
    SPatchAllocateConstantMemorySurfaceProgramBinaryInfo constSurf2 = *progWithConst.programScopeTokens.allocateConstantMemorySurface[0];
    constSurf2.ConstantBufferIndex += 1;
    constSurf2.InlineDataSize *= 2;
    progWithConst.programScopeTokens.allocateConstantMemorySurface.push_back(&constSurf2);
    SPatchConstantPointerProgramBinaryInfo constPointer2 = *progWithConst.programScopeTokens.constantPointer[0];
    constPointer2.BufferIndex = 1;
    constPointer2.ConstantPointerOffset += 8;
    constPointer2.ConstantBufferIndex = 1;
    progWithConst.programScopeTokens.constantPointer.push_back(&constPointer2);

    progWithConst.programScopeTokens.allocateGlobalMemorySurface.push_back(progWithGlobal.programScopeTokens.allocateGlobalMemorySurface[0]);
    SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo globSurf2 = *progWithGlobal.programScopeTokens.allocateGlobalMemorySurface[0];
    globSurf2.GlobalBufferIndex += 2;
    globSurf2.InlineDataSize *= 3;
    progWithConst.programScopeTokens.allocateGlobalMemorySurface.push_back(&globSurf2);
    progWithConst.programScopeTokens.globalPointer.push_back(progWithGlobal.programScopeTokens.globalPointer[0]);
    SPatchGlobalPointerProgramBinaryInfo globPointer2 = *progWithGlobal.programScopeTokens.globalPointer[0];
    globPointer2.GlobalPointerOffset += 8;
    progWithConst.programScopeTokens.globalPointer.push_back(&globPointer2);
    SPatchGlobalPointerProgramBinaryInfo globPointer3 = globPointer2;
    globPointer3.GlobalPointerOffset += 8;
    progWithConst.programScopeTokens.globalPointer.push_back(&globPointer3);
    iOpenCL::SPatchFunctionTableInfo symbolTable;
    symbolTable.Token = iOpenCL::PATCH_TOKEN_PROGRAM_SYMBOL_TABLE;
    symbolTable.Size = sizeof(iOpenCL::SPatchFunctionTableInfo);
    symbolTable.NumEntries = 7;
    progWithConst.programScopeTokens.symbolTable = &symbolTable;

    auto unknownToken0 = globPointer2;
    unknownToken0.Token = NUM_PATCH_TOKENS;
    progWithConst.unhandledTokens.push_back(&unknownToken0);

    auto unknownToken1 = globPointer2;
    unknownToken1.Token = NUM_PATCH_TOKENS;
    progWithConst.unhandledTokens.push_back(&unknownToken1);

    std::string generated = NEO::PatchTokenBinary::asString(progWithConst);
    std::stringstream expected;
    expected <<
        R"===(Program of size : )===" << progWithConst.blobs.programInfo.size() << R"===( decoded successfully
struct SProgramBinaryHeader {
    uint32_t   Magic; // = 1229870147
    uint32_t   Version; // = )==="
             << CURRENT_ICBE_VERSION << R"===(

    uint32_t   Device; // = )==="
             << renderCoreFamily << R"===(
    uint32_t   GPUPointerSizeInBytes; // = )==="
             << progWithConst.header->GPUPointerSizeInBytes << R"===(

    uint32_t   NumberOfKernels; // = 0

    uint32_t   SteppingId; // = 0

    uint32_t   PatchListSize; // = )==="
             << progWithConst.header->PatchListSize << R"===(
};
Program-scope tokens section size : )==="
             << progWithConst.blobs.patchList.size() << R"===(
  WARNING : Unhandled program-scope tokens detected [2] :
   + [0]:
   |  struct SPatchItemHeader {
   |      uint32_t   Token;// = )==="
             << NUM_PATCH_TOKENS << R"===(
   |      uint32_t   Size;// = )==="
             << progWithConst.unhandledTokens[0]->Size << R"===(
   |  };
   + [1]:
   |  struct SPatchItemHeader {
   |      uint32_t   Token;// = )==="
             << (NUM_PATCH_TOKENS) << R"===(
   |      uint32_t   Size;// = )==="
             << progWithConst.unhandledTokens[1]->Size << R"===(
   |  };
  Inline Costant Surface(s) [2] :
   + [0]:
   |  struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :
   |         SPatchItemHeader (Token=42(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   ConstantBufferIndex;// = 0
   |      uint32_t   InlineDataSize;// = 128
   |  }
   + [1]:
   |  struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :
   |         SPatchItemHeader (Token=42(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   ConstantBufferIndex;// = 1
   |      uint32_t   InlineDataSize;// = 256
   |  }
  Inline Costant Surface - self relocations [2] :
   + [0]:
   |  struct SPatchConstantPointerProgramBinaryInfo :
   |         SPatchItemHeader (Token=48(PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchConstantPointerProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   ConstantBufferIndex;// = 0
   |      uint64_t   ConstantPointerOffset;// = 96
   |      uint32_t   BufferType;// = 1
   |      uint32_t   BufferIndex;// = 0
   |  }
   + [1]:
   |  struct SPatchConstantPointerProgramBinaryInfo :
   |         SPatchItemHeader (Token=48(PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchConstantPointerProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   ConstantBufferIndex;// = 1
   |      uint64_t   ConstantPointerOffset;// = 104
   |      uint32_t   BufferType;// = 1
   |      uint32_t   BufferIndex;// = 1
   |  }
  Inline Global Variable Surface(s) [2] :
   + [0]:
   |  struct SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo :
   |         SPatchItemHeader (Token=41(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   Type;// = 0
   |      uint32_t   GlobalBufferIndex;// = 0
   |      uint32_t   InlineDataSize;// = 256
   |  }
   + [1]:
   |  struct SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo :
   |         SPatchItemHeader (Token=41(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   Type;// = 0
   |      uint32_t   GlobalBufferIndex;// = 2
   |      uint32_t   InlineDataSize;// = 768
   |  }
  Inline Global Variable Surface - self relocations [3] :
   + [0]:
   |  struct SPatchGlobalPointerProgramBinaryInfo :
   |         SPatchItemHeader (Token=47(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchGlobalPointerProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   GlobalBufferIndex;// = 0
   |      uint64_t   GlobalPointerOffset;// = 48
   |      uint32_t   BufferType;// = 0
   |      uint32_t   BufferIndex;// = 0
   |  }
   + [1]:
   |  struct SPatchGlobalPointerProgramBinaryInfo :
   |         SPatchItemHeader (Token=47(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchGlobalPointerProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   GlobalBufferIndex;// = 0
   |      uint64_t   GlobalPointerOffset;// = 56
   |      uint32_t   BufferType;// = 0
   |      uint32_t   BufferIndex;// = 0
   |  }
   + [2]:
   |  struct SPatchGlobalPointerProgramBinaryInfo :
   |         SPatchItemHeader (Token=47(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO), Size=)==="
             << sizeof(SPatchGlobalPointerProgramBinaryInfo) << R"===()
   |  {
   |      uint32_t   GlobalBufferIndex;// = 0
   |      uint64_t   GlobalPointerOffset;// = 64
   |      uint32_t   BufferType;// = 0
   |      uint32_t   BufferIndex;// = 0
   |  }
  struct SPatchFunctionTableInfo :
         SPatchItemHeader (Token=53(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE), Size=)==="
             << sizeof(SPatchFunctionTableInfo) << R"===()
  {
      uint32_t   NumEntries;// = 7
  }
Kernels section size : 0
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(ProgramDumper, GivenProgramWithKernelThenProperlyCreatesDump) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm program;
    std::string generated = NEO::PatchTokenBinary::asString(program);
    std::stringstream expected;
    expected <<
        R"===(Program of size : )===" << program.blobs.programInfo.size() << R"===( decoded successfully
struct SProgramBinaryHeader {
    uint32_t   Magic; // = 1229870147
    uint32_t   Version; // = )==="
             << iOpenCL::CURRENT_ICBE_VERSION << R"===(

    uint32_t   Device; // = )==="
             << renderCoreFamily << R"===(
    uint32_t   GPUPointerSizeInBytes; // = )==="
             << program.header->GPUPointerSizeInBytes << R"===(

    uint32_t   NumberOfKernels; // = 1

    uint32_t   SteppingId; // = 0

    uint32_t   PatchListSize; // = 0
};
Program-scope tokens section size : 0
Kernels section size : 0
kernel[0] test_kernel:
Kernel of size : )==="
             << program.kernels[0].blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << program.kernels[0].header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << program.kernels[0].header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << program.kernels[0].blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
  struct SPatchAllocateLocalSurface :
         SPatchItemHeader (Token=15(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE), Size=)==="
             << sizeof(iOpenCL::SPatchAllocateLocalSurface) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   TotalInlineLocalMemorySize;// = 16
  }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(ProgramDumper, GivenProgramWithMultipleKerneslThenProperlyCreatesDump) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm program;
    program.kernels.push_back(program.kernels[0]);
    program.kernels[1].tokens.allocateLocalSurface = nullptr;
    program.kernels[1].name = ArrayRef<const char>("different_kernel");
    program.kernels[1].blobs.patchList = ArrayRef<const uint8_t>();
    program.kernels.push_back(program.kernels[1]);
    program.kernels[2].name = ArrayRef<const char>();
    std::string generated = NEO::PatchTokenBinary::asString(program);
    std::stringstream expected;
    expected <<
        R"===(Program of size : )===" << program.blobs.programInfo.size() << R"===( decoded successfully
struct SProgramBinaryHeader {
    uint32_t   Magic; // = 1229870147
    uint32_t   Version; // = )==="
             << iOpenCL::CURRENT_ICBE_VERSION << R"===(

    uint32_t   Device; // = )==="
             << renderCoreFamily << R"===(
    uint32_t   GPUPointerSizeInBytes; // = )==="
             << program.header->GPUPointerSizeInBytes << R"===(

    uint32_t   NumberOfKernels; // = 1

    uint32_t   SteppingId; // = 0

    uint32_t   PatchListSize; // = 0
};
Program-scope tokens section size : 0
Kernels section size : 0
kernel[0] test_kernel:
Kernel of size : )==="
             << program.kernels[0].blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << program.kernels[0].header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << program.kernels[0].header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << program.kernels[0].blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
  struct SPatchAllocateLocalSurface :
         SPatchItemHeader (Token=15(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE), Size=)==="
             << sizeof(iOpenCL::SPatchAllocateLocalSurface) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   TotalInlineLocalMemorySize;// = 16
  }
kernel[1] different_kernel:
Kernel of size : )==="
             << program.kernels[1].blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << program.kernels[1].header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << program.kernels[1].header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << program.kernels[1].blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
kernel[2] <UNNAMED>:
Kernel of size : )==="
             << program.kernels[2].blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << program.kernels[2].header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << program.kernels[2].header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << program.kernels[2].blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelDumper, GivenKernelWithNonCrossthreadDataPatchtokensThenProperlyCreatesDump) {
    using namespace iOpenCL;
    using namespace PatchTokensTestData;
    std::vector<uint8_t> stream;
    auto kernel = ValidEmptyKernel::create(stream);
    auto samplerStateArray = initToken<SPatchSamplerStateArray>(PATCH_TOKEN_SAMPLER_STATE_ARRAY);
    auto bindingTableState = initToken<SPatchBindingTableState>(PATCH_TOKEN_BINDING_TABLE_STATE);
    auto allocateLocalSurface = initToken<SPatchAllocateLocalSurface>(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE);
    SPatchMediaVFEState mediaVfeState[2] = {initToken<SPatchMediaVFEState>(PATCH_TOKEN_MEDIA_VFE_STATE), initToken<SPatchMediaVFEState>(PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1)};
    auto mediaInterfaceDescriptorLoad = initToken<SPatchMediaInterfaceDescriptorLoad>(PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    auto interfaceDescriptorData = initToken<SPatchInterfaceDescriptorData>(PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA);
    auto threadPayload = initToken<SPatchThreadPayload>(PATCH_TOKEN_THREAD_PAYLOAD);
    auto executionEnvironment = initToken<SPatchExecutionEnvironment>(PATCH_TOKEN_EXECUTION_ENVIRONMENT);
    auto dataParameterStream = initToken<SPatchDataParameterStream>(PATCH_TOKEN_DATA_PARAMETER_STREAM);
    auto kernelAttributesInfo = initToken<SPatchKernelAttributesInfo>(PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO);
    auto allocateStatelessPrivateSurface = initToken<SPatchAllocateStatelessPrivateSurface>(PATCH_TOKEN_ALLOCATE_PRIVATE_MEMORY);
    auto allocateStatelessConstantMemorySurfaceWithInitialization = initToken<SPatchAllocateStatelessConstantMemorySurfaceWithInitialization>(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION);
    auto allocateStatelessGlobalMemorySurfaceWithInitialization = initToken<SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization>(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION);
    auto allocateStatelessPrintfSurface = initToken<SPatchAllocateStatelessPrintfSurface>(PATCH_TOKEN_ALLOCATE_PRINTF_SURFACE);
    auto allocateStatelessEventPoolSurface = initToken<SPatchAllocateStatelessEventPoolSurface>(PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE);
    auto allocateStatelessDefaultDeviceQueueSurface = initToken<SPatchAllocateStatelessDefaultDeviceQueueSurface>(PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT);
    auto inlineVmeSamplerInfo = initToken<SPatchItemHeader>(PATCH_TOKEN_INLINE_VME_SAMPLER_INFO);
    auto gtpinFreeGrfInfo = initToken<SPatchGtpinFreeGRFInfo>(PATCH_TOKEN_GTPIN_FREE_GRF_INFO);
    auto stateSip = initToken<SPatchStateSIP>(PATCH_TOKEN_STATE_SIP);
    auto allocateSystemThreadSurface = initToken<SPatchAllocateSystemThreadSurface>(PATCH_TOKEN_ALLOCATE_SIP_SURFACE);
    auto gtpinInfo = initToken<SPatchItemHeader>(PATCH_TOKEN_GTPIN_INFO);
    auto programSymbolTable = initToken<SPatchFunctionTableInfo>(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE);
    auto programRelocationTable = initToken<SPatchFunctionTableInfo>(PATCH_TOKEN_PROGRAM_RELOCATION_TABLE);
    auto unknownToken0 = initToken<SPatchItemHeader>(NUM_PATCH_TOKENS);
    auto unknownToken1 = initToken<SPatchItemHeader>(NUM_PATCH_TOKENS);

    kernel.tokens.samplerStateArray = &samplerStateArray;
    kernel.tokens.bindingTableState = &bindingTableState;
    kernel.tokens.allocateLocalSurface = &allocateLocalSurface;
    kernel.tokens.mediaVfeState[0] = &mediaVfeState[0];
    kernel.tokens.mediaVfeState[1] = &mediaVfeState[1];
    kernel.tokens.mediaInterfaceDescriptorLoad = &mediaInterfaceDescriptorLoad;
    kernel.tokens.interfaceDescriptorData = &interfaceDescriptorData;
    kernel.tokens.threadPayload = &threadPayload;
    kernel.tokens.executionEnvironment = &executionEnvironment;
    kernel.tokens.dataParameterStream = &dataParameterStream;
    kernel.tokens.kernelAttributesInfo = &kernelAttributesInfo;
    kernel.tokens.allocateStatelessPrivateSurface = &allocateStatelessPrivateSurface;
    kernel.tokens.allocateStatelessConstantMemorySurfaceWithInitialization = &allocateStatelessConstantMemorySurfaceWithInitialization;
    kernel.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization = &allocateStatelessGlobalMemorySurfaceWithInitialization;
    kernel.tokens.allocateStatelessPrintfSurface = &allocateStatelessPrintfSurface;
    kernel.tokens.allocateStatelessEventPoolSurface = &allocateStatelessEventPoolSurface;
    kernel.tokens.allocateStatelessDefaultDeviceQueueSurface = &allocateStatelessDefaultDeviceQueueSurface;
    kernel.tokens.inlineVmeSamplerInfo = &inlineVmeSamplerInfo;
    kernel.tokens.gtpinFreeGrfInfo = &gtpinFreeGrfInfo;
    kernel.tokens.stateSip = &stateSip;
    kernel.tokens.allocateSystemThreadSurface = &allocateSystemThreadSurface;
    kernel.tokens.gtpinInfo = &gtpinInfo;
    kernel.tokens.programSymbolTable = &programSymbolTable;
    kernel.tokens.programRelocationTable = &programRelocationTable;
    kernel.unhandledTokens.push_back(&unknownToken0);
    kernel.unhandledTokens.push_back(&unknownToken1);

    std::string generated = NEO::PatchTokenBinary::asString(kernel);
    std::stringstream expected;
    expected <<
        R"===(Kernel of size : )===" << kernel.blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << kernel.header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << kernel.header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << kernel.blobs.patchList.size() << R"===(
  WARNING : Unhandled kernel-scope tokens detected [2] :
   + [0]:
   |  struct SPatchItemHeader {
   |      uint32_t   Token;// = )==="
             << NUM_PATCH_TOKENS << R"===(
   |      uint32_t   Size;// = 8
   |  };
   + [1]:
   |  struct SPatchItemHeader {
   |      uint32_t   Token;// = )==="
             << NUM_PATCH_TOKENS << R"===(
   |      uint32_t   Size;// = 8
   |  };
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 0
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 0
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
  struct SPatchThreadPayload :
         SPatchItemHeader (Token=22(PATCH_TOKEN_THREAD_PAYLOAD), Size=)==="
             << sizeof(SPatchThreadPayload) << R"===()
  {
      uint32_t    HeaderPresent;// = 0
      uint32_t    LocalIDXPresent;// = 0
      uint32_t    LocalIDYPresent;// = 0
      uint32_t    LocalIDZPresent;// = 0
      uint32_t    LocalIDFlattenedPresent;// = 0
      uint32_t    IndirectPayloadStorage;// = 0
      uint32_t    UnusedPerThreadConstantPresent;// = 0
      uint32_t    GetLocalIDPresent;// = 0
      uint32_t    GetGroupIDPresent;// = 0
      uint32_t    GetGlobalOffsetPresent;// = 0
      uint32_t    StageInGridOriginPresent;// = 0
      uint32_t    StageInGridSizePresent;// = 0
      uint32_t    OffsetToSkipPerThreadDataLoad;// = 0
      uint32_t    OffsetToSkipSetFFIDGP;// = 0
      uint32_t    PassInlineData;// = 0
  }
  struct SPatchSamplerStateArray :
         SPatchItemHeader (Token=5(PATCH_TOKEN_SAMPLER_STATE_ARRAY), Size=)==="
             << sizeof(SPatchSamplerStateArray) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   Count;// = 0
      uint32_t   BorderColorOffset;// = 0
  }
  struct SPatchBindingTableState :
         SPatchItemHeader (Token=8(PATCH_TOKEN_BINDING_TABLE_STATE), Size=)==="
             << sizeof(SPatchBindingTableState) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   Count;// = 0
      uint32_t   SurfaceStateOffset;// = 0
  }
  struct SPatchAllocateLocalSurface :
         SPatchItemHeader (Token=15(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE), Size=)==="
             << sizeof(SPatchAllocateLocalSurface) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   TotalInlineLocalMemorySize;// = 0
  }
  mediaVfeState [2] :
   + [0]:
   |  struct SPatchMediaVFEState :
   |         SPatchItemHeader (Token=18(PATCH_TOKEN_MEDIA_VFE_STATE), Size=)==="
             << sizeof(SPatchMediaVFEState) << R"===()
   |  {
   |      uint32_t   ScratchSpaceOffset;// = 0
   |      uint32_t   PerThreadScratchSpace;// = 0
   |  }
   + [1]:
   |  struct SPatchMediaVFEState :
   |         SPatchItemHeader (Token=55(PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1), Size=)==="
             << sizeof(SPatchMediaVFEState) << R"===()
   |  {
   |      uint32_t   ScratchSpaceOffset;// = 0
   |      uint32_t   PerThreadScratchSpace;// = 0
   |  }
  struct SPatchMediaInterfaceDescriptorLoad :
         SPatchItemHeader (Token=19(PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD), Size=)==="
             << sizeof(SPatchMediaInterfaceDescriptorLoad) << R"===()
  {
      uint32_t   InterfaceDescriptorDataOffset;// = 0
  }
  struct SPatchInterfaceDescriptorData :
         SPatchItemHeader (Token=21(PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA), Size=)==="
             << sizeof(SPatchInterfaceDescriptorData) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   SamplerStateOffset;// = 0
      uint32_t   KernelOffset;// = 0
      uint32_t   BindingTableOffset;// = 0
  }
  struct SPatchKernelAttributesInfo :
         SPatchItemHeader (Token=27(PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO), Size=)==="
             << sizeof(SPatchKernelAttributesInfo) << R"===()
  {
      uint32_t AttributesSize;// = 0
  }
  struct SPatchAllocateStatelessPrivateSurface :
         SPatchItemHeader (Token=24(PATCH_TOKEN_ALLOCATE_PRIVATE_MEMORY), Size=)==="
             << sizeof(SPatchAllocateStatelessPrivateSurface) << R"===()
  {
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
      uint32_t   PerThreadPrivateMemorySize;// = 0
  }
  struct SPatchAllocateStatelessConstantMemorySurfaceWithInitialization :
         SPatchItemHeader (Token=39(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION), Size=)==="
             << sizeof(SPatchAllocateStatelessConstantMemorySurfaceWithInitialization) << R"===()
  {
      uint32_t   ConstantBufferIndex;// = 0
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
  }
  struct SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization :
         SPatchItemHeader (Token=40(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION), Size=)==="
             << sizeof(SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization) << R"===()
  {
      uint32_t   GlobalBufferIndex;// = 0
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
  }
  struct SPatchAllocateStatelessPrintfSurface :
         SPatchItemHeader (Token=29(PATCH_TOKEN_ALLOCATE_PRINTF_SURFACE), Size=)==="
             << sizeof(SPatchAllocateStatelessPrintfSurface) << R"===()
  {
      uint32_t   PrintfSurfaceIndex;// = 0
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
  }
  struct SPatchAllocateStatelessEventPoolSurface :
         SPatchItemHeader (Token=36(PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE), Size=)==="
             << sizeof(SPatchAllocateStatelessEventPoolSurface) << R"===()
  {
      uint32_t   EventPoolSurfaceIndex;// = 0
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
  }
  struct SPatchAllocateStatelessDefaultDeviceQueueSurface :
         SPatchItemHeader (Token=46(PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT), Size=)==="
             << sizeof(SPatchAllocateStatelessDefaultDeviceQueueSurface) << R"===()
  {
      uint32_t   SurfaceStateHeapOffset;// = 0
      uint32_t   DataParamOffset;// = 0
      uint32_t   DataParamSize;// = 0
  }
  struct SPatchItemHeader {
      uint32_t   Token;// = 50(PATCH_TOKEN_INLINE_VME_SAMPLER_INFO)
      uint32_t   Size;// = )==="
             << sizeof(SPatchItemHeader) << R"===(
  };
  struct SPatchGtpinFreeGRFInfo :
         SPatchItemHeader (Token=51(PATCH_TOKEN_GTPIN_FREE_GRF_INFO), Size=)==="
             << sizeof(SPatchGtpinFreeGRFInfo) << R"===()
  {
      uint32_t   BufferSize;// = 0
  }
  struct SPatchStateSIP :
         SPatchItemHeader (Token=2(PATCH_TOKEN_STATE_SIP), Size=)==="
             << sizeof(SPatchStateSIP) << R"===()
  {
      uint32_t   SystemKernelOffset;// = 0
  }
  struct SPatchAllocateSystemThreadSurface :
         SPatchItemHeader (Token=10(PATCH_TOKEN_ALLOCATE_SIP_SURFACE), Size=)==="
             << sizeof(SPatchAllocateSystemThreadSurface) << R"===()
  {
      uint32_t   Offset;// = 0
      uint32_t   PerThreadSystemThreadSurfaceSize;// = 0
      uint32_t   BTI;// = 0
  }
  struct SPatchItemHeader {
      uint32_t   Token;// = 52(PATCH_TOKEN_GTPIN_INFO)
      uint32_t   Size;// = )==="
             << sizeof(SPatchItemHeader) << R"===(
  };
  struct SPatchFunctionTableInfo :
         SPatchItemHeader (Token=53(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE), Size=)==="
             << sizeof(SPatchFunctionTableInfo) << R"===()
  {
      uint32_t   NumEntries;// = 0
  }
  struct SPatchFunctionTableInfo :
         SPatchItemHeader (Token=54(PATCH_TOKEN_PROGRAM_RELOCATION_TABLE), Size=)==="
             << sizeof(SPatchFunctionTableInfo) << R"===()
  {
      uint32_t   NumEntries;// = 0
  }
  struct SPatchDataParameterStream :
         SPatchItemHeader (Token=25(PATCH_TOKEN_DATA_PARAMETER_STREAM), Size=)==="
             << sizeof(SPatchDataParameterStream) << R"===()
  {
      uint32_t   DataParameterStreamSize;// = 0
  }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelDumper, GivenKernelWithStringPatchTokensThenProperlyCreatesDump) {
    std::vector<uint8_t> kernelStream;
    auto kernel = PatchTokensTestData::ValidEmptyKernel::create(kernelStream);

    std::vector<uint8_t> strTokStream;

    std::string str0{"some_string0"};
    std::string str1{"another_string"};
    std::string str2{"yet_another_string"};
    auto string0Off = PatchTokensTestData::pushBackStringToken(str0, 0, strTokStream);
    auto string1Off = PatchTokensTestData::pushBackStringToken(str1, 1, strTokStream);
    auto string2Off = PatchTokensTestData::pushBackStringToken(str2, 2, strTokStream);

    kernel.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string0Off));
    kernel.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string1Off));
    kernel.tokens.strings.push_back(reinterpret_cast<iOpenCL::SPatchString *>(strTokStream.data() + string2Off));
    std::string generated = NEO::PatchTokenBinary::asString(kernel);
    std::stringstream expected;
    expected << R"===(Kernel of size : )===" << kernel.blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << kernel.header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << kernel.header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << kernel.blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
  String literals [3] :
   + [0]:
   |  struct SPatchString :
   |         SPatchItemHeader (Token=28(PATCH_TOKEN_STRING), Size=)==="
             << (sizeof(iOpenCL::SPatchString) + str0.length()) << R"===()
   |  {
   |      uint32_t   Index;// = 0
   |      uint32_t   StringSize;// = 12 : [some_string0]
   |  }
   + [1]:
   |  struct SPatchString :
   |         SPatchItemHeader (Token=28(PATCH_TOKEN_STRING), Size=)==="
             << (sizeof(iOpenCL::SPatchString) + str1.length()) << R"===()
   |  {
   |      uint32_t   Index;// = 1
   |      uint32_t   StringSize;// = 14 : [another_string]
   |  }
   + [2]:
   |  struct SPatchString :
   |         SPatchItemHeader (Token=28(PATCH_TOKEN_STRING), Size=)==="
             << (sizeof(iOpenCL::SPatchString) + str2.length()) << R"===()
   |  {
   |      uint32_t   Index;// = 2
   |      uint32_t   StringSize;// = 18 : [yet_another_string]
   |  }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelDumper, GivenKernelWithNonArgCrossThreadDataPatchtokensThenProperlyCreatesDump) {
    using namespace iOpenCL;
    using namespace PatchTokensTestData;
    std::vector<uint8_t> stream;
    auto kernel = ValidEmptyKernel::create(stream);

    SPatchDataParameterBuffer localWorkSize[3] = {initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE),
                                                  initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE, 1U),
                                                  initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE, 2U)};
    SPatchDataParameterBuffer localWorkSize2[3] = {initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE),
                                                   initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE, 1U),
                                                   initDataParameterBufferToken(DATA_PARAMETER_LOCAL_WORK_SIZE, 2U)};
    SPatchDataParameterBuffer enqueuedLocalWorkSize[3] = {initDataParameterBufferToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE),
                                                          initDataParameterBufferToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, 1U),
                                                          initDataParameterBufferToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, 2U)};
    SPatchDataParameterBuffer numWorkgroups[3] = {initDataParameterBufferToken(DATA_PARAMETER_NUM_WORK_GROUPS),
                                                  initDataParameterBufferToken(DATA_PARAMETER_NUM_WORK_GROUPS, 1U),
                                                  initDataParameterBufferToken(DATA_PARAMETER_NUM_WORK_GROUPS, 2U)};
    SPatchDataParameterBuffer globalWorkOffset[3] = {initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET),
                                                     initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET, 1U),
                                                     initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET, 2U)};
    SPatchDataParameterBuffer globalWorkSize[3] = {initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_SIZE),
                                                   initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_SIZE, 1U),
                                                   initDataParameterBufferToken(DATA_PARAMETER_GLOBAL_WORK_SIZE, 2U)};
    SPatchDataParameterBuffer maxWorkGroupSize = initDataParameterBufferToken(DATA_PARAMETER_MAX_WORKGROUP_SIZE);
    auto workDimensions = initDataParameterBufferToken(DATA_PARAMETER_WORK_DIMENSIONS);
    auto simdSize = initDataParameterBufferToken(DATA_PARAMETER_SIMD_SIZE);
    auto parentEvent = initDataParameterBufferToken(DATA_PARAMETER_PARENT_EVENT);
    auto privateMemoryStatelessSize = initDataParameterBufferToken(DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE);
    auto localMemoryStatelessWindowSize = initDataParameterBufferToken(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE);
    auto localMemoryStatelessWindowStartAddress = initDataParameterBufferToken(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS);
    auto preferredWorkgroupMultiple = initDataParameterBufferToken(DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE);
    SPatchDataParameterBuffer childBlockSimdSize[2] = {initDataParameterBufferToken(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE),
                                                       initDataParameterBufferToken(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE, 2U)};
    auto unknownToken0 = initDataParameterBufferToken(NUM_DATA_PARAMETER_TOKENS);
    auto unknownToken1 = initDataParameterBufferToken(NUM_DATA_PARAMETER_TOKENS);

    kernel.tokens.crossThreadPayloadArgs.localWorkSize[0] = &localWorkSize[0];
    kernel.tokens.crossThreadPayloadArgs.localWorkSize[1] = &localWorkSize[1];
    kernel.tokens.crossThreadPayloadArgs.localWorkSize[2] = &localWorkSize[2];
    kernel.tokens.crossThreadPayloadArgs.localWorkSize2[0] = &localWorkSize2[0];
    kernel.tokens.crossThreadPayloadArgs.localWorkSize2[1] = nullptr;
    kernel.tokens.crossThreadPayloadArgs.localWorkSize2[2] = &localWorkSize2[2];
    kernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[0] = &enqueuedLocalWorkSize[0];
    kernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[1] = nullptr;
    kernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[2] = nullptr;
    kernel.tokens.crossThreadPayloadArgs.numWorkGroups[0] = &numWorkgroups[0];
    kernel.tokens.crossThreadPayloadArgs.numWorkGroups[1] = &numWorkgroups[1];
    kernel.tokens.crossThreadPayloadArgs.numWorkGroups[2] = &numWorkgroups[2];
    kernel.tokens.crossThreadPayloadArgs.globalWorkOffset[0] = &globalWorkOffset[0];
    kernel.tokens.crossThreadPayloadArgs.globalWorkOffset[1] = &globalWorkOffset[1];
    kernel.tokens.crossThreadPayloadArgs.globalWorkOffset[2] = &globalWorkOffset[2];
    kernel.tokens.crossThreadPayloadArgs.globalWorkSize[0] = &globalWorkSize[0];
    kernel.tokens.crossThreadPayloadArgs.globalWorkSize[1] = &globalWorkSize[1];
    kernel.tokens.crossThreadPayloadArgs.globalWorkSize[2] = &globalWorkSize[2];
    kernel.tokens.crossThreadPayloadArgs.maxWorkGroupSize = &maxWorkGroupSize;
    kernel.tokens.crossThreadPayloadArgs.workDimensions = &workDimensions;
    kernel.tokens.crossThreadPayloadArgs.simdSize = &simdSize;
    kernel.tokens.crossThreadPayloadArgs.parentEvent = &parentEvent;
    kernel.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize = &privateMemoryStatelessSize;
    kernel.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize = &localMemoryStatelessWindowSize;
    kernel.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress = &localMemoryStatelessWindowStartAddress;
    kernel.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple = &preferredWorkgroupMultiple;
    kernel.tokens.crossThreadPayloadArgs.childBlockSimdSize.push_back(&childBlockSimdSize[0]);
    kernel.tokens.crossThreadPayloadArgs.childBlockSimdSize.push_back(&childBlockSimdSize[1]);
    kernel.unhandledTokens.push_back(&unknownToken0);
    kernel.unhandledTokens.push_back(&unknownToken1);

    std::string generated = NEO::PatchTokenBinary::asString(kernel);
    static constexpr auto tokenSize = sizeof(SPatchDataParameterBuffer);
    std::stringstream expected;
    expected << R"===(Kernel of size : )===" << kernel.blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << kernel.header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << kernel.header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << kernel.blobs.patchList.size() << R"===(
  WARNING : Unhandled kernel-scope tokens detected [2] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = )==="
             << NUM_DATA_PARAMETER_TOKENS << R"===(
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = )==="
             << NUM_DATA_PARAMETER_TOKENS << R"===(
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
  localWorkSize [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 2(DATA_PARAMETER_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 2(DATA_PARAMETER_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 4
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [2]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 2(DATA_PARAMETER_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  localWorkSize2 [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 2(DATA_PARAMETER_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [2]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 2(DATA_PARAMETER_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  enqueuedLocalWorkSize [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 28(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  numWorkGroups [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 4(DATA_PARAMETER_NUM_WORK_GROUPS)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 4(DATA_PARAMETER_NUM_WORK_GROUPS)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 4
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [2]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 4(DATA_PARAMETER_NUM_WORK_GROUPS)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  globalWorkOffset [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 16(DATA_PARAMETER_GLOBAL_WORK_OFFSET)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 16(DATA_PARAMETER_GLOBAL_WORK_OFFSET)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 4
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [2]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 16(DATA_PARAMETER_GLOBAL_WORK_OFFSET)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  globalWorkSize [3] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 3(DATA_PARAMETER_GLOBAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 3(DATA_PARAMETER_GLOBAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 4
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [2]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 3(DATA_PARAMETER_GLOBAL_WORK_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 29(DATA_PARAMETER_MAX_WORKGROUP_SIZE)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 5(DATA_PARAMETER_WORK_DIMENSIONS)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 34(DATA_PARAMETER_SIMD_SIZE)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 22(DATA_PARAMETER_PARENT_EVENT)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 33(DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 32(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 31(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  struct SPatchDataParameterBuffer :
         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
  {
      uint32_t   Type;// = 30(DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE)
      uint32_t   ArgumentNumber;// = 0
      uint32_t   Offset;// = 0
      uint32_t   DataSize;// = 0
      uint32_t   SourceOffset;// = 0
      uint32_t   LocationIndex;// = 0
      uint32_t   LocationIndex2;// = 0
      uint32_t   IsEmulationArgument;// = 0
  }
  Child block simd size(s) [2] :
   + [0]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 38(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 0
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
   + [1]:
   |  struct SPatchDataParameterBuffer :
   |         SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << tokenSize << R"===()
   |  {
   |      uint32_t   Type;// = 38(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE)
   |      uint32_t   ArgumentNumber;// = 0
   |      uint32_t   Offset;// = 0
   |      uint32_t   DataSize;// = 0
   |      uint32_t   SourceOffset;// = 8
   |      uint32_t   LocationIndex;// = 0
   |      uint32_t   LocationIndex2;// = 0
   |      uint32_t   IsEmulationArgument;// = 0
   |  }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelDumper, GivenKernelWithArgThenProperlyCreatesDump) {
    std::vector<uint8_t> stream;
    auto kernel = PatchTokensTestData::ValidEmptyKernel::create(stream);
    kernel.tokens.kernelArgs.push_back(NEO::PatchTokenBinary::KernelArgFromPatchtokens{});
    auto kernelArgObjId = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_OBJECT_ID);
    kernel.tokens.kernelArgs[0].objectId = &kernelArgObjId;
    kernel.tokens.kernelArgs.push_back(kernel.tokens.kernelArgs[0]);
    auto generated = NEO::PatchTokenBinary::asString(kernel);
    std::stringstream expected;
    expected << R"===(Kernel of size : )===" << kernel.blobs.kernelInfo.size() << R"===(  decoded successfully
struct SKernelBinaryHeader {
    uint32_t   CheckSum;// = )==="
             << kernel.header->CheckSum << R"===(
    uint64_t   ShaderHashCode;// = 0
    uint32_t   KernelNameSize;// = 12
    uint32_t   PatchListSize;// = )==="
             << kernel.header->PatchListSize << R"===(
};
Kernel-scope tokens section size : )==="
             << kernel.blobs.patchList.size() << R"===(
  struct SPatchExecutionEnvironment :
         SPatchItemHeader (Token=23(PATCH_TOKEN_EXECUTION_ENVIRONMENT), Size=)==="
             << sizeof(iOpenCL::SPatchExecutionEnvironment) << R"===()
  {
      uint32_t    RequiredWorkGroupSizeX;// = 0
      uint32_t    RequiredWorkGroupSizeY;// = 0
      uint32_t    RequiredWorkGroupSizeZ;// = 0
      uint32_t    LargestCompiledSIMDSize;// = 32
      uint32_t    CompiledSubGroupsNumber;// = 0
      uint32_t    HasBarriers;// = 0
      uint32_t    DisableMidThreadPreemption;// = 0
      uint32_t    CompiledSIMD8;// = 0
      uint32_t    CompiledSIMD16;// = 0
      uint32_t    CompiledSIMD32;// = 1
      uint32_t    HasDeviceEnqueue;// = 0
      uint32_t    MayAccessUndeclaredResource;// = 0
      uint32_t    UsesFencesForReadWriteImages;// = 0
      uint32_t    UsesStatelessSpillFill;// = 0
      uint32_t    UsesMultiScratchSpaces;// = 0
      uint32_t    IsCoherent;// = 0
      uint32_t    IsInitializer;// = 0
      uint32_t    IsFinalizer;// = 0
      uint32_t    SubgroupIndependentForwardProgressRequired;// = 0
      uint32_t    CompiledForGreaterThan4GBBuffers;// = 0
      uint32_t    NumGRFRequired;// = 0
      uint32_t    WorkgroupWalkOrderDims;// = 0
      uint32_t    HasGlobalAtomics;// = 0
  }
Kernel arguments [2] :
  + kernelArg[0]:
  | Kernel argument of type unspecified
  |   struct SPatchDataParameterBuffer :
  |          SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |   {
  |       uint32_t   Type;// = 35(DATA_PARAMETER_OBJECT_ID)
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   Offset;// = 0
  |       uint32_t   DataSize;// = 0
  |       uint32_t   SourceOffset;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
  + kernelArg[1]:
  | Kernel argument of type unspecified
  |   struct SPatchDataParameterBuffer :
  |          SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |   {
  |       uint32_t   Type;// = 35(DATA_PARAMETER_OBJECT_ID)
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   Offset;// = 0
  |       uint32_t   DataSize;// = 0
  |       uint32_t   SourceOffset;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenKernelArgWithObjectIdAndArgInfoThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto kernelArgObjId = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_OBJECT_ID);
    kernelArg.objectId = &kernelArgObjId;

    std::vector<uint8_t> argInfoStorage;
    PatchTokensTestData::pushBackArgInfoToken(argInfoStorage);
    kernelArg.argInfo = reinterpret_cast<iOpenCL::SPatchKernelArgumentInfo *>(argInfoStorage.data());

    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type unspecified
  |   struct SPatchKernelArgumentInfo :
  |          SPatchItemHeader (Token=26(PATCH_TOKEN_KERNEL_ARGUMENT_INFO), Size=)==="
             << kernelArg.argInfo->Size << R"===()
  |   {
  |       uint32_t ArgumentNumber;// = 0
  |       uint32_t AddressQualifierSize;// = 8 : [__global]
  |       uint32_t AccessQualifierSize;// = 10 : [read_write]
  |       uint32_t ArgumentNameSize;// = 10 : [custom_arg]
  |       uint32_t TypeNameSize;// = 5 : [int*;]
  |       uint32_t TypeQualifierSize;// = 5 : [const]
  |   }
  |   struct SPatchDataParameterBuffer :
  |          SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |   {
  |       uint32_t   Type;// = 35(DATA_PARAMETER_OBJECT_ID)
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   Offset;// = 0
  |       uint32_t   DataSize;// = 0
  |       uint32_t   SourceOffset;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenSamplerObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchSamplerKernelArgument>(iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type SAMPLER
  |   struct SPatchSamplerKernelArgument :
  |          SPatchItemHeader (Token=16(PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   Type;// = 0
  |       uint32_t   Offset;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   needBindlessHandle;// = 0
  |       uint32_t   TextureMask;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |       uint32_t   btiOffset;// = 0
  |   }
  |   Sampler Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenImageObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchImageMemoryObjectKernelArgument>(iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Image;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type IMAGE
  |   struct SPatchImageMemoryObjectKernelArgument :
  |          SPatchItemHeader (Token=12(PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t  ArgumentNumber;// = 0
  |       uint32_t  Type;// = 0
  |       uint32_t  Offset;// = 0
  |       uint32_t  LocationIndex;// = 0
  |       uint32_t  LocationIndex2;// = 0
  |       uint32_t  Writeable;// = 0
  |       uint32_t  Transformable;// = 0
  |       uint32_t  needBindlessHandle;// = 0
  |       uint32_t  IsEmulationArgument;// = 0
  |       uint32_t  btiOffset;// = 0
  |   }
  |   Image Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenGlobalMemoryObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchGlobalMemoryObjectKernelArgument>(iOpenCL::PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type BUFFER
  |   struct SPatchGlobalMemoryObjectKernelArgument :
  |          SPatchItemHeader (Token=11(PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   Offset;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
  |   Buffer Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenStatelessGlobalMemoryObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument>(iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type BUFFER
  |   struct SPatchStatelessGlobalMemoryObjectKernelArgument :
  |          SPatchItemHeader (Token=30(PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   SurfaceStateHeapOffset;// = 0
  |       uint32_t   DataParamOffset;// = 0
  |       uint32_t   DataParamSize;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
  |   Buffer Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenStatelessConstantMemoryObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchStatelessConstantMemoryObjectKernelArgument>(iOpenCL::PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type BUFFER
  |   struct SPatchStatelessConstantMemoryObjectKernelArgument :
  |          SPatchItemHeader (Token=31(PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   SurfaceStateHeapOffset;// = 0
  |       uint32_t   DataParamOffset;// = 0
  |       uint32_t   DataParamSize;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
  |   Buffer Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenStatelessDeviceQueueObjectKernelArgThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    auto objectArg = PatchTokensTestData::initToken<iOpenCL::SPatchStatelessDeviceQueueKernelArgument>(iOpenCL::PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT);
    kernelArg.objectArg = &objectArg;
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type BUFFER
  |   struct SPatchStatelessDeviceQueueKernelArgument :
  |          SPatchItemHeader (Token=46(PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT), Size=)==="
             << sizeof(objectArg) << R"===()
  |   {
  |       uint32_t   ArgumentNumber;// = 0
  |       uint32_t   SurfaceStateHeapOffset;// = 0
  |       uint32_t   DataParamOffset;// = 0
  |       uint32_t   DataParamSize;// = 0
  |       uint32_t   LocationIndex;// = 0
  |       uint32_t   LocationIndex2;// = 0
  |       uint32_t   IsEmulationArgument;// = 0
  |   }
  |   Buffer Metadata:
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenBufferKernelArgWithMetadataTokensThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Buffer;
    auto dataBufferOffset = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_BUFFER_OFFSET);
    auto pureStateful = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL);
    kernelArg.metadata.buffer.bufferOffset = &dataBufferOffset;
    kernelArg.metadata.buffer.pureStateful = &pureStateful;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type BUFFER
  |   Buffer Metadata:
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 42(DATA_PARAMETER_BUFFER_OFFSET)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 43(DATA_PARAMETER_BUFFER_STATEFUL)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenImageKernelArgWithMetadataTokensThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Image;
    auto width = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_WIDTH);
    auto height = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_HEIGHT);
    auto depth = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_DEPTH);
    auto channelDataType = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE);
    auto channelOrder = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_ORDER);
    auto arraySize = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_ARRAY_SIZE);
    auto numSamples = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_NUM_SAMPLES);
    auto numMipLevels = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS);
    auto flatBaseOffset = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_FLAT_IMAGE_BASEOFFSET);
    auto flatWidth = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_FLAT_IMAGE_WIDTH);
    auto flatHeight = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_FLAT_IMAGE_HEIGHT);
    auto flatPitch = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_FLAT_IMAGE_PITCH);

    kernelArg.metadata.image.width = &width;
    kernelArg.metadata.image.height = &height;
    kernelArg.metadata.image.depth = &depth;
    kernelArg.metadata.image.channelDataType = &channelDataType;
    kernelArg.metadata.image.channelOrder = &channelOrder;
    kernelArg.metadata.image.arraySize = &arraySize;
    kernelArg.metadata.image.numSamples = &numSamples;
    kernelArg.metadata.image.numMipLevels = &numMipLevels;
    kernelArg.metadata.image.flatBaseOffset = &flatBaseOffset;
    kernelArg.metadata.image.flatWidth = &flatWidth;
    kernelArg.metadata.image.flatHeight = &flatHeight;
    kernelArg.metadata.image.flatPitch = &flatPitch;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type IMAGE
  |   Image Metadata:
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 9(DATA_PARAMETER_IMAGE_WIDTH)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 10(DATA_PARAMETER_IMAGE_HEIGHT)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 11(DATA_PARAMETER_IMAGE_DEPTH)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 12(DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 13(DATA_PARAMETER_IMAGE_CHANNEL_ORDER)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 18(DATA_PARAMETER_IMAGE_ARRAY_SIZE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 20(DATA_PARAMETER_IMAGE_NUM_SAMPLES)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 27(DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 44(DATA_PARAMETER_FLAT_IMAGE_BASEOFFSET)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 45(DATA_PARAMETER_FLAT_IMAGE_WIDTH)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 46(DATA_PARAMETER_FLAT_IMAGE_HEIGHT)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 47(DATA_PARAMETER_FLAT_IMAGE_PITCH)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenSamplerKernelArgWithMetadataTokensThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Sampler;
    auto coordinateSnapWaRequired = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED);
    auto addressMode = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE);
    auto normalizedCoords = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS);
    kernelArg.metadata.sampler.coordinateSnapWaRequired = &coordinateSnapWaRequired;
    kernelArg.metadata.sampler.addressMode = &addressMode;
    kernelArg.metadata.sampler.normalizedCoords = &normalizedCoords;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type SAMPLER
  |   Sampler Metadata:
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 14(DATA_PARAMETER_SAMPLER_ADDRESS_MODE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 21(DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 15(DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenSlmKernelArgWithMetadataTokensThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Slm;
    auto slm = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES);
    kernelArg.metadata.slm.token = &slm;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type SLM
  |   Slm Metadata:
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 8(DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(KernelArgDumper, GivenVmeKernelArgWithMetadataTokensThenProperlyCreatesDump) {
    NEO::PatchTokenBinary::KernelArgFromPatchtokens kernelArg = {};
    kernelArg.objectType = NEO::PatchTokenBinary::ArgObjectType::Image;
    kernelArg.objectTypeSpecialized = NEO::PatchTokenBinary::ArgObjectTypeSpecialized::Vme;
    auto mbBlockType = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_VME_MB_BLOCK_TYPE);
    auto subpixelMode = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_VME_SUBPIXEL_MODE);
    auto sadAdjustMode = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_VME_SAD_ADJUST_MODE);
    auto searchPathType = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_VME_SEARCH_PATH_TYPE);

    kernelArg.metadataSpecialized.vme.mbBlockType = &mbBlockType;
    kernelArg.metadataSpecialized.vme.subpixelMode = &subpixelMode;
    kernelArg.metadataSpecialized.vme.sadAdjustMode = &sadAdjustMode;
    kernelArg.metadataSpecialized.vme.searchPathType = &searchPathType;
    auto generated = NEO::PatchTokenBinary::asString(kernelArg, "  | ");
    std::stringstream expected;
    expected << R"===(  | Kernel argument of type IMAGE [ VME ]
  |   Image Metadata:
  |   Vme Metadata:
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 23(DATA_PARAMETER_VME_MB_BLOCK_TYPE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 25(DATA_PARAMETER_VME_SAD_ADJUST_MODE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 26(DATA_PARAMETER_VME_SEARCH_PATH_TYPE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
  |     struct SPatchDataParameterBuffer :
  |            SPatchItemHeader (Token=17(PATCH_TOKEN_DATA_PARAMETER_BUFFER), Size=)==="
             << sizeof(iOpenCL::SPatchDataParameterBuffer) << R"===()
  |     {
  |         uint32_t   Type;// = 24(DATA_PARAMETER_VME_SUBPIXEL_MODE)
  |         uint32_t   ArgumentNumber;// = 0
  |         uint32_t   Offset;// = 0
  |         uint32_t   DataSize;// = 0
  |         uint32_t   SourceOffset;// = 0
  |         uint32_t   LocationIndex;// = 0
  |         uint32_t   LocationIndex2;// = 0
  |         uint32_t   IsEmulationArgument;// = 0
  |     }
)===";
    EXPECT_STREQ(expected.str().c_str(), generated.c_str());
}

TEST(PatchTokenDumper, GivenAnyTokenThenDumpingIsHandled) {
    constexpr uint32_t maxTokenSize = 4096;

    PatchTokensTestData::ValidEmptyProgram programToDecode;
    {
        programToDecode.storage.resize(programToDecode.storage.size() + maxTokenSize, 0U);
        programToDecode.recalcTokPtr();
        programToDecode.blobs.patchList = ArrayRef<const uint8_t>(programToDecode.storage.data() + sizeof(*programToDecode.headerMutable),
                                                                  programToDecode.storage.size() - sizeof(*programToDecode.headerMutable));
        programToDecode.headerMutable->PatchListSize = static_cast<uint32_t>(programToDecode.blobs.patchList.size());
    }
    iOpenCL::SPatchItemHeader *programToken = reinterpret_cast<iOpenCL::SPatchItemHeader *>(programToDecode.storage.data() + sizeof(*programToDecode.headerMutable));
    programToken->Size = maxTokenSize;

    std::vector<uint8_t> kernelToDecodeStorage;
    iOpenCL::SPatchItemHeader *kernelToken = nullptr;
    auto kernelToDecode = PatchTokensTestData::ValidEmptyKernel::create(kernelToDecodeStorage);
    {
        auto kernelPatchListOffset = ptrDiff(kernelToDecode.blobs.patchList.begin(), kernelToDecodeStorage.data());
        kernelToDecodeStorage.resize(kernelToDecodeStorage.size() + maxTokenSize, 0U);
        kernelToDecode.blobs.kernelInfo = ArrayRef<const uint8_t>(kernelToDecodeStorage.data(), kernelToDecodeStorage.data() + kernelToDecodeStorage.size());
        kernelToDecode.blobs.patchList = ArrayRef<const uint8_t>(kernelToDecodeStorage.data() + kernelPatchListOffset, kernelToDecodeStorage.data() + kernelToDecodeStorage.size());
        auto kernelHeaderMutable = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(&*(kernelToDecodeStorage.begin()));
        kernelHeaderMutable->PatchListSize = static_cast<uint32_t>(kernelToDecode.blobs.patchList.size());
        kernelToDecode.tokens.executionEnvironment = reinterpret_cast<iOpenCL::SPatchExecutionEnvironment *>(kernelToDecodeStorage.data() + kernelPatchListOffset);
        kernelToken = reinterpret_cast<iOpenCL::SPatchItemHeader *>(kernelToDecodeStorage.data() + kernelPatchListOffset + sizeof(iOpenCL::SPatchExecutionEnvironment));
    }
    kernelToken->Size = maxTokenSize;

    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    std::unordered_set<int> tokensWhitelist{50, 52};
    for (int i = 0; i < iOpenCL::NUM_PATCH_TOKENS; ++i) {
        if (tokensWhitelist.count(i) != 0) {
            continue;
        }
        kernelToken->Token = i;
        decodedKernel = {};
        NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToDecode.blobs.kernelInfo, decodedKernel);
        EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
        if (decodedKernel.unhandledTokens.empty()) {
            auto dump = NEO::PatchTokenBinary::asString(decodedKernel);
            EXPECT_EQ(std::string::npos, dump.find("struct SPatchItemHeader")) << "Update patchtokens_dumper.cpp with definition of new patchtoken : " << i;
            continue;
        }
        EXPECT_EQ(kernelToken, decodedKernel.unhandledTokens[0]);

        programToken->Token = i;
        decodedProgram = {};
        NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToDecode.blobs.programInfo, decodedProgram);
        EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
        if (decodedProgram.unhandledTokens.empty()) {
            auto dump = NEO::PatchTokenBinary::asString(decodedProgram);
            EXPECT_EQ(std::string::npos, dump.find("struct SPatchItemHeader")) << "Update patchtokens_dumper.cpp with definition of new patchtoken : " << i;
            continue;
        }
        EXPECT_EQ(programToken, decodedProgram.unhandledTokens[0]);
    }

    auto kernelDataParamToken = static_cast<iOpenCL::SPatchDataParameterBuffer *>(kernelToken);
    *kernelDataParamToken = PatchTokensTestData::initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_BUFFER_OFFSET);
    kernelDataParamToken->Size = maxTokenSize;
    std::unordered_set<int> dataParamTokensWhitelist{6, 7, 17, 19, 36, 37, 39, 40, 41};
    for (int i = 0; i < iOpenCL::NUM_DATA_PARAMETER_TOKENS; ++i) {
        if (dataParamTokensWhitelist.count(i) != 0) {
            continue;
        }
        kernelDataParamToken->Type = i;
        decodedKernel = {};
        NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToDecode.blobs.kernelInfo, decodedKernel);
        auto dump = NEO::PatchTokenBinary::asString(decodedKernel);
        if (decodedKernel.unhandledTokens.empty()) {
            auto dump = NEO::PatchTokenBinary::asString(decodedKernel);
            EXPECT_NE(std::string::npos, dump.find("Type;// = " + std::to_string(i) + "(")) << "Update patchtokens_dumper.cpp with definition of SPatchDataParameterBuffer with type :" << i;
        }
    }
}
