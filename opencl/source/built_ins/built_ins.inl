/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/populate_built_ins.inl"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
namespace NEO {

BuiltInOp<EBuiltInOps::AuxTranslation>::BuiltInOp(BuiltIns &kernelsLib, ClDevice &device) : BuiltinDispatchInfoBuilder(kernelsLib, device) {
    BuiltinDispatchInfoBuilder::populate(EBuiltInOps::AuxTranslation, "", "fullCopy", multiDeviceBaseKernel);
    baseKernel = multiDeviceBaseKernel->getKernel(clDevice.getRootDeviceIndex());
    resizeKernelInstances(5);
}

void BuiltInOp<EBuiltInOps::AuxTranslation>::resizeKernelInstances(size_t size) const {
    convertToNonAuxKernel.reserve(size);
    convertToAuxKernel.reserve(size);

    for (size_t i = convertToNonAuxKernel.size(); i < size; i++) {
        auto clonedNonAuxToAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), clDevice, nullptr);
        clonedNonAuxToAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::NonAuxToAux);

        auto clonedAuxToNonAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), clDevice, nullptr);
        clonedAuxToNonAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::AuxToNonAux);

        clonedNonAuxToAuxKernel->cloneKernel(baseKernel);
        clonedAuxToNonAuxKernel->cloneKernel(baseKernel);

        convertToAuxKernel.emplace_back(clonedNonAuxToAuxKernel);
        convertToNonAuxKernel.emplace_back(clonedAuxToNonAuxKernel);
    }
}

} // namespace NEO
