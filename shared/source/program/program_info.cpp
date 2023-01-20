/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info.h"

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/program/kernel_info.h"

namespace NEO {

ProgramInfo::~ProgramInfo() {
    for (auto &kernelInfo : kernelInfos) {
        delete kernelInfo;
    }
    kernelInfos.clear();
}

void ProgramInfo::prepareLinkerInputStorage() {
    if (this->linkerInput == nullptr) {
        this->linkerInput = std::make_unique<LinkerInput>();
    }
}

size_t getMaxInlineSlmNeeded(const ProgramInfo &programInfo) {
    uint32_t ret = 0U;
    for (const auto &kernelInfo : programInfo.kernelInfos) {
        ret = std::max(ret, kernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize);
    }
    return ret;
}

bool requiresLocalMemoryWindowVA(const ProgramInfo &programInfo) {
    for (const auto &kernelInfo : programInfo.kernelInfos) {
        if (isValidOffset(kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres)) {
            return true;
        }
    }
    return false;
}

bool isRebuiltToPatchtokensRequired(Device *neoDevice, ArrayRef<const uint8_t> archive, std::string &optionsString, bool isBuiltin, bool isVmeUsed) {
    if (isBuiltin) {
        return false;
    }
    auto isSourceLevelDebuggerActive = (nullptr != neoDevice->getSourceLevelDebugger());
    auto isZebinFormat = NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(archive);
    if ((isSourceLevelDebuggerActive || isVmeUsed) && isZebinFormat) {
        auto pos = optionsString.find(NEO::CompilerOptions::allowZebin.str());
        optionsString.erase(pos, pos + NEO::CompilerOptions::allowZebin.length());
        optionsString += " " + NEO::CompilerOptions::disableZebin.str();
        return true;
    }
    return false;
}

} // namespace NEO
