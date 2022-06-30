/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_validator.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
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

namespace Ocloc {

int validate(const std::vector<std::string> &args, OclocArgHelper *argHelper) {
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary deviceBinary;
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
    argHelper->printf("Validating : %s (%d bytes).\n", fileName.c_str(), fileData.size());

    deviceBinary.deviceBinary = deviceBinary.deviceBinary.fromAny(fileData.data(), fileData.size());
    if (false == NEO::isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(deviceBinary.deviceBinary)) {
        argHelper->printf("Input is not a Zebin file (not elf or wrong elf object file type)\n", errors.c_str());
        return -2;
    }
    auto decodeResult = NEO::decodeSingleDeviceBinary<DeviceBinaryFormat::Zebin>(programInfo, deviceBinary, errors, warnings);
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
            argHelper->printf("Binary contains global variables section of size :  %d.\n", programInfo.globalVariables.size);
        }
        if (0 != programInfo.globalConstants.size) {
            argHelper->printf("Binary contains global constants section of size :  %d.\n", programInfo.globalConstants.size);
        }
        argHelper->printf("Binary contains %d kernels.\n", programInfo.kernelInfos.size());
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

} // namespace NEO
