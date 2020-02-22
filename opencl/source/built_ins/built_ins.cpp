/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "built_ins/built_ins.h"

#include "core/compiler_interface/compiler_interface.h"
#include "core/helpers/basic_math.h"
#include "core/helpers/debug_helpers.h"

#include "built_ins/aux_translation_builtin.h"
#include "built_ins/sip.h"
#include "compiler_options.h"
#include "device/cl_device.h"
#include "helpers/built_ins_helper.h"
#include "helpers/convert_color.h"
#include "helpers/dispatch_info_builder.h"
#include "kernel/kernel.h"
#include "mem_obj/image.h"
#include "program/program.h"

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
        cl_int retVal = CL_SUCCESS;

        std::vector<char> sipBinary;
        auto compilerInteface = device.getExecutionEnvironment()->getCompilerInterface();
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
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        UNRECOVERABLE_IF(program == nullptr);

        program->setDevice(&device);

        retVal = program->processGenBinary();
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        sipBuiltIn.first.reset(new SipKernel(type, program));
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

BuiltInOwnershipWrapper::BuiltInOwnershipWrapper(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    takeOwnership(inputBuilder, context);
}
BuiltInOwnershipWrapper::~BuiltInOwnershipWrapper() {
    if (builder) {
        for (auto &kernel : builder->peekUsedKernels()) {
            kernel->setContext(nullptr);
            kernel->releaseOwnership();
        }
    }
}
void BuiltInOwnershipWrapper::takeOwnership(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    UNRECOVERABLE_IF(builder);
    builder = &inputBuilder;
    for (auto &kernel : builder->peekUsedKernels()) {
        kernel->takeOwnership();
        kernel->setContext(context);
    }
}

} // namespace NEO
