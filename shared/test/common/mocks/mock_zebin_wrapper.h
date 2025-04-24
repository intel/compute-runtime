/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

namespace NEO {
template <size_t binariesCount = 1u, Elf::ElfIdentifierClass numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64>
struct MockZebinWrapper {
    MockZebinWrapper(const HardwareInfo &hwInfo, uint8_t simdSize)
        : data(hwInfo, simdSize) {
        std::fill_n(binaries.begin(), binariesCount, reinterpret_cast<const unsigned char *>(this->data.storage.data()));
        std::fill_n(binarySizes.begin(), binariesCount, this->data.storage.size());
    }

    auto &getFlags() {
        return reinterpret_cast<Zebin::Elf::ZebinTargetFlags &>(this->data.elfHeader->flags);
    }

    void setAsMockCompilerReturnedBinary() {
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

    ZebinTestData::ZebinCopyBufferSimdModule<numBits> data;
    std::array<const unsigned char *, binariesCount> binaries;
    std::array<size_t, binariesCount> binarySizes;
    std::unique_ptr<void, void (*)(void *)> debugVarsRestore{nullptr, nullptr};
};
} // namespace NEO