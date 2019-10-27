/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "test.h"

#include <vector>

namespace PatchTokensTestData {
struct ValidEmptyProgram : NEO::PatchTokenBinary::ProgramFromPatchtokens {
    ValidEmptyProgram() {
        iOpenCL::SProgramBinaryHeader headerTok = {};
        headerTok.Magic = iOpenCL::MAGIC_CL;
        headerTok.Version = iOpenCL::CURRENT_ICBE_VERSION;
        headerTok.Device = renderCoreFamily;
        this->decodeStatus = NEO::PatchTokenBinary::DecoderError::Success;

        storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&headerTok), reinterpret_cast<uint8_t *>((&headerTok) + 1));
        recalcTokPtr();
    }
    void recalcTokPtr() {
        this->blobs.programInfo = storage;
        this->headerMutable = reinterpret_cast<iOpenCL::SProgramBinaryHeader *>(storage.data());
        this->header = this->headerMutable;
    }

    std::vector<uint8_t> storage;
    iOpenCL::SProgramBinaryHeader *headerMutable = nullptr;
};

struct ValidProgramWithConstantSurface : ValidEmptyProgram {
    ValidProgramWithConstantSurface() {
        iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo constSurfTok = {};
        constSurfTok.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        constSurfTok.Size = sizeof(constSurfTok);
        constSurfTok.InlineDataSize = 128;
        this->programScopeTokens.allocateConstantMemorySurface.push_back(nullptr);
        storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&constSurfTok), reinterpret_cast<uint8_t *>((&constSurfTok) + 1));
        storage.resize(storage.size() + constSurfTok.InlineDataSize);
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidEmptyProgram::recalcTokPtr();
        this->constSurfMutable = reinterpret_cast<iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *>(storage.data() + sizeof(*this->headerMutable));
        this->programScopeTokens.allocateConstantMemorySurface[0] = this->constSurfMutable;
        this->blobs.patchList = ArrayRef<const uint8_t>(storage.data() + sizeof(*this->headerMutable), storage.size() - sizeof(*this->headerMutable));
        this->headerMutable->PatchListSize = static_cast<uint32_t>(this->blobs.patchList.size());
    }

    iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *constSurfMutable = nullptr;
};

struct ValidProgramWithGlobalSurface : ValidEmptyProgram {
    ValidProgramWithGlobalSurface() {
        iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo globalSurfTok = {};
        globalSurfTok.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        globalSurfTok.Size = sizeof(globalSurfTok);
        globalSurfTok.InlineDataSize = 256;
        this->programScopeTokens.allocateGlobalMemorySurface.push_back(nullptr);
        storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&globalSurfTok), reinterpret_cast<uint8_t *>((&globalSurfTok) + 1));
        storage.resize(storage.size() + globalSurfTok.InlineDataSize);
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidEmptyProgram::recalcTokPtr();
        this->globalSurfMutable = reinterpret_cast<iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *>(storage.data() + sizeof(*this->headerMutable));
        this->programScopeTokens.allocateGlobalMemorySurface[0] = this->globalSurfMutable;
        this->blobs.patchList = ArrayRef<const uint8_t>(storage.data() + sizeof(*this->headerMutable), storage.size() - sizeof(*this->headerMutable));
        this->headerMutable->PatchListSize = static_cast<uint32_t>(this->blobs.patchList.size());
    }

    iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *globalSurfMutable = nullptr;
};

struct ValidProgramWithConstantSurfaceAndPointer : ValidProgramWithConstantSurface {
    ValidProgramWithConstantSurfaceAndPointer() {
        iOpenCL::SPatchConstantPointerProgramBinaryInfo constantPointerTok = {};
        constantPointerTok.Token = iOpenCL::PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
        constantPointerTok.Size = sizeof(constantPointerTok);
        constantPointerTok.ConstantBufferIndex = 0;
        constantPointerTok.BufferIndex = 0;
        constantPointerTok.BufferType = iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER;
        constantPointerTok.ConstantPointerOffset = 96;
        this->programScopeTokens.constantPointer.push_back(nullptr);
        storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&constantPointerTok), reinterpret_cast<uint8_t *>((&constantPointerTok) + 1));
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidProgramWithConstantSurface::recalcTokPtr();
        this->constantPointerMutable = reinterpret_cast<iOpenCL::SPatchConstantPointerProgramBinaryInfo *>(storage.data() + sizeof(*this->headerMutable) + sizeof(*this->constSurfMutable) + this->constSurfMutable->InlineDataSize);
        this->programScopeTokens.constantPointer[0] = this->constantPointerMutable;
    }

    iOpenCL::SPatchConstantPointerProgramBinaryInfo *constantPointerMutable = nullptr;
};

struct ValidProgramWithGlobalSurfaceAndPointer : ValidProgramWithGlobalSurface {
    ValidProgramWithGlobalSurfaceAndPointer() {
        iOpenCL::SPatchGlobalPointerProgramBinaryInfo globalPointerTok = {};
        globalPointerTok.Token = iOpenCL::PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
        globalPointerTok.Size = sizeof(globalPointerTok);
        globalPointerTok.GlobalBufferIndex = 0;
        globalPointerTok.BufferIndex = 0;
        globalPointerTok.BufferType = iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER;
        globalPointerTok.GlobalPointerOffset = 48;
        this->programScopeTokens.globalPointer.push_back(nullptr);
        storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&globalPointerTok), reinterpret_cast<uint8_t *>((&globalPointerTok) + 1));
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidProgramWithGlobalSurface::recalcTokPtr();
        this->globalPointerMutable = reinterpret_cast<iOpenCL::SPatchGlobalPointerProgramBinaryInfo *>(storage.data() + sizeof(*this->headerMutable) + sizeof(*this->globalSurfMutable) + this->globalSurfMutable->InlineDataSize);
        this->programScopeTokens.globalPointer[0] = this->globalPointerMutable;
    }

    iOpenCL::SPatchGlobalPointerProgramBinaryInfo *globalPointerMutable = nullptr;
};

struct ValidEmptyKernel {
    static NEO::PatchTokenBinary::KernelFromPatchtokens create(std::vector<uint8_t> &storage) {
        NEO::PatchTokenBinary::KernelFromPatchtokens ret;
        iOpenCL::SKernelBinaryHeaderCommon headerTokInl = {};
        ret.decodeStatus = NEO::PatchTokenBinary::DecoderError::Success;
        ret.name = "test_kernel";
        headerTokInl.KernelNameSize = static_cast<uint32_t>(ret.name.size());

        auto kernOffset = storage.size();
        storage.reserve(storage.size() + 512);
        storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&headerTokInl), reinterpret_cast<const uint8_t *>((&headerTokInl) + 1));
        auto headerTok = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(&*(storage.begin() + kernOffset));
        ret.NEO::PatchTokenBinary::KernelFromPatchtokens::header = headerTok;
        storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(ret.name.begin()), reinterpret_cast<const uint8_t *>(ret.name.end()));
        ret.blobs.kernelInfo = ArrayRef<const uint8_t>(storage.data() + kernOffset, storage.data() + storage.size());
        headerTok->CheckSum = NEO::PatchTokenBinary::calcKernelChecksum(ret.blobs.kernelInfo);
        return ret;
    }
};

struct ValidProgramWithKernel : ValidEmptyProgram {
    ValidProgramWithKernel() {
        this->headerMutable->NumberOfKernels = 1;
        kernOffset = storage.size();
        this->kernels.push_back(ValidEmptyKernel::create(storage));
        this->kernels[0].decodeStatus = NEO::PatchTokenBinary::DecoderError::Success;
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidEmptyProgram::recalcTokPtr();
        this->kernels[0].blobs.kernelInfo = ArrayRef<const uint8_t>(storage.data() + kernOffset, storage.data() + storage.size());
        kernelHeaderMutable = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(&*(storage.begin() + kernOffset));
        this->kernels[0].header = kernelHeaderMutable;
        kernelHeaderMutable->CheckSum = NEO::PatchTokenBinary::calcKernelChecksum(this->kernels[0].blobs.kernelInfo);
    }

    size_t kernOffset = 0U;
    iOpenCL::SKernelBinaryHeaderCommon *kernelHeaderMutable = nullptr;
};

struct ValidProgramWithKernelUsingSlm : ValidProgramWithKernel {
    ValidProgramWithKernelUsingSlm() {
        patchlistOffset = storage.size();
        iOpenCL::SPatchAllocateLocalSurface slmTok = {};
        slmTok.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;
        slmTok.Size = sizeof(slmTok);
        slmTok.TotalInlineLocalMemorySize = 16;
        storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&slmTok), reinterpret_cast<const uint8_t *>((&slmTok) + 1));
        recalcTokPtr();
    }

    void recalcTokPtr() {
        ValidProgramWithKernel::recalcTokPtr();
        this->kernels[0].blobs.patchList = ArrayRef<const uint8_t>(storage.data() + patchlistOffset, storage.data() + storage.size());
        slmMutable = reinterpret_cast<iOpenCL::SPatchAllocateLocalSurface *>(storage.data() + patchlistOffset);
        this->kernels[0].tokens.allocateLocalSurface = slmMutable;
        this->kernelHeaderMutable->PatchListSize = static_cast<uint32_t>(this->kernels[0].blobs.patchList.size());
    }

    iOpenCL::SPatchAllocateLocalSurface *slmMutable = nullptr;
    size_t patchlistOffset = 0U;
};

template <typename TokenT>
inline TokenT initToken(iOpenCL::PATCH_TOKEN tok) {
    TokenT ret = {};
    ret.Size = sizeof(TokenT);
    ret.Token = tok;
    return ret;
}

inline iOpenCL::SPatchDataParameterBuffer initDataParameterBufferToken(iOpenCL::DATA_PARAMETER_TOKEN type, uint32_t sourceIndex = 0, uint32_t argNum = 0) {
    iOpenCL::SPatchDataParameterBuffer tok = {};
    tok.Size = static_cast<uint32_t>(sizeof(iOpenCL::SPatchDataParameterBuffer));
    tok.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    tok.Type = type;
    tok.SourceOffset = sourceIndex * sizeof(uint32_t);
    tok.ArgumentNumber = argNum;
    return tok;
}

inline uint32_t pushBackString(const std::string &str, std::vector<uint8_t> &storage) {
    auto offset = storage.size();
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(str.c_str()), reinterpret_cast<const uint8_t *>(str.c_str()) + str.size());
    return static_cast<uint32_t>(offset);
}

inline uint32_t pushBackStringToken(const std::string &str, uint32_t stringIndex, std::vector<uint8_t> &outStream) {
    auto off = outStream.size();
    outStream.reserve(outStream.size() + sizeof(iOpenCL::SPatchString) + str.length());
    outStream.resize(outStream.size() + sizeof(iOpenCL::SPatchString));
    iOpenCL::SPatchString *tok = reinterpret_cast<iOpenCL::SPatchString *>(outStream.data() + off);
    *tok = initToken<iOpenCL::SPatchString>(iOpenCL::PATCH_TOKEN::PATCH_TOKEN_STRING);
    tok->StringSize = static_cast<uint32_t>(str.length());
    tok->Size += tok->StringSize;
    tok->Index = stringIndex;
    pushBackString(str, outStream);
    return static_cast<uint32_t>(off);
};

inline uint32_t pushBackArgInfoToken(std::vector<uint8_t> &outStream,
                                     uint32_t argNum = 0,
                                     const std::string &addressQualifier = "__global", const std::string &accessQualifier = "read_write",
                                     const std::string &argName = "custom_arg", const std::string &typeName = "int*;", std::string typeQualifier = "const") {
    auto off = outStream.size();
    iOpenCL::SPatchKernelArgumentInfo tok = {};
    tok.Token = iOpenCL::PATCH_TOKEN_KERNEL_ARGUMENT_INFO;
    tok.AddressQualifierSize = static_cast<uint32_t>(addressQualifier.size());
    tok.AccessQualifierSize = static_cast<uint32_t>(accessQualifier.size());
    tok.ArgumentNameSize = static_cast<uint32_t>(argName.size());
    tok.TypeNameSize = static_cast<uint32_t>(typeName.size());
    tok.TypeQualifierSize = static_cast<uint32_t>(typeQualifier.size());
    tok.Size = sizeof(iOpenCL::SPatchKernelArgumentInfo) + tok.AddressQualifierSize + tok.AccessQualifierSize + tok.ArgumentNameSize + tok.TypeNameSize + tok.TypeQualifierSize;

    outStream.insert(outStream.end(), reinterpret_cast<const uint8_t *>(&tok), reinterpret_cast<const uint8_t *>(&tok) + sizeof(tok));
    pushBackString(addressQualifier, outStream);
    pushBackString(accessQualifier, outStream);
    pushBackString(argName, outStream);
    pushBackString(typeName, outStream);
    pushBackString(typeQualifier, outStream);
    return static_cast<uint32_t>(off);
}
} // namespace PatchTokensTestData
