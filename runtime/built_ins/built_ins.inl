/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/aux_translation_builtin.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"

namespace NEO {
template <typename... KernelsDescArgsT>
void BuiltinDispatchInfoBuilder::populate(Context &context, Device &device, EBuiltInOps::Type op, const char *options, KernelsDescArgsT &&... desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltinCode::ECodeType::Any, device);
    prog.reset(BuiltinsLib::createProgramFromCode(src, context, device).release());
    prog->build(0, nullptr, options, nullptr, nullptr, kernelsLib.isCacheingEnabled());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}

BuiltInOp<EBuiltInOps::AuxTranslation>::BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device) : BuiltinDispatchInfoBuilder(kernelsLib) {
    BuiltinDispatchInfoBuilder::populate(context, device, EBuiltInOps::AuxTranslation, "", "fullCopy", baseKernel);
    resizeKernelInstances(5);
}

void BuiltInOp<EBuiltInOps::AuxTranslation>::resizeKernelInstances(size_t size) const {
    convertToNonAuxKernel.reserve(size);
    convertToAuxKernel.reserve(size);

    for (size_t i = convertToNonAuxKernel.size(); i < size; i++) {
        auto clonedKernel1 = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedKernel1->setAuxTranslationFlag(true);
        auto clonedKernel2 = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedKernel2->setAuxTranslationFlag(true);
        clonedKernel1->cloneKernel(baseKernel);
        clonedKernel2->cloneKernel(baseKernel);

        convertToNonAuxKernel.emplace_back(clonedKernel1);
        convertToAuxKernel.emplace_back(clonedKernel2);
    }
}

} // namespace NEO
