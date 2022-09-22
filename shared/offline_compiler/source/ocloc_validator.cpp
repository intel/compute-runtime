/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_validator.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"

namespace NEO {
ProgramInfo::~ProgramInfo() {
    for (auto &kernelInfo : kernelInfos) {
        delete kernelInfo;
    }
    kernelInfos.clear();
}

KernelInfo::~KernelInfo() {
    delete[] crossThreadData;
}
} // namespace NEO

namespace Ocloc {
using namespace NEO;

int validate(const std::vector<std::string> &args, OclocArgHelper *argHelper) {
    NEO::ProgramInfo programInfo;
    std::string errors;
    std::string warnings;
    UNRECOVERABLE_IF(nullptr == argHelper)
    std::string fileName;
    for (uint32_t i = 0; i < args.size(); ++i) {
        if (args.size() > (i + 1) && (ConstStringRef("-file") == args[i])) {
            fileName = args[i + 1];
        }
    }
    if (fileName.empty()) {
        argHelper->printf("Error : Mandatory argument -file is missing.\n");
        return -1;
    }

    if (false == argHelper->fileExists(fileName)) {
        argHelper->printf("Error : Input file missing : %s\n", fileName.c_str());
        return -1;
    }

    auto fileData = argHelper->readBinaryFile(fileName);
    argHelper->printf("Validating : %s (%zd bytes).\n", fileName.c_str(), fileData.size());

    auto deviceBinary = ArrayRef<const uint8_t>::fromAny(fileData.data(), fileData.size());
    if (false == NEO::isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(deviceBinary)) {
        argHelper->printf("Input is not a Zebin file (not elf or wrong elf object file type)\n");
        return -2;
    }

    NEO::DecodeError decodeResult;
    if (NEO::Elf::isElf<NEO::Elf::EI_CLASS_32>(deviceBinary)) {
        auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_32>(deviceBinary, errors, warnings);
        decodeResult = NEO::decodeZebin(programInfo, elf, errors, warnings);
    } else {
        auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(deviceBinary, errors, warnings);
        decodeResult = NEO::decodeZebin(programInfo, elf, errors, warnings);
    }

    if (false == warnings.empty()) {
        argHelper->printf("Validator detected potential problems :\n%s\n", warnings.c_str());
    }
    if (false == errors.empty()) {
        argHelper->printf("Validator detected errors :\n%s\n", errors.c_str());
    }

    argHelper->printf("Binary is %s (%s).\n", ((NEO::DecodeError::Success == decodeResult) ? "VALID" : "INVALID"), NEO::asString(decodeResult));
    if (NEO::DecodeError::Success == decodeResult) {
        argHelper->printf("Statistics : \n");
        if (0 != programInfo.globalVariables.size) {
            argHelper->printf("Binary contains global variables section of size :  %zd.\n", programInfo.globalVariables.size);
        }
        if (0 != programInfo.globalConstants.size) {
            argHelper->printf("Binary contains global constants section of size :  %zd.\n", programInfo.globalConstants.size);
        }
        argHelper->printf("Binary contains %zd kernels.\n", programInfo.kernelInfos.size());
        for (size_t i = 0U; i < programInfo.kernelInfos.size(); ++i) {
            const auto &kernelDescriptor = programInfo.kernelInfos[i]->kernelDescriptor;
            argHelper->printf("\nKernel #%d named %s:\n", static_cast<int>(i), kernelDescriptor.kernelMetadata.kernelName.c_str());
            argHelper->printf(" * Number of binding table entries %d:\n", kernelDescriptor.payloadMappings.bindingTable.numEntries);
            argHelper->printf(" * Cross-thread data size %d:\n", kernelDescriptor.kernelAttributes.crossThreadDataSize);
            argHelper->printf(" * Per-thread data size %d:\n", kernelDescriptor.kernelAttributes.perThreadDataSize);
        }
    }

    return static_cast<int>(decodeResult);
}

} // namespace Ocloc
