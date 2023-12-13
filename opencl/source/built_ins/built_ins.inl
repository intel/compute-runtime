/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/populate_built_ins.inl"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
namespace NEO {

BuiltInOp<EBuiltInOps::auxTranslation>::BuiltInOp(BuiltIns &kernelsLib, ClDevice &device) : BuiltinDispatchInfoBuilder(kernelsLib, device) {
    BuiltinDispatchInfoBuilder::populate(EBuiltInOps::auxTranslation, "", "fullCopy", multiDeviceBaseKernel);
    baseKernel = multiDeviceBaseKernel->getKernel(clDevice.getRootDeviceIndex());
    resizeKernelInstances(5);
}

void BuiltInOp<EBuiltInOps::auxTranslation>::resizeKernelInstances(size_t size) const {
    convertToNonAuxKernel.reserve(size);
    convertToAuxKernel.reserve(size);

    for (size_t i = convertToNonAuxKernel.size(); i < size; i++) {
        cl_int retVal{CL_SUCCESS};
        auto clonedNonAuxToAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), clDevice, retVal);
        UNRECOVERABLE_IF(CL_SUCCESS != retVal);
        clonedNonAuxToAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::nonAuxToAux);

        auto clonedAuxToNonAuxKernel = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), clDevice, retVal);
        UNRECOVERABLE_IF(CL_SUCCESS != retVal);
        clonedAuxToNonAuxKernel->setAuxTranslationDirection(AuxTranslationDirection::auxToNonAux);

        clonedNonAuxToAuxKernel->cloneKernel(baseKernel);
        clonedAuxToNonAuxKernel->cloneKernel(baseKernel);

        convertToAuxKernel.emplace_back(clonedNonAuxToAuxKernel);
        convertToNonAuxKernel.emplace_back(clonedAuxToNonAuxKernel);
    }
}

} // namespace NEO
