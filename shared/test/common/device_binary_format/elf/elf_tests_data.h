/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/string.h"

#include "patch_list.h"

using namespace NEO;

enum class enabledIrFormat {
    NONE,
    ENABLE_SPIRV,
    ENABLE_LLVM
};

template <enabledIrFormat irFormat = enabledIrFormat::NONE>
struct MockElfBinaryPatchtokens {
    MockElfBinaryPatchtokens(const HardwareInfo &hwInfo) : MockElfBinaryPatchtokens(std::string{}, hwInfo){};
    MockElfBinaryPatchtokens(const std::string &buildOptions, const HardwareInfo &hwInfo) {
        mockDevBinaryHeader.Device = hwInfo.platform.eRenderCoreFamily;
        mockDevBinaryHeader.GPUPointerSizeInBytes = sizeof(void *);
        mockDevBinaryHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
        constexpr size_t mockDevBinaryDataSize = sizeof(mockDevBinaryHeader) + mockDataSize;
        constexpr size_t mockSpirvBinaryDataSize = sizeof(spirvMagic) + mockDataSize;
        constexpr size_t mockLlvmBinaryDataSize = sizeof(llvmBcMagic) + mockDataSize;

        char mockDevBinaryData[mockDevBinaryDataSize]{};
        memcpy_s(mockDevBinaryData, mockDevBinaryDataSize, &mockDevBinaryHeader, sizeof(mockDevBinaryHeader));
        memset(mockDevBinaryData + sizeof(mockDevBinaryHeader), '\x01', mockDataSize);

        char mockSpirvBinaryData[mockSpirvBinaryDataSize]{};
        memcpy_s(mockSpirvBinaryData, mockSpirvBinaryDataSize, spirvMagic.data(), spirvMagic.size());
        memset(mockSpirvBinaryData + spirvMagic.size(), '\x02', mockDataSize);

        char mockLlvmBinaryData[mockLlvmBinaryDataSize]{};
        memcpy_s(mockLlvmBinaryData, mockLlvmBinaryDataSize, llvmBcMagic.data(), llvmBcMagic.size());
        memset(mockLlvmBinaryData + llvmBcMagic.size(), '\x03', mockDataSize);

        Elf::ElfEncoder<Elf::EI_CLASS_64> enc;
        enc.getElfFileHeader().identity = Elf::ElfFileHeaderIdentity(Elf::EI_CLASS_64);
        enc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
        enc.appendSection(Elf::SHT_OPENCL_DEV_BINARY, Elf::SectionNamesOpenCl::deviceBinary, ArrayRef<const uint8_t>::fromAny(mockDevBinaryData, mockDevBinaryDataSize));
        if (irFormat == enabledIrFormat::ENABLE_SPIRV)
            enc.appendSection(Elf::SHT_OPENCL_SPIRV, Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(mockSpirvBinaryData, mockSpirvBinaryDataSize));
        else if (irFormat == enabledIrFormat::ENABLE_LLVM)
            enc.appendSection(Elf::SHT_OPENCL_LLVM_BINARY, Elf::SectionNamesOpenCl::llvmObject, ArrayRef<const uint8_t>::fromAny(mockLlvmBinaryData, mockLlvmBinaryDataSize));
        if (false == buildOptions.empty())
            enc.appendSection(Elf::SHT_OPENCL_OPTIONS, Elf::SectionNamesOpenCl::buildOptions, ArrayRef<const uint8_t>::fromAny(buildOptions.data(), buildOptions.size()));
        storage = enc.encode();
    }
    static constexpr size_t mockDataSize = 0x10;
    iOpenCL::SProgramBinaryHeader mockDevBinaryHeader = iOpenCL::SProgramBinaryHeader{iOpenCL::MAGIC_CL, 0, 0, 0, 0, 0, 0};
    std::vector<uint8_t> storage;
};