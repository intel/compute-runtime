/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/helpers/dispatch_info_builder.h"
#include "runtime/helpers/hw_helper.h"

#include <memory>

namespace NEO {
template <>
class BuiltInOp<EBuiltInOps::AuxTranslation> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device);
    template <typename GfxFamily>
    bool buildDispatchInfosForAuxTranslation(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        size_t kernelInstanceNumber = 0;
        size_t numMemObjectsToTranslate = multiDispatchInfo.getMemObjsForAuxTranslation()->size();
        resizeKernelInstances(numMemObjectsToTranslate);
        multiDispatchInfo.setBuiltinOpParams(operationParams);

        for (auto &memObj : *multiDispatchInfo.getMemObjsForAuxTranslation()) {
            DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::NoSplit> builder;
            size_t allocationSize = alignUp(memObj->getSize(), 512);

            UNRECOVERABLE_IF(builder.getMaxNumDispatches() != 1);

            if (kernelInstanceNumber == 0) {
                // Before Kernel
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchInitCommands, true);
            }
            if (kernelInstanceNumber == numMemObjectsToTranslate - 1) {
                // After Kernel
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchEpilogueCommands, false);
            }

            if (AuxTranslationDirection::AuxToNonAux == operationParams.auxTranslationDirection) {
                builder.setKernel(convertToNonAuxKernel[kernelInstanceNumber++].get());
            } else {
                UNRECOVERABLE_IF(AuxTranslationDirection::NonAuxToAux != operationParams.auxTranslationDirection);
                builder.setKernel(convertToAuxKernel[kernelInstanceNumber++].get());
            }

            builder.setArg(0, memObj);
            builder.setArg(1, memObj);

            size_t xGws = allocationSize / 16;

            builder.setDispatchGeometry(Vec3<size_t>{xGws, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            builder.bake(multiDispatchInfo);
        }

        return true;
    }

  protected:
    using RegisteredMethodDispatcherT = RegisteredMethodDispatcher<DispatchInfo::DispatchCommandMethodT,
                                                                   DispatchInfo::EstimateCommandsMethodT>;

    template <typename GfxFamily, bool dcFlush>
    static void dispatchPipeControl(LinearStream &linearStream) {
        PipeControlHelper<GfxFamily>::addPipeControl(linearStream, dcFlush);
    }

    template <typename GfxFamily>
    void registerPipeControlProgramming(RegisteredMethodDispatcherT &dispatcher, bool dcFlush) const {
        if (dcFlush) {
            dispatcher.registerMethod(this->dispatchPipeControl<GfxFamily, true>);
        } else {
            dispatcher.registerMethod(this->dispatchPipeControl<GfxFamily, false>);
        }
        dispatcher.registerCommandsSizeEstimationMethod(PipeControlHelper<GfxFamily>::getSizeForSinglePipeControl);
    }

    void resizeKernelInstances(size_t size) const;
    Kernel *baseKernel = nullptr;
    mutable std::vector<std::unique_ptr<Kernel>> convertToNonAuxKernel;
    mutable std::vector<std::unique_ptr<Kernel>> convertToAuxKernel;
};
} // namespace NEO
