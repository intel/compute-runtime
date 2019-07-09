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
        size_t numMemObjectsToTranslate = operationParams.memObjsForAuxTranslation->size();
        resizeKernelInstances(numMemObjectsToTranslate);
        multiDispatchInfo.setBuiltinOpParams(operationParams);

        for (auto &memObj : *operationParams.memObjsForAuxTranslation) {
            DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::NoSplit> builder;
            auto graphicsAllocation = memObj->getGraphicsAllocation();
            size_t allocationSize = alignUp(memObj->getSize(), 512);

            UNRECOVERABLE_IF(builder.getMaxNumDispatches() != 1);

            if (kernelInstanceNumber == 0) {
                // Before Kernel
                bool dcFlush = (AuxTranslationDirection::AuxToNonAux == operationParams.auxTranslationDirection);
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchInitCommands, dcFlush);
            }
            if (kernelInstanceNumber == numMemObjectsToTranslate - 1) {
                // After Kernel
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchEpilogueCommands, false);
            }

            if (AuxTranslationDirection::AuxToNonAux == operationParams.auxTranslationDirection) {
                builder.setKernel(convertToNonAuxKernel[kernelInstanceNumber++].get());
                builder.setArg(0, memObj);
                builder.setArgSvm(1, allocationSize, reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), nullptr, 0u);
            } else {
                UNRECOVERABLE_IF(AuxTranslationDirection::NonAuxToAux != operationParams.auxTranslationDirection);
                builder.setKernel(convertToAuxKernel[kernelInstanceNumber++].get());
                builder.setArgSvm(0, allocationSize, reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), nullptr, 0u);
                builder.setArg(1, memObj);
            }

            size_t xGws = allocationSize / 16;

            builder.setDispatchGeometry(Vec3<size_t>{xGws, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            builder.bake(multiDispatchInfo);
        }

        return true;
    }

  protected:
    template <typename GfxFamily>
    void registerPipeControlProgramming(RegisteredMethodDispatcher<DispatchInfo::DispatchCommandMethodT> &dispatcher, bool dcFlush) const {
        auto method = std::bind(PipeControlHelper<GfxFamily>::addPipeControl, std::placeholders::_1, dcFlush);
        dispatcher.registerMethod(method);
        dispatcher.registerCommandsSizeEstimationMethod(PipeControlHelper<GfxFamily>::getSizeForSinglePipeControl);
    }
    void resizeKernelInstances(size_t size) const;
    Kernel *baseKernel = nullptr;
    mutable std::vector<std::unique_ptr<Kernel>> convertToNonAuxKernel;
    mutable std::vector<std::unique_ptr<Kernel>> convertToAuxKernel;
};
} // namespace NEO
