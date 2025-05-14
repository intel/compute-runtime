/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"
#include "opencl/source/mem_obj/buffer.h"

#include <memory>

namespace NEO {
template <>
class BuiltInOp<EBuiltInOps::auxTranslation> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device);
    template <typename GfxFamily>
    bool buildDispatchInfosForAuxTranslation(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        size_t kernelInstanceNumber = 0;
        size_t numKernelObjectsToTranslate = multiDispatchInfo.getKernelObjsForAuxTranslation()->size();
        resizeKernelInstances(numKernelObjectsToTranslate);
        multiDispatchInfo.setBuiltinOpParams(operationParams);

        for (auto &kernelObj : *multiDispatchInfo.getKernelObjsForAuxTranslation()) {
            DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit> builder(clDevice);

            UNRECOVERABLE_IF(builder.getMaxNumDispatches() != 1);

            if (kernelInstanceNumber == 0) {
                // Before Kernel
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchInitCommands, true);
            }
            if (kernelInstanceNumber == numKernelObjectsToTranslate - 1) {
                // After Kernel
                registerPipeControlProgramming<GfxFamily>(builder.getDispatchInfo(0).dispatchEpilogueCommands, false);
            }

            if (AuxTranslationDirection::auxToNonAux == operationParams.auxTranslationDirection) {
                builder.setKernel(convertToNonAuxKernel[kernelInstanceNumber++].get());
            } else {
                UNRECOVERABLE_IF(AuxTranslationDirection::nonAuxToAux != operationParams.auxTranslationDirection);
                builder.setKernel(convertToAuxKernel[kernelInstanceNumber++].get());
            }

            size_t allocationSize = 0;
            if (kernelObj.type == KernelObjForAuxTranslation::Type::memObj) {
                auto buffer = static_cast<Buffer *>(kernelObj.object);
                builder.setArg(0, buffer);
                builder.setArg(1, buffer);
                allocationSize = alignUp(buffer->getSize(), 512);
            } else {
                DEBUG_BREAK_IF(kernelObj.type != KernelObjForAuxTranslation::Type::gfxAlloc);
                auto svmAlloc = static_cast<GraphicsAllocation *>(kernelObj.object);
                auto svmPtr = reinterpret_cast<void *>(svmAlloc->getGpuAddressToPatch());
                builder.setArgSvmAlloc(0, svmPtr, svmAlloc);
                builder.setArgSvmAlloc(1, svmPtr, svmAlloc);
                allocationSize = alignUp(svmAlloc->getUnderlyingBufferSize(), 512);
            }

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
    static void dispatchPipeControl(LinearStream &linearStream, TimestampPacketDependencies *, const RootDeviceEnvironment &rootDeviceEnvironment) {
        PipeControlArgs args;
        args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(dcFlush, rootDeviceEnvironment);
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(linearStream, args);
    }

    template <typename GfxFamily>
    static size_t getSizeForSinglePipeControl(size_t, const RootDeviceEnvironment &rootDeviceEnvironment, bool) {
        return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }

    template <typename GfxFamily>
    void registerPipeControlProgramming(RegisteredMethodDispatcherT &dispatcher, bool dcFlush) const {
        if (dcFlush) {
            dispatcher.registerMethod(this->dispatchPipeControl<GfxFamily, true>);
        } else {
            dispatcher.registerMethod(this->dispatchPipeControl<GfxFamily, false>);
        }
        dispatcher.registerCommandsSizeEstimationMethod(this->getSizeForSinglePipeControl<GfxFamily>);
    }

    void resizeKernelInstances(size_t size) const;
    MultiDeviceKernel *multiDeviceBaseKernel = nullptr;
    Kernel *baseKernel = nullptr;
    mutable std::vector<std::unique_ptr<Kernel>> convertToNonAuxKernel;
    mutable std::vector<std::unique_ptr<Kernel>> convertToAuxKernel;
};
} // namespace NEO
