/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

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
        int retVal = 0;

        std::vector<char> sipBinary;
        auto compilerInteface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInteface == nullptr);

        auto ret = compilerInteface->getSipKernelBinary(device, type, sipBinary);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);
        auto program = createProgramForSip(*device.getExecutionEnvironment(),
                                           nullptr,
                                           sipBinary,
                                           sipBinary.size(),
                                           &retVal,
                                           &device);
        DEBUG_BREAK_IF(retVal != 0);
        UNRECOVERABLE_IF(program == nullptr);

        program->setDevice(&device);

        retVal = program->processGenBinary();
        DEBUG_BREAK_IF(retVal != 0);

        sipBuiltIn.first.reset(new SipKernel(type, program));
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

} // namespace NEO
