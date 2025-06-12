/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/builtins_dispatch_builder.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/image_helper.h"

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/built_ins.inl"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/program/program.h"

#include <cstdint>

namespace NEO {
template <>
class BuiltInOp<EBuiltInOps::copyBufferToBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}
    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit> kernelSplit1DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        uintptr_t start = reinterpret_cast<uintptr_t>(operationParams.dstPtr) + operationParams.dstOffset.x;

        size_t middleAlignment = MemoryConstants::cacheLineSize;
        size_t middleElSize = sizeof(uint32_t) * 4;

        uintptr_t leftSize = start % middleAlignment;
        leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0; // calc left leftover size
        leftSize = std::min(leftSize, operationParams.size.x);        // clamp left leftover size to requested size

        uintptr_t rightSize = (start + operationParams.size.x) % middleAlignment; // calc right leftover size
        rightSize = std::min(rightSize, operationParams.size.x - leftSize);       // clamp

        uintptr_t middleSizeBytes = operationParams.size.x - leftSize - rightSize; // calc middle size

        // corner case - fully optimized kernel requires DWORD alignment. If we don't have it, run slower, misaligned kernel
        const auto srcMiddleStart = reinterpret_cast<uintptr_t>(operationParams.srcPtr) + operationParams.srcOffset.x + leftSize;
        const auto srcMisalignment = srcMiddleStart % sizeof(uint32_t);
        const auto isSrcMisaligned = srcMisalignment != 0u;

        auto middleSizeEls = middleSizeBytes / middleElSize; // num work items in middle walker

        uint32_t rootDeviceIndex = clDevice.getRootDeviceIndex();

        // Set-up ISA
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::left, kernLeftLeftover->getKernel(rootDeviceIndex));
        if (isSrcMisaligned) {
            kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::middle, kernMiddleMisaligned->getKernel(rootDeviceIndex));
        } else {
            kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::middle, kernMiddle->getKernel(rootDeviceIndex));
        }
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::right, kernRightLeftover->getKernel(rootDeviceIndex));

        // Set-up common kernel args
        if (operationParams.srcSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(0, operationParams.srcPtr, operationParams.srcSvmAlloc);
        } else if (operationParams.srcMemObj) {
            kernelSplit1DBuilder.setArg(0, operationParams.srcMemObj);
        } else {
            kernelSplit1DBuilder.setArgSvm(0, operationParams.size.x + operationParams.srcOffset.x, operationParams.srcPtr, nullptr, CL_MEM_READ_ONLY);
        }

        bool isDestinationInSystem = false;
        if (operationParams.dstSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(1, operationParams.dstPtr, operationParams.dstSvmAlloc);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstSvmAlloc->getAllocationType());
        } else if (operationParams.dstMemObj) {
            kernelSplit1DBuilder.setArg(1, operationParams.dstMemObj);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
        } else {
            kernelSplit1DBuilder.setArgSvm(1, operationParams.size.x + operationParams.dstOffset.x, operationParams.dstPtr, nullptr, 0u);
            isDestinationInSystem = operationParams.dstPtr != nullptr;
        }
        kernelSplit1DBuilder.setKernelDestinationArgumentInSystem(isDestinationInSystem);

        kernelSplit1DBuilder.setUnifiedMemorySyncRequirement(operationParams.unifiedMemoryArgsRequireMemSync);

        // Set-up srcOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::left, 2, static_cast<OffsetType>(operationParams.srcOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::right, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize + middleSizeBytes));

        // Set-up dstOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::left, 3, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::right, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        if (isSrcMisaligned) {
            kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 4, static_cast<uint32_t>(srcMisalignment * 8));
        }

        // Set-up work sizes
        // Note for split walker, it would be just builder.SetDipatchGeometry(GWS, ELWS, OFFSET)
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::left, Vec3<size_t>{leftSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::middle, Vec3<size_t>{middleSizeEls, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::right, Vec3<size_t>{rightSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.bake(multiDispatchInfo);

        return true;
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernLeftLeftover = nullptr;
    MultiDeviceKernel *kernMiddle = nullptr;
    MultiDeviceKernel *kernMiddleMisaligned = nullptr;
    MultiDeviceKernel *kernRightLeftover = nullptr;
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::copyBufferToBuffer,
                     "",
                     "CopyBufferToBufferLeftLeftover", kernLeftLeftover,
                     "CopyBufferToBufferMiddle", kernMiddle,
                     "CopyBufferToBufferMiddleMisaligned", kernMiddleMisaligned,
                     "CopyBufferToBufferRightLeftover", kernRightLeftover);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferToBufferStateless> : public BuiltInOp<EBuiltInOps::copyBufferToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToBufferLeftLeftoverStateless", kernLeftLeftover,
                 "CopyBufferToBufferMiddleStateless", kernMiddle,
                 "CopyBufferToBufferMiddleMisalignedStateless", kernMiddleMisaligned,
                 "CopyBufferToBufferRightLeftoverStateless", kernRightLeftover);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferToBufferStatelessHeapless> : public BuiltInOp<EBuiltInOps::copyBufferToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferToBufferStatelessHeapless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToBufferLeftLeftoverStateless", kernLeftLeftover,
                 "CopyBufferToBufferMiddleStateless", kernMiddle,
                 "CopyBufferToBufferMiddleMisalignedStateless", kernMiddleMisaligned,
                 "CopyBufferToBufferRightLeftoverStateless", kernRightLeftover);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferRect> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        size_t hostPtrSize = 0;
        size_t srcOffsetFromAlignedPtr = 0;
        size_t dstOffsetFromAlignedPtr = 0;
        bool is3D = false;
        auto srcPtr = operationParams.srcPtr;
        auto dstPtr = operationParams.dstPtr;

        if (operationParams.srcMemObj && operationParams.dstMemObj) {
            DEBUG_BREAK_IF(!((srcPtr == nullptr) && (dstPtr == nullptr)));
            is3D = (operationParams.size.z > 1) || (operationParams.srcOffset.z > 0) || (operationParams.dstOffset.z > 0);
        } else {
            if (srcPtr) {
                size_t origin[] = {operationParams.srcOffset.x, operationParams.srcOffset.y, operationParams.srcOffset.z};
                size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};
                hostPtrSize = Buffer::calculateHostPtrSize(origin, region, operationParams.srcRowPitch, operationParams.srcSlicePitch);
                is3D = (operationParams.size.z > 1) || (operationParams.dstOffset.z > 0);
                if (!is3D) {
                    auto srcPtrOffset = ptrOffset(srcPtr, operationParams.srcOffset.z * operationParams.srcSlicePitch);
                    srcPtr = alignDown(srcPtrOffset, 4);
                    srcOffsetFromAlignedPtr = ptrDiff(srcPtrOffset, srcPtr);
                }
            } else if (dstPtr) {
                size_t origin[] = {operationParams.dstOffset.x, operationParams.dstOffset.y, operationParams.dstOffset.z};
                size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};
                hostPtrSize = Buffer::calculateHostPtrSize(origin, region, operationParams.dstRowPitch, operationParams.dstSlicePitch);
                is3D = (operationParams.size.z > 1) || (operationParams.srcOffset.z > 0);
                if (!is3D) {
                    auto dstPtrOffset = ptrOffset(dstPtr, operationParams.dstOffset.z * operationParams.dstSlicePitch);
                    dstPtr = alignDown(dstPtrOffset, 4);
                    dstOffsetFromAlignedPtr = ptrDiff(dstPtrOffset, dstPtr);
                }
            } else {
                DEBUG_BREAK_IF(!false);
            }
        }

        const uint32_t rootDeviceIndex = clDevice.getRootDeviceIndex();
        const int dimensions = is3D ? 3 : 2;

        if (this->clDevice.getProductHelper().isCopyBufferRectSplitSupported()) {
            DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit> kernelSplit3DBuilder(clDevice);

            const auto totalSize = operationParams.size.x * operationParams.size.y * operationParams.size.z;
            if (totalSize == 0u) {
                return true;
            }

            const uintptr_t start = reinterpret_cast<uintptr_t>(dstPtr) + operationParams.dstOffset.x;

            constexpr size_t middleAlignment = MemoryConstants::cacheLineSize;
            constexpr size_t middleElSize = sizeof(uint32_t) * 4;

            uintptr_t leftSize = start % middleAlignment;
            leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0; // calc left leftover size
            leftSize = std::min(leftSize, operationParams.size.x);        // clamp left leftover size to requested size

            uintptr_t rightSize = (start + operationParams.size.x) % middleAlignment; // calc right leftover size
            rightSize = std::min(rightSize, operationParams.size.x - leftSize);       // clamp

            const uintptr_t middleSizeBytes = (operationParams.size.x > leftSize + rightSize) ? operationParams.size.x - leftSize - rightSize : 0u; // calc middle size

            // corner case - fully optimized kernel requires DWORD alignment. If we don't have it, run slower, misaligned kernel
            const auto srcMiddleStart = reinterpret_cast<uintptr_t>(srcPtr) + operationParams.srcOffset.x + leftSize;
            const auto srcMisalignment = srcMiddleStart % sizeof(uint32_t);
            const auto srcRowPitchMisalignment = operationParams.srcRowPitch % sizeof(uint32_t);
            const auto srcSlicePitchMisalignment = operationParams.srcSlicePitch % sizeof(uint32_t);
            const auto dstRowPitchMisalignment = operationParams.dstRowPitch % sizeof(uint32_t);
            const auto dstSlicePitchMisalignment = operationParams.dstSlicePitch % sizeof(uint32_t);
            const auto isSrcMisaligned = srcMisalignment != 0u || srcRowPitchMisalignment != 0u || srcSlicePitchMisalignment != 0u || dstRowPitchMisalignment != 0u || dstSlicePitchMisalignment != 0u;
            ;

            const auto middleSizeEls = middleSizeBytes / middleElSize; // num work items in middle walker

            // Set-up ISA

            kernelSplit3DBuilder.setKernel(SplitDispatch::RegionCoordX::left, kernelLeft[dimensions - 1]->getKernel(rootDeviceIndex));
            if (isSrcMisaligned) {
                kernelSplit3DBuilder.setKernel(SplitDispatch::RegionCoordX::middle, kernelBytes[dimensions - 1]->getKernel(rootDeviceIndex));
            } else {
                kernelSplit3DBuilder.setKernel(SplitDispatch::RegionCoordX::middle, kernelMiddle[dimensions - 1]->getKernel(rootDeviceIndex));
            }
            kernelSplit3DBuilder.setKernel(SplitDispatch::RegionCoordX::right, kernelRight[dimensions - 1]->getKernel(rootDeviceIndex));

            // arg0 = src
            if (operationParams.srcMemObj) {
                kernelSplit3DBuilder.setArg(0, operationParams.srcMemObj);
            } else {
                kernelSplit3DBuilder.setArgSvm(0, hostPtrSize, srcPtr, nullptr, CL_MEM_READ_ONLY);
            }

            bool isDestinationInSystem = false;
            // arg1 = dst
            if (operationParams.dstMemObj) {
                kernelSplit3DBuilder.setArg(1, operationParams.dstMemObj);
                isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
            } else {
                kernelSplit3DBuilder.setArgSvm(1, hostPtrSize, dstPtr, nullptr, 0u);
                isDestinationInSystem = dstPtr != nullptr;
            }
            kernelSplit3DBuilder.setKernelDestinationArgumentInSystem(isDestinationInSystem);

            // arg2 = srcOrigin
            OffsetType kSrcOrigin[4] = {static_cast<OffsetType>(operationParams.srcOffset.x + srcOffsetFromAlignedPtr), static_cast<OffsetType>(operationParams.srcOffset.y), static_cast<OffsetType>(operationParams.srcOffset.z), 0};
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::left, 2, sizeof(OffsetType) * 4, kSrcOrigin);
            kSrcOrigin[0] += static_cast<uint32_t>(leftSize);
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 2, sizeof(OffsetType) * 4, kSrcOrigin);
            kSrcOrigin[0] += static_cast<uint32_t>(middleSizeBytes);
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::right, 2, sizeof(OffsetType) * 4, kSrcOrigin);

            // arg3 = dstOrigin
            OffsetType kDstOrigin[4] = {static_cast<OffsetType>(operationParams.dstOffset.x + dstOffsetFromAlignedPtr), static_cast<OffsetType>(operationParams.dstOffset.y), static_cast<OffsetType>(operationParams.dstOffset.z), 0};
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::left, 3, sizeof(OffsetType) * 4, kDstOrigin);
            kDstOrigin[0] += static_cast<uint32_t>(leftSize);
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 3, sizeof(OffsetType) * 4, kDstOrigin);
            kDstOrigin[0] += static_cast<uint32_t>(middleSizeBytes);
            kernelSplit3DBuilder.setArg(SplitDispatch::RegionCoordX::right, 3, sizeof(OffsetType) * 4, kDstOrigin);

            // arg4 = srcPitch
            OffsetType kSrcPitch[2] = {static_cast<OffsetType>(operationParams.srcRowPitch), static_cast<OffsetType>(operationParams.srcSlicePitch)};
            kernelSplit3DBuilder.setArg(4, sizeof(OffsetType) * 2, kSrcPitch);

            // arg5 = dstPitch
            OffsetType kDstPitch[2] = {static_cast<OffsetType>(operationParams.dstRowPitch), static_cast<OffsetType>(operationParams.dstSlicePitch)};
            kernelSplit3DBuilder.setArg(5, sizeof(OffsetType) * 2, kDstPitch);

            // Set-up work sizes
            kernelSplit3DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::left, Vec3<size_t>{leftSize, operationParams.size.y, operationParams.size.z}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            kernelSplit3DBuilder.getDispatchInfo(0u).setDim(0u);

            kernelSplit3DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::middle, Vec3<size_t>{isSrcMisaligned ? middleSizeBytes : middleSizeEls, operationParams.size.y, operationParams.size.z}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            kernelSplit3DBuilder.getDispatchInfo(1u).setDim(0u);

            kernelSplit3DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::right, Vec3<size_t>{rightSize, operationParams.size.y, operationParams.size.z}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            kernelSplit3DBuilder.getDispatchInfo(2u).setDim(0u);

            kernelSplit3DBuilder.bake(multiDispatchInfo);

            UNRECOVERABLE_IF(leftSize + rightSize + middleSizeEls * middleElSize != operationParams.size.x);

            return true;
        } else {
            DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> kernelNoSplit3DBuilder(clDevice);

            // Set-up ISA
            kernelNoSplit3DBuilder.setKernel(kernelBytes[dimensions - 1]->getKernel(rootDeviceIndex));

            // arg0 = src
            if (operationParams.srcMemObj) {
                kernelNoSplit3DBuilder.setArg(0, operationParams.srcMemObj);
            } else {
                kernelNoSplit3DBuilder.setArgSvm(0, hostPtrSize, srcPtr, nullptr, CL_MEM_READ_ONLY);
            }

            bool isDestinationInSystem = false;
            // arg1 = dst
            if (operationParams.dstMemObj) {
                kernelNoSplit3DBuilder.setArg(1, operationParams.dstMemObj);
                isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
            } else {
                kernelNoSplit3DBuilder.setArgSvm(1, hostPtrSize, dstPtr, nullptr, 0u);
                isDestinationInSystem = dstPtr != nullptr;
            }
            kernelNoSplit3DBuilder.setKernelDestinationArgumentInSystem(isDestinationInSystem);

            // arg2 = srcOrigin
            OffsetType kSrcOrigin[4] = {static_cast<OffsetType>(operationParams.srcOffset.x + srcOffsetFromAlignedPtr), static_cast<OffsetType>(operationParams.srcOffset.y), static_cast<OffsetType>(operationParams.srcOffset.z), 0};
            kernelNoSplit3DBuilder.setArg(2, sizeof(OffsetType) * 4, kSrcOrigin);

            // arg3 = dstOrigin
            OffsetType kDstOrigin[4] = {static_cast<OffsetType>(operationParams.dstOffset.x + dstOffsetFromAlignedPtr), static_cast<OffsetType>(operationParams.dstOffset.y), static_cast<OffsetType>(operationParams.dstOffset.z), 0};
            kernelNoSplit3DBuilder.setArg(3, sizeof(OffsetType) * 4, kDstOrigin);

            // arg4 = srcPitch
            OffsetType kSrcPitch[2] = {static_cast<OffsetType>(operationParams.srcRowPitch), static_cast<OffsetType>(operationParams.srcSlicePitch)};
            kernelNoSplit3DBuilder.setArg(4, sizeof(OffsetType) * 2, kSrcPitch);

            // arg5 = dstPitch
            OffsetType kDstPitch[2] = {static_cast<OffsetType>(operationParams.dstRowPitch), static_cast<OffsetType>(operationParams.dstSlicePitch)};
            kernelNoSplit3DBuilder.setArg(5, sizeof(OffsetType) * 2, kDstPitch);

            // Set-up work sizes
            kernelNoSplit3DBuilder.setDispatchGeometry(operationParams.size, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
            kernelNoSplit3DBuilder.bake(multiDispatchInfo);

            return true;
        }
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernelBytes[3]{};
    MultiDeviceKernel *kernelLeft[3]{};
    MultiDeviceKernel *kernelMiddle[3]{};
    MultiDeviceKernel *kernelRight[3]{};
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::copyBufferRect,
                     "",
                     "CopyBufferRectBytes2d", kernelBytes[0],
                     "CopyBufferRectBytes2d", kernelBytes[1],
                     "CopyBufferRectBytes3d", kernelBytes[2],
                     "CopyBufferRectBytes2d", kernelLeft[0],
                     "CopyBufferRectBytes2d", kernelLeft[1],
                     "CopyBufferRectBytes3d", kernelLeft[2],
                     "CopyBufferRectBytesMiddle2d", kernelMiddle[0],
                     "CopyBufferRectBytesMiddle2d", kernelMiddle[1],
                     "CopyBufferRectBytesMiddle3d", kernelMiddle[2],
                     "CopyBufferRectBytes2d", kernelRight[0],
                     "CopyBufferRectBytes2d", kernelRight[1],
                     "CopyBufferRectBytes3d", kernelRight[2]);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferRectStateless> : public BuiltInOp<EBuiltInOps::copyBufferRect> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferRect>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferRectStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferRectBytes2dStateless", kernelBytes[0],
                 "CopyBufferRectBytes2dStateless", kernelBytes[1],
                 "CopyBufferRectBytes3dStateless", kernelBytes[2],
                 "CopyBufferRectBytes2dStateless", kernelLeft[0],
                 "CopyBufferRectBytes2dStateless", kernelLeft[1],
                 "CopyBufferRectBytes3dStateless", kernelLeft[2],
                 "CopyBufferRectBytesMiddle2dStateless", kernelMiddle[0],
                 "CopyBufferRectBytesMiddle2dStateless", kernelMiddle[1],
                 "CopyBufferRectBytesMiddle3dStateless", kernelMiddle[2],
                 "CopyBufferRectBytes2dStateless", kernelRight[0],
                 "CopyBufferRectBytes2dStateless", kernelRight[1],
                 "CopyBufferRectBytes3dStateless", kernelRight[2]);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferRectStatelessHeapless> : public BuiltInOp<EBuiltInOps::copyBufferRect> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferRect>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferRectStatelessHeapless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferRectBytes2dStateless", kernelBytes[0],
                 "CopyBufferRectBytes2dStateless", kernelBytes[1],
                 "CopyBufferRectBytes3dStateless", kernelBytes[2],
                 "CopyBufferRectBytes2dStateless", kernelLeft[0],
                 "CopyBufferRectBytes2dStateless", kernelLeft[1],
                 "CopyBufferRectBytes3dStateless", kernelLeft[2],
                 "CopyBufferRectBytesMiddle2dStateless", kernelMiddle[0],
                 "CopyBufferRectBytesMiddle2dStateless", kernelMiddle[1],
                 "CopyBufferRectBytesMiddle3dStateless", kernelMiddle[2],
                 "CopyBufferRectBytes2dStateless", kernelRight[0],
                 "CopyBufferRectBytes2dStateless", kernelRight[1],
                 "CopyBufferRectBytes3dStateless", kernelRight[2]);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit> kernelSplit1DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        uintptr_t start = reinterpret_cast<uintptr_t>(operationParams.dstPtr) + operationParams.dstOffset.x;

        size_t middleAlignment = MemoryConstants::cacheLineSize;
        size_t middleElSize = sizeof(uint32_t);

        uintptr_t leftSize = start % middleAlignment;
        leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0; // calc left leftover size
        leftSize = std::min(leftSize, operationParams.size.x);        // clamp left leftover size to requested size

        uintptr_t rightSize = (start + operationParams.size.x) % middleAlignment; // calc right leftover size
        rightSize = std::min(rightSize, operationParams.size.x - leftSize);       // clamp

        uintptr_t middleSizeBytes = operationParams.size.x - leftSize - rightSize; // calc middle size

        auto middleSizeEls = middleSizeBytes / middleElSize; // num work items in middle walker

        uint32_t rootDeviceIndex = clDevice.getRootDeviceIndex();

        // Set-up ISA
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::left, kernLeftLeftover->getKernel(rootDeviceIndex));
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::middle, kernMiddle->getKernel(rootDeviceIndex));
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::right, kernRightLeftover->getKernel(rootDeviceIndex));

        DEBUG_BREAK_IF((operationParams.srcMemObj == nullptr) || (operationParams.srcOffset != 0));
        DEBUG_BREAK_IF((operationParams.dstMemObj == nullptr) && (operationParams.dstSvmAlloc == nullptr));

        bool isDestinationInSystem = false;
        // Set-up dstMemObj with buffer
        if (operationParams.dstSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(0, operationParams.dstPtr, operationParams.dstSvmAlloc);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstSvmAlloc->getAllocationType());
        } else {
            kernelSplit1DBuilder.setArg(0, operationParams.dstMemObj);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
        }
        kernelSplit1DBuilder.setKernelDestinationArgumentInSystem(isDestinationInSystem);

        // Set-up dstOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::left, 1, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::right, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        // Set-up srcMemObj with pattern
        auto graphicsAllocation = operationParams.srcMemObj->getMultiGraphicsAllocation().getDefaultGraphicsAllocation();
        kernelSplit1DBuilder.setArgSvm(2, operationParams.srcMemObj->getSize(), reinterpret_cast<void *>(graphicsAllocation->getGpuAddressToPatch()), graphicsAllocation, CL_MEM_READ_ONLY);

        // Set-up patternSizeInEls
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::left, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize()));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::middle, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize() / middleElSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::right, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize()));

        // Set-up work sizes
        // Note for split walker, it would be just builder.SetDipatchGeomtry(GWS, ELWS, OFFSET)
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::left, Vec3<size_t>{leftSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::middle, Vec3<size_t>{middleSizeEls, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::right, Vec3<size_t>{rightSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.bake(multiDispatchInfo);

        return true;
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernLeftLeftover = nullptr;
    MultiDeviceKernel *kernMiddle = nullptr;
    MultiDeviceKernel *kernRightLeftover = nullptr;

    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::fillBuffer,
                     "",
                     "FillBufferLeftLeftover", kernLeftLeftover,
                     "FillBufferMiddle", kernMiddle,
                     "FillBufferRightLeftover", kernRightLeftover);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillBufferStateless> : public BuiltInOp<EBuiltInOps::fillBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device) : BuiltInOp<EBuiltInOps::fillBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::fillBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "FillBufferLeftLeftoverStateless", kernLeftLeftover,
                 "FillBufferMiddleStateless", kernMiddle,
                 "FillBufferRightLeftoverStateless", kernRightLeftover);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfos) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfos);
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillBufferStatelessHeapless> : public BuiltInOp<EBuiltInOps::fillBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device) : BuiltInOp<EBuiltInOps::fillBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::fillBufferStatelessHeapless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "FillBufferLeftLeftoverStateless", kernLeftLeftover,
                 "FillBufferMiddleStateless", kernMiddle,
                 "FillBufferRightLeftoverStateless", kernRightLeftover);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfos) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfos);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferToImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernelBytes[5] = {nullptr};
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::copyBufferToImage3d,
                     "",
                     "CopyBufferToImage3dBytes", kernelBytes[0],
                     "CopyBufferToImage3d2Bytes", kernelBytes[1],
                     "CopyBufferToImage3d4Bytes", kernelBytes[2],
                     "CopyBufferToImage3d8Bytes", kernelBytes[3],
                     "CopyBufferToImage3d16Bytes", kernelBytes[4]);
        }
    }

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!(((operationParams.srcPtr != nullptr) || (operationParams.srcMemObj != nullptr)) && (operationParams.dstPtr == nullptr)));

        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe image to be byte-copy
        auto dstImageRedescribed = dstImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        // Calculate srcRowPitch and srcSlicePitch
        auto bytesPerPixel = dstImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;

        size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};

        auto srcRowPitch = operationParams.srcRowPitch ? operationParams.srcRowPitch : region[0] * bytesPerPixel;

        auto srcSlicePitch =
            operationParams.srcSlicePitch ? operationParams.srcSlicePitch : ((dstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * srcRowPitch);

        // Determine size of host ptr surface for residency purposes
        size_t hostPtrSize = operationParams.srcPtr ? Image::calculateHostPtrSize(region, srcRowPitch, srcSlicePitch, bytesPerPixel, dstImage->getImageDesc().image_type) : 0;
        hostPtrSize += operationParams.srcOffset.x;

        // Set-up kernel
        auto bytesExponent = Math::log2(bytesPerPixel);
        DEBUG_BREAK_IF(bytesExponent >= 5);
        kernelNoSplit3DBuilder.setKernel(kernelBytes[bytesExponent]->getKernel(clDevice.getRootDeviceIndex()));

        // Set-up source host ptr / buffer
        if (operationParams.srcPtr) {
            kernelNoSplit3DBuilder.setArgSvm(0, hostPtrSize, operationParams.srcPtr, nullptr, CL_MEM_READ_ONLY);
        } else {
            kernelNoSplit3DBuilder.setArg(0, operationParams.srcMemObj);
        }

        // Set-up destination image
        kernelNoSplit3DBuilder.setArg(1, dstImageRedescribed, operationParams.dstMipLevel);

        // Set-up srcOffset
        kernelNoSplit3DBuilder.setArg(2, static_cast<OffsetType>(operationParams.srcOffset.x));

        // Set-up dstOrigin
        {
            uint32_t origin[] = {
                static_cast<uint32_t>(operationParams.dstOffset.x),
                static_cast<uint32_t>(operationParams.dstOffset.y),
                static_cast<uint32_t>(operationParams.dstOffset.z),
                0};
            kernelNoSplit3DBuilder.setArg(3, sizeof(origin), origin);
        }

        // Set-up srcRowPitch
        {
            OffsetType pitch[] = {
                static_cast<OffsetType>(srcRowPitch),
                static_cast<OffsetType>(srcSlicePitch)};
            kernelNoSplit3DBuilder.setArg(4, sizeof(pitch), pitch);
        }

        // Set-up work sizes
        kernelNoSplit3DBuilder.setDispatchGeometry(operationParams.size, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelNoSplit3DBuilder.bake(multiDispatchInfo);

        return true;
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferToImage3dStateless> : public BuiltInOp<EBuiltInOps::copyBufferToImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferToImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferToImage3dStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToImage3dBytesStateless", kernelBytes[0],
                 "CopyBufferToImage3d2BytesStateless", kernelBytes[1],
                 "CopyBufferToImage3d4BytesStateless", kernelBytes[2],
                 "CopyBufferToImage3d8BytesStateless", kernelBytes[3],
                 "CopyBufferToImage3d16BytesStateless", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyBufferToImage3dHeapless> : public BuiltInOp<EBuiltInOps::copyBufferToImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyBufferToImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyBufferToImage3dHeapless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToImage3dBytesStateless", kernelBytes[0],
                 "CopyBufferToImage3d2BytesStateless", kernelBytes[1],
                 "CopyBufferToImage3d4BytesStateless", kernelBytes[2],
                 "CopyBufferToImage3d8BytesStateless", kernelBytes[3],
                 "CopyBufferToImage3d16BytesStateless", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyImage3dToBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernelBytes[5] = {nullptr};

    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::copyImage3dToBuffer,
                     "",
                     "CopyImage3dToBufferBytes", kernelBytes[0],
                     "CopyImage3dToBuffer2Bytes", kernelBytes[1],
                     "CopyImage3dToBuffer4Bytes", kernelBytes[2],
                     "CopyImage3dToBuffer8Bytes", kernelBytes[3],
                     "CopyImage3dToBuffer16Bytes", kernelBytes[4]);
        }
    }

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && ((operationParams.dstPtr != nullptr) || (operationParams.dstMemObj != nullptr))));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);

        // Redescribe image to be byte-copy
        auto srcImageRedescribed = srcImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's

        // Calculate dstRowPitch and dstSlicePitch
        auto bytesPerPixel = srcImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;

        size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};

        auto dstRowPitch = operationParams.dstRowPitch ? operationParams.dstRowPitch : region[0] * bytesPerPixel;

        auto dstSlicePitch =
            operationParams.dstSlicePitch ? operationParams.dstSlicePitch : ((srcImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

        // Determine size of host ptr surface for residency purposes
        size_t hostPtrSize = operationParams.dstPtr ? Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, srcImage->getImageDesc().image_type) : 0;
        hostPtrSize += operationParams.dstOffset.x;

        uint32_t rootDeviceIndex = clDevice.getRootDeviceIndex();

        // Set-up ISA
        auto bytesExponent = Math::log2(bytesPerPixel);
        DEBUG_BREAK_IF(bytesExponent >= 5);
        kernelNoSplit3DBuilder.setKernel(kernelBytes[bytesExponent]->getKernel(rootDeviceIndex));

        // Set-up source image
        kernelNoSplit3DBuilder.setArg(0, srcImageRedescribed, operationParams.srcMipLevel);

        bool isDestinationInSystem = false;
        // Set-up destination host ptr / buffer
        if (operationParams.dstPtr) {
            kernelNoSplit3DBuilder.setArgSvm(1, hostPtrSize, operationParams.dstPtr, nullptr, 0u);
            isDestinationInSystem = operationParams.dstPtr != nullptr;
        } else {
            kernelNoSplit3DBuilder.setArg(1, operationParams.dstMemObj);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
        }
        kernelNoSplit3DBuilder.setKernelDestinationArgumentInSystem(isDestinationInSystem);

        // Set-up srcOrigin
        {
            uint32_t origin[] = {
                static_cast<uint32_t>(operationParams.srcOffset.x),
                static_cast<uint32_t>(operationParams.srcOffset.y),
                static_cast<uint32_t>(operationParams.srcOffset.z),
                0};
            kernelNoSplit3DBuilder.setArg(2, sizeof(origin), origin);
        }

        // Set-up dstOffset
        kernelNoSplit3DBuilder.setArg(3, static_cast<OffsetType>(operationParams.dstOffset.x));

        // Set-up dstRowPitch
        {
            OffsetType pitch[] = {
                static_cast<OffsetType>(dstRowPitch),
                static_cast<OffsetType>(dstSlicePitch)};
            kernelNoSplit3DBuilder.setArg(4, sizeof(pitch), pitch);
        }

        // Set-up work sizes
        kernelNoSplit3DBuilder.setDispatchGeometry(operationParams.size, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelNoSplit3DBuilder.bake(multiDispatchInfo);

        return true;
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyImage3dToBufferStateless> : public BuiltInOp<EBuiltInOps::copyImage3dToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyImage3dToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyImage3dToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyImage3dToBufferBytesStateless", kernelBytes[0],
                 "CopyImage3dToBuffer2BytesStateless", kernelBytes[1],
                 "CopyImage3dToBuffer4BytesStateless", kernelBytes[2],
                 "CopyImage3dToBuffer8BytesStateless", kernelBytes[3],
                 "CopyImage3dToBuffer16BytesStateless", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyImage3dToBufferHeapless> : public BuiltInOp<EBuiltInOps::copyImage3dToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyImage3dToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyImage3dToBufferHeapless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyImage3dToBufferBytesStateless", kernelBytes[0],
                 "CopyImage3dToBuffer2BytesStateless", kernelBytes[1],
                 "CopyImage3dToBuffer4BytesStateless", kernelBytes[2],
                 "CopyImage3dToBuffer8BytesStateless", kernelBytes[3],
                 "CopyImage3dToBuffer16BytesStateless", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::copyImageToImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        populate(EBuiltInOps::copyImageToImage3d,
                 "",
                 "CopyImage3dToImage3d", kernelCopyImage3dToImage3d,
                 "CopyImage1dBufferToImage3d", kernelCopyImage1dBufferToImage3d,
                 "CopyImage3dToImage1dBuffer", kernelCopyImage3dToImage1dBuffer,
                 "CopyImage1dBufferToImage1dBuffer", kernelCopyImage1dBufferToImage1dBuffer);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && (operationParams.dstPtr == nullptr)));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);
        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe images to be byte-copies
        auto srcImageRedescribed = srcImage->redescribe();
        auto dstImageRedescribed = dstImage->redescribe();

        auto srcImageDescriptor = Image::convertDescriptor(srcImage->getImageDesc());
        auto srcSurfaceFormatInfo = srcImage->getSurfaceFormatInfo();
        SurfaceOffsets srcSurfaceOffsets;
        srcImage->getSurfaceOffsets(srcSurfaceOffsets);
        ImageInfo srcImgInfo;
        srcImgInfo.imgDesc = srcImageDescriptor;
        srcImgInfo.surfaceFormat = &srcSurfaceFormatInfo.surfaceFormat;
        srcImgInfo.xOffset = srcSurfaceOffsets.xOffset;

        auto dstImageDescriptor = Image::convertDescriptor(dstImage->getImageDesc());
        auto dstSurfaceFormatInfo = dstImage->getSurfaceFormatInfo();
        SurfaceOffsets dstSurfaceOffsets;
        dstImage->getSurfaceOffsets(dstSurfaceOffsets);
        ImageInfo dstImgInfo;
        dstImgInfo.imgDesc = dstImageDescriptor;
        dstImgInfo.surfaceFormat = &dstSurfaceFormatInfo.surfaceFormat;
        dstImgInfo.xOffset = dstSurfaceOffsets.xOffset;

        const auto *srcAllocation = srcImage->getGraphicsAllocation(clDevice.getRootDeviceIndex());
        const auto *dstAllocation = dstImage->getGraphicsAllocation(clDevice.getRootDeviceIndex());

        if (ImageHelper::areImagesCompatibleWithPackedFormat(clDevice.getProductHelper(), srcImgInfo, dstImgInfo, srcAllocation, dstAllocation, operationParams.size.x)) {
            srcImageRedescribed->setIsPackedFormat(true);
            dstImageRedescribed->setIsPackedFormat(true);
        }

        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        bool src1dBuffer = srcImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER;
        bool dst1dBuffer = dstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER;

        // Set-up kernel
        if (src1dBuffer && dst1dBuffer) {
            kernelNoSplit3DBuilder.setKernel(kernelCopyImage1dBufferToImage1dBuffer->getKernel(clDevice.getRootDeviceIndex()));
        } else if (src1dBuffer) {
            kernelNoSplit3DBuilder.setKernel(kernelCopyImage1dBufferToImage3d->getKernel(clDevice.getRootDeviceIndex()));
        } else if (dst1dBuffer) {
            kernelNoSplit3DBuilder.setKernel(kernelCopyImage3dToImage1dBuffer->getKernel(clDevice.getRootDeviceIndex()));
        } else {
            kernelNoSplit3DBuilder.setKernel(kernelCopyImage3dToImage3d->getKernel(clDevice.getRootDeviceIndex()));
        }

        // Set-up source image
        kernelNoSplit3DBuilder.setArg(0, srcImageRedescribed, operationParams.srcMipLevel);

        // Set-up destination image
        kernelNoSplit3DBuilder.setArg(1, dstImageRedescribed, operationParams.dstMipLevel);

        // Set-up srcOrigin
        {
            uint32_t origin[] = {
                static_cast<uint32_t>(operationParams.srcOffset.x),
                static_cast<uint32_t>(operationParams.srcOffset.y),
                static_cast<uint32_t>(operationParams.srcOffset.z),
                0};
            kernelNoSplit3DBuilder.setArg(2, sizeof(origin), origin);
        }

        // Set-up dstOrigin
        {
            uint32_t origin[] = {
                static_cast<uint32_t>(operationParams.dstOffset.x),
                static_cast<uint32_t>(operationParams.dstOffset.y),
                static_cast<uint32_t>(operationParams.dstOffset.z),
                0};
            kernelNoSplit3DBuilder.setArg(3, sizeof(origin), origin);
        }

        // Set-up work sizes
        kernelNoSplit3DBuilder.setDispatchGeometry(operationParams.size, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelNoSplit3DBuilder.bake(multiDispatchInfo);

        return true;
    }

  protected:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::copyImageToImage3d,
                     "",
                     "CopyImage3dToImage3d", kernelCopyImage3dToImage3d,
                     "CopyImage1dBufferToImage3d", kernelCopyImage1dBufferToImage3d,
                     "CopyImage3dToImage1dBuffer", kernelCopyImage3dToImage1dBuffer,
                     "CopyImage1dBufferToImage1dBuffer", kernelCopyImage1dBufferToImage1dBuffer);
        }
    }

    MultiDeviceKernel *kernelCopyImage3dToImage3d = nullptr;
    MultiDeviceKernel *kernelCopyImage1dBufferToImage3d = nullptr;
    MultiDeviceKernel *kernelCopyImage3dToImage1dBuffer = nullptr;
    MultiDeviceKernel *kernelCopyImage1dBufferToImage1dBuffer = nullptr;
};

template <>
class BuiltInOp<EBuiltInOps::copyImageToImage3dHeapless> : public BuiltInOp<EBuiltInOps::copyImageToImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::copyImageToImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::copyImageToImage3dHeapless,
                 "",
                 "CopyImage3dToImage3d", kernelCopyImage3dToImage3d,
                 "CopyImage1dBufferToImage3d", kernelCopyImage1dBufferToImage3d,
                 "CopyImage3dToImage1dBuffer", kernelCopyImage3dToImage1dBuffer,
                 "CopyImage1dBufferToImage1dBuffer", kernelCopyImage1dBufferToImage1dBuffer);
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        populate(EBuiltInOps::fillImage3d,
                 "",
                 "FillImage3d", kernel);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();
        DEBUG_BREAK_IF(!((operationParams.srcMemObj == nullptr) && (operationParams.srcPtr != nullptr) && (operationParams.dstPtr == nullptr)));

        auto image = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe image to be byte-copy
        auto imageRedescribed = image->redescribeFillImage();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(imageRedescribed));

        // Set-up kernel
        kernelNoSplit3DBuilder.setKernel(kernel->getKernel(clDevice.getRootDeviceIndex()));

        // Set-up destination image
        kernelNoSplit3DBuilder.setArg(0, imageRedescribed);

        // Set-up fill color
        int iFillColor[4] = {0};
        const void *fillColor = operationParams.srcPtr;
        convertFillColor(fillColor,
                         iFillColor,
                         image->getSurfaceFormatInfo().oclImageFormat,
                         imageRedescribed->getSurfaceFormatInfo().oclImageFormat);
        kernelNoSplit3DBuilder.setArg(1, 4 * sizeof(int32_t), iFillColor);

        // Set-up dstOffset
        {
            uint32_t offset[] = {
                static_cast<uint32_t>(operationParams.dstOffset.x),
                static_cast<uint32_t>(operationParams.dstOffset.y),
                static_cast<uint32_t>(operationParams.dstOffset.z),
                0};
            kernelNoSplit3DBuilder.setArg(2, sizeof(offset), offset);
        }

        // Set-up work sizes
        kernelNoSplit3DBuilder.setDispatchGeometry(operationParams.size, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelNoSplit3DBuilder.bake(multiDispatchInfo);

        return true;
    }

  protected:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {

            populate(EBuiltInOps::fillImage3d,
                     "",
                     "FillImage3d", kernel);
        }
    }

    MultiDeviceKernel *kernel = nullptr;
};

template <>
class BuiltInOp<EBuiltInOps::fillImage3dHeapless> : public BuiltInOp<EBuiltInOps::fillImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::fillImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::fillImage3dHeapless,
                 "",
                 "FillImage3d", kernel);
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillImage1dBuffer> : public BuiltInOp<EBuiltInOps::fillImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::fillImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::fillImage1dBuffer,
                 "",
                 "FillImage1dBuffer", kernel);
    }
};

template <>
class BuiltInOp<EBuiltInOps::fillImage1dBufferHeapless> : public BuiltInOp<EBuiltInOps::fillImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::fillImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::fillImage1dBufferHeapless,
                 "",
                 "FillImage1dBuffer", kernel);
    }
};

BuiltinDispatchInfoBuilder &BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, ClDevice &device) {
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &builtins = *device.getDevice().getBuiltIns();
    auto clExecutionEnvironment = static_cast<ClExecutionEnvironment *>(device.getExecutionEnvironment());
    auto &operationBuilder = clExecutionEnvironment->peekBuilders(device.getRootDeviceIndex())[operationId];
    switch (operation) {
    case EBuiltInOps::copyBufferToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferToBufferStatelessHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToBufferStatelessHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferRect:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferRect>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferRectStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferRectStateless>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferRectStatelessHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferRectStatelessHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::fillBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::fillBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::fillBufferStatelessHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillBufferStatelessHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferToImage3dStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToImage3dStateless>>(builtins, device); });
        break;
    case EBuiltInOps::copyBufferToImage3dHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyBufferToImage3dHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::copyImage3dToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyImage3dToBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::copyImage3dToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyImage3dToBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::copyImage3dToBufferHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyImage3dToBufferHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::copyImageToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyImageToImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::copyImageToImage3dHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::copyImageToImage3dHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::fillImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::fillImage3dHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillImage3dHeapless>>(builtins, device); });
        break;
    case EBuiltInOps::auxTranslation:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::auxTranslation>>(builtins, device); });
        break;
    case EBuiltInOps::fillImage1dBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillImage1dBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::fillImage1dBufferHeapless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::fillImage1dBufferHeapless>>(builtins, device); });
        break;
    default:
        UNRECOVERABLE_IF("getBuiltinDispatchInfoBuilder failed");
    }
    return *operationBuilder.first;
}

BuiltInOwnershipWrapper::BuiltInOwnershipWrapper(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    takeOwnership(inputBuilder, context);
}
BuiltInOwnershipWrapper::~BuiltInOwnershipWrapper() {
    if (builder) {
        for (auto &kernel : builder->peekUsedKernels()) {
            kernel->releaseOwnership();
        }
        if (!builder->peekUsedKernels().empty()) {
            builder->peekUsedKernels()[0]->getProgram()->setContext(nullptr);
            builder->peekUsedKernels()[0]->getProgram()->releaseOwnership();
        }
    }
}
void BuiltInOwnershipWrapper::takeOwnership(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    UNRECOVERABLE_IF(builder);
    builder = &inputBuilder;
    if (!builder->peekUsedKernels().empty()) {
        builder->peekUsedKernels()[0]->getProgram()->takeOwnership();
        builder->peekUsedKernels()[0]->getProgram()->setContext(context);
    }
    for (auto &kernel : builder->peekUsedKernels()) {
        kernel->takeOwnership();
    }
}

std::unique_ptr<Program> BuiltinDispatchInfoBuilder::createProgramFromCode(const BuiltinCode &bc, const ClDeviceVector &deviceVector) {
    std::unique_ptr<Program> ret;
    const char *data = bc.resource.data();
    size_t dataLen = bc.resource.size();
    cl_int err = 0;
    switch (bc.type) {
    default:
        break;
    case BuiltinCode::ECodeType::source:
    case BuiltinCode::ECodeType::intermediate:
        ret.reset(Program::createBuiltInFromSource(data, nullptr, deviceVector, &err));
        break;
    case BuiltinCode::ECodeType::binary:
        ret.reset(Program::createBuiltInFromGenBinary(nullptr, deviceVector, data, dataLen, &err));
        break;
    }
    return ret;
}

} // namespace NEO
