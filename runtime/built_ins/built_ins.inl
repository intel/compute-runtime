/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/aux_translation_builtin.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {
template <typename... KernelsDescArgsT>
void BuiltinDispatchInfoBuilder::populate(Context &context, Device &device, EBuiltInOps op, const char *options, KernelsDescArgsT &&... desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltinCode::ECodeType::Any, device);
    prog.reset(BuiltinsLib::createProgramFromCode(src, context, device).release());
    prog->build(0, nullptr, options, nullptr, nullptr, kernelsLib.isCacheingEnabled());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}

template <typename HWFamily>
BuiltInOp<HWFamily, EBuiltInOps::AuxTranslation>::BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
    : BuiltinDispatchInfoBuilder(kernelsLib) {
    BuiltinDispatchInfoBuilder::populate(context, device, EBuiltInOps::AuxTranslation, "", "fullCopy", baseKernel);
    resizeKernelInstances(5);
}

template <typename HWFamily>
bool BuiltInOp<HWFamily, EBuiltInOps::AuxTranslation>::buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
    size_t kernelInstanceNumber = 0;
    resizeKernelInstances(operationParams.memObjsForAuxTranslation->size());

    for (auto &memObj : *operationParams.memObjsForAuxTranslation) {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::NoSplit> builder;
        auto graphicsAllocation = memObj->getGraphicsAllocation();
        size_t allocationSize = graphicsAllocation->getUnderlyingBufferSize();

        if (AuxTranslationDirection::AuxToNonAux == operationParams.auxTranslationDirection) {
            builder.setKernel(convertToNonAuxKernel.at(kernelInstanceNumber++).get());
            builder.setArg(0, memObj);
            builder.setArgSvm(1, allocationSize, reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), nullptr, 0u);
        } else {
            UNRECOVERABLE_IF(AuxTranslationDirection::NonAuxToAux != operationParams.auxTranslationDirection);
            builder.setKernel(convertToAuxKernel.at(kernelInstanceNumber++).get());
            builder.setArgSvm(0, allocationSize, reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), nullptr, 0u);
            builder.setArg(1, memObj);
        }

        size_t elementSize = sizeof(uint32_t) * 4;
        DEBUG_BREAK_IF(allocationSize < elementSize || !isAligned<4>(allocationSize));
        size_t xGws = allocationSize / elementSize;

        builder.setDispatchGeometry(Vec3<size_t>{xGws, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        builder.bake(multiDispatchInfo);
    }

    return true;
}

template <typename HWFamily>
void BuiltInOp<HWFamily, EBuiltInOps::AuxTranslation>::resizeKernelInstances(size_t size) const {
    convertToNonAuxKernel.reserve(size);
    convertToAuxKernel.reserve(size);

    for (size_t i = convertToNonAuxKernel.size(); i < size; i++) {
        auto clonedKernel1 = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedKernel1->setDisableL3forStatefulBuffers(true);
        auto clonedKernel2 = Kernel::create(baseKernel->getProgram(), baseKernel->getKernelInfo(), nullptr);
        clonedKernel1->cloneKernel(baseKernel);
        clonedKernel2->cloneKernel(baseKernel);

        convertToNonAuxKernel.emplace_back(clonedKernel1);
        convertToAuxKernel.emplace_back(clonedKernel2);
    }
}

} // namespace OCLRT
