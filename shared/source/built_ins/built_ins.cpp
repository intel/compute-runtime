/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/helpers/built_ins_helper.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/dispatch_info_builder.h"

#include "compiler_options.h"

#include <cstdint>
#include <sstream>

namespace NEO {

BuiltIns::BuiltIns() {
    builtinsLib.reset(new BuiltinsLib());
}

BuiltIns::~BuiltIns() = default;

const SipKernel &BuiltIns::getSipKernel(SipKernelType type, Device &device) {
    uint32_t kernelId = static_cast<uint32_t>(type);
    UNRECOVERABLE_IF(kernelId >= static_cast<uint32_t>(SipKernelType::COUNT));
    auto &sipBuiltIn = this->sipKernels[kernelId];

    auto initializer = [&] {
        std::vector<char> sipBinary;
        auto compilerInteface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInteface == nullptr);

        auto ret = compilerInteface->getSipKernelBinary(device, type, sipBinary);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);

        ProgramInfo programInfo;
        auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(sipBinary.data()), sipBinary.size());
        SingleDeviceBinary deviceBinary = {};
        deviceBinary.deviceBinary = blob;
        std::string decodeErrors;
        std::string decodeWarnings;

        DecodeError decodeError;
        DeviceBinaryFormat singleDeviceBinaryFormat;
        std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, deviceBinary, decodeErrors, decodeWarnings);
        UNRECOVERABLE_IF(DecodeError::Success != decodeError);

        auto success = programInfo.kernelInfos[0]->createKernelAllocation(device, true);
        UNRECOVERABLE_IF(!success);

        sipBuiltIn.first.reset(new SipKernel(type, programInfo.kernelInfos[0]->kernelAllocation));
        programInfo.kernelInfos[0]->kernelAllocation = nullptr;
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

void BuiltIns::freeSipKernels(MemoryManager *memoryManager) {
    for (auto &sipKernel : sipKernels) {
        if (sipKernel.first.get()) {
            memoryManager->freeGraphicsMemory(sipKernel.first->getSipAllocation());
        }
    }
}

} // namespace NEO
