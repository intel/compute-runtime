/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
}

namespace NEO {
template <size_t binariesCount = 1u, Elf::ElfIdentifierClass numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64>
struct MockZebinWrapper {
    using Descriptor = ZebinTestData::ZebinCopyBufferModule<numBits>::Descriptor;

    MockZebinWrapper(const HardwareInfo &hwInfo, Descriptor desc)
        : data(hwInfo, desc) {
        std::fill_n(binaries.begin(), binariesCount, reinterpret_cast<const unsigned char *>(this->data.storage.data()));
        std::fill_n(binarySizes.begin(), binariesCount, this->data.storage.size());
    }

    MockZebinWrapper(const HardwareInfo &hwInfo) : MockZebinWrapper(hwInfo, Descriptor{}) {}

    auto &getFlags() {
        return reinterpret_cast<Zebin::Elf::ZebinTargetFlags &>(this->data.elfHeader->flags);
    }

    void setAsMockCompilerReturnedBinary() {
        debugVarsRestore.reset();
        MockCompilerDebugVars debugVars;
        debugVars.binaryToReturn = const_cast<unsigned char *>(this->binaries[0]);
        debugVars.binaryToReturnSize = sizeof(unsigned char) * this->binarySizes[0];
        gEnvironment->igcPushDebugVars(debugVars);
        gEnvironment->fclPushDebugVars(debugVars);
        this->debugVarsRestore = std::unique_ptr<void, void (*)(void *)>{&gEnvironment, [](void *) -> void {
                                                                             gEnvironment->igcPopDebugVars();
                                                                             gEnvironment->fclPopDebugVars();
                                                                         }};
    }

    void setAsMockCompilerLoadedFile(const std::string &fileName) {
        debugVarsRestore.reset();
        virtualFileList[fileName] << this->binaries[0];
        MockCompilerDebugVars debugVars;
        debugVars.fileName = fileName;
        gEnvironment->igcPushDebugVars(debugVars);
        gEnvironment->fclPushDebugVars(debugVars);
        this->debugVarsRestore = std::unique_ptr<void, void (*)(void *)>{&gEnvironment, [](void *) -> void {
                                                                             gEnvironment->igcPopDebugVars();
                                                                             gEnvironment->fclPopDebugVars();
                                                                         }};
    }

    ZebinTestData::ZebinCopyBufferModule<numBits> data;
    std::array<const unsigned char *, binariesCount> binaries;
    std::array<size_t, binariesCount> binarySizes;
    std::unique_ptr<void, void (*)(void *)> debugVarsRestore{nullptr, nullptr};
    const char *kernelName = "CopyBuffer";
};

class FixtureWithMockZebin {
  public:
    void setUp() {
        zebinPtr = std::make_unique<MockZebinWrapper<>>(*defaultHwInfo);
        zebinPtr->setAsMockCompilerReturnedBinary();
    }

    void tearDown() {
        zebinPtr.reset();
    }

    static constexpr const char sourceKernel[] = "example_kernel(){}";
    static constexpr size_t sourceKernelSize = sizeof(sourceKernel);
    const char *sources[1] = {sourceKernel};
    std::unique_ptr<MockZebinWrapper<>> zebinPtr;
};
} // namespace NEO
