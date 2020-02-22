/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/populate_built_ins.inl"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
namespace NEO {

BuiltInOp<EBuiltInOps::AuxTranslation>::BuiltInOp(BuiltIns &kernelsLib, Device &device) : BuiltinDispatchInfoBuilder(kernelsLib) {
    BuiltinDispatchInfoBuilder::populate(device, EBuiltInOps::AuxTranslation, "", "fullCopy", baseKernel);

    resizeKernelInstances(5);
}

void BuiltInOp<EBuiltInOps::AuxTranslation>::resizeKernelInstances(size_t size) const {
    convertToNonAuxKernel.reserve(size);
    convertToAuxKernel.reserve(size);

    for (size_t i = convertToNonAuxKernel.size(); i < size; i++) {
        auto clonedNonAuxToAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedNonAuxToAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::NonAuxToAux);

        auto clonedAuxToNonAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedAuxToNonAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::AuxToNonAux);

        clonedNonAuxToAuxKernel->cloneKernel(baseKernel);
        clonedAuxToNonAuxKernel->cloneKernel(baseKernel);

        convertToAuxKernel.emplace_back(clonedNonAuxToAuxKernel);
        convertToNonAuxKernel.emplace_back(clonedAuxToNonAuxKernel);
    }
}

} // namespace NEO
