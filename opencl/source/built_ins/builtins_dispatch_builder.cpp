/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/builtins_dispatch_builder.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/built_ins.inl"
#include "opencl/source/built_ins/vme_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/program/program.h"

#include "compiler_options.h"

#include <cstdint>
#include <sstream>

namespace NEO {
template <>
class BuiltInOp<EBuiltInOps::CopyBufferToBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}
    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::KernelSplit> kernelSplit1DBuilder(clDevice);
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
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Left, kernLeftLeftover->getKernel(rootDeviceIndex));
        if (isSrcMisaligned) {
            kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Middle, kernMiddleMisaligned->getKernel(rootDeviceIndex));
        } else {
            kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Middle, kernMiddle->getKernel(rootDeviceIndex));
        }
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Right, kernRightLeftover->getKernel(rootDeviceIndex));

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
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 2, static_cast<OffsetType>(operationParams.srcOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize + middleSizeBytes));

        // Set-up dstOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 3, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        if (isSrcMisaligned) {
            kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 4, static_cast<uint32_t>(srcMisalignment * 8));
        }

        // Set-up work sizes
        // Note for split walker, it would be just builder.SetDipatchGeometry(GWS, ELWS, OFFSET)
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Left, Vec3<size_t>{leftSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Middle, Vec3<size_t>{middleSizeEls, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Right, Vec3<size_t>{rightSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
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
            populate(EBuiltInOps::CopyBufferToBuffer,
                     "",
                     "CopyBufferToBufferLeftLeftover", kernLeftLeftover,
                     "CopyBufferToBufferMiddle", kernMiddle,
                     "CopyBufferToBufferMiddleMisaligned", kernMiddleMisaligned,
                     "CopyBufferToBufferRightLeftover", kernRightLeftover);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferToBufferStateless> : public BuiltInOp<EBuiltInOps::CopyBufferToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::CopyBufferToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::CopyBufferToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToBufferLeftLeftover", kernLeftLeftover,
                 "CopyBufferToBufferMiddle", kernMiddle,
                 "CopyBufferToBufferMiddleMisaligned", kernMiddleMisaligned,
                 "CopyBufferToBufferRightLeftover", kernRightLeftover);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferRect> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        size_t hostPtrSize = 0;
        bool is3D = false;

        if (operationParams.srcMemObj && operationParams.dstMemObj) {
            DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && (operationParams.dstPtr == nullptr)));
            is3D = (operationParams.size.z > 1) || (operationParams.srcOffset.z > 0) || (operationParams.dstOffset.z > 0);
        } else {
            if (operationParams.srcPtr) {
                size_t origin[] = {operationParams.srcOffset.x, operationParams.srcOffset.y, operationParams.srcOffset.z};
                size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};
                hostPtrSize = Buffer::calculateHostPtrSize(origin, region, operationParams.srcRowPitch, operationParams.srcSlicePitch);
                is3D = (operationParams.size.z > 1) || (operationParams.dstOffset.z > 0);
            } else if (operationParams.dstPtr) {
                size_t origin[] = {operationParams.dstOffset.x, operationParams.dstOffset.y, operationParams.dstOffset.z};
                size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};
                hostPtrSize = Buffer::calculateHostPtrSize(origin, region, operationParams.dstRowPitch, operationParams.dstSlicePitch);
                is3D = (operationParams.size.z > 1) || (operationParams.srcOffset.z > 0);
            } else {
                DEBUG_BREAK_IF(!false);
            }
        }

        uint32_t rootDeviceIndex = clDevice.getRootDeviceIndex();

        // Set-up ISA
        int dimensions = is3D ? 3 : 2;
        kernelNoSplit3DBuilder.setKernel(kernelBytes[dimensions - 1]->getKernel(rootDeviceIndex));

        size_t srcOffsetFromAlignedPtr = 0;
        size_t dstOffsetFromAlignedPtr = 0;

        // arg0 = src
        if (operationParams.srcMemObj) {
            kernelNoSplit3DBuilder.setArg(0, operationParams.srcMemObj);
        } else {
            void *srcPtrToSet = operationParams.srcPtr;
            if (!is3D) {
                auto srcPtr = ptrOffset(operationParams.srcPtr, operationParams.srcOffset.z * operationParams.srcSlicePitch);
                srcPtrToSet = alignDown(srcPtr, 4);
                srcOffsetFromAlignedPtr = ptrDiff(srcPtr, srcPtrToSet);
            }
            kernelNoSplit3DBuilder.setArgSvm(0, hostPtrSize, srcPtrToSet, nullptr, CL_MEM_READ_ONLY);
        }

        bool isDestinationInSystem = false;
        // arg1 = dst
        if (operationParams.dstMemObj) {
            kernelNoSplit3DBuilder.setArg(1, operationParams.dstMemObj);
            isDestinationInSystem = Kernel::graphicsAllocationTypeUseSystemMemory(operationParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
        } else {
            void *dstPtrToSet = operationParams.dstPtr;
            if (!is3D) {
                auto dstPtr = ptrOffset(operationParams.dstPtr, operationParams.dstOffset.z * operationParams.dstSlicePitch);
                dstPtrToSet = alignDown(dstPtr, 4);
                dstOffsetFromAlignedPtr = ptrDiff(dstPtr, dstPtrToSet);
            }
            kernelNoSplit3DBuilder.setArgSvm(1, hostPtrSize, dstPtrToSet, nullptr, 0u);
            isDestinationInSystem = operationParams.dstPtr != nullptr;
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

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo);
    }

  protected:
    MultiDeviceKernel *kernelBytes[3]{};
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device, bool populateKernels)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        if (populateKernels) {
            populate(EBuiltInOps::CopyBufferRect,
                     "",
                     "CopyBufferRectBytes2d", kernelBytes[0],
                     "CopyBufferRectBytes2d", kernelBytes[1],
                     "CopyBufferRectBytes3d", kernelBytes[2]);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferRectStateless> : public BuiltInOp<EBuiltInOps::CopyBufferRect> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::CopyBufferRect>(kernelsLib, device, false) {
        populate(EBuiltInOps::CopyBufferRectStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferRectBytes2d", kernelBytes[0],
                 "CopyBufferRectBytes2d", kernelBytes[1],
                 "CopyBufferRectBytes3d", kernelBytes[2]);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::FillBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp(kernelsLib, device, true) {}

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::KernelSplit> kernelSplit1DBuilder(clDevice);
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
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Left, kernLeftLeftover->getKernel(rootDeviceIndex));
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Middle, kernMiddle->getKernel(rootDeviceIndex));
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Right, kernRightLeftover->getKernel(rootDeviceIndex));

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
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 1, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        // Set-up srcMemObj with pattern
        auto graphicsAllocation = operationParams.srcMemObj->getMultiGraphicsAllocation().getDefaultGraphicsAllocation();
        kernelSplit1DBuilder.setArgSvm(2, operationParams.srcMemObj->getSize(), graphicsAllocation->getUnderlyingBuffer(), graphicsAllocation, CL_MEM_READ_ONLY);

        // Set-up patternSizeInEls
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize()));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize() / middleElSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 3, static_cast<OffsetType>(operationParams.srcMemObj->getSize()));

        // Set-up work sizes
        // Note for split walker, it would be just builder.SetDipatchGeomtry(GWS, ELWS, OFFSET)
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Left, Vec3<size_t>{leftSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Middle, Vec3<size_t>{middleSizeEls, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Right, Vec3<size_t>{rightSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
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
            populate(EBuiltInOps::FillBuffer,
                     "",
                     "FillBufferLeftLeftover", kernLeftLeftover,
                     "FillBufferMiddle", kernMiddle,
                     "FillBufferRightLeftover", kernRightLeftover);
        }
    }
};

template <>
class BuiltInOp<EBuiltInOps::FillBufferStateless> : public BuiltInOp<EBuiltInOps::FillBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device) : BuiltInOp<EBuiltInOps::FillBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::FillBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "FillBufferLeftLeftover", kernLeftLeftover,
                 "FillBufferMiddle", kernMiddle,
                 "FillBufferRightLeftover", kernRightLeftover);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfos) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfos);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferToImage3d> : public BuiltinDispatchInfoBuilder {
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
            populate(EBuiltInOps::CopyBufferToImage3d,
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
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!(((operationParams.srcPtr != nullptr) || (operationParams.srcMemObj != nullptr)) && (operationParams.dstPtr == nullptr)));

        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe image to be byte-copy
        auto dstImageRedescribed = dstImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        // Calculate srcRowPitch and srcSlicePitch
        auto bytesPerPixel = dstImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;

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
class BuiltInOp<EBuiltInOps::CopyBufferToImage3dStateless> : public BuiltInOp<EBuiltInOps::CopyBufferToImage3d> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::CopyBufferToImage3d>(kernelsLib, device, false) {
        populate(EBuiltInOps::CopyBufferToImage3dStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToImage3dBytes", kernelBytes[0],
                 "CopyBufferToImage3d2Bytes", kernelBytes[1],
                 "CopyBufferToImage3d4Bytes", kernelBytes[2],
                 "CopyBufferToImage3d8Bytes", kernelBytes[3],
                 "CopyBufferToImage3d16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyImage3dToBuffer> : public BuiltinDispatchInfoBuilder {
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
            populate(EBuiltInOps::CopyImage3dToBuffer,
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
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && ((operationParams.dstPtr != nullptr) || (operationParams.dstMemObj != nullptr))));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);

        // Redescribe image to be byte-copy
        auto srcImageRedescribed = srcImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's

        // Calculate dstRowPitch and dstSlicePitch
        auto bytesPerPixel = srcImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;

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
class BuiltInOp<EBuiltInOps::CopyImage3dToBufferStateless> : public BuiltInOp<EBuiltInOps::CopyImage3dToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltInOp<EBuiltInOps::CopyImage3dToBuffer>(kernelsLib, device, false) {
        populate(EBuiltInOps::CopyImage3dToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyImage3dToBufferBytes", kernelBytes[0],
                 "CopyImage3dToBuffer2Bytes", kernelBytes[1],
                 "CopyImage3dToBuffer4Bytes", kernelBytes[2],
                 "CopyImage3dToBuffer8Bytes", kernelBytes[3],
                 "CopyImage3dToBuffer16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyImageToImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        populate(EBuiltInOps::CopyImageToImage3d,
                 "",
                 "CopyImageToImage3d", kernel);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder(clDevice);
        auto &operationParams = multiDispatchInfo.peekBuiltinOpParams();

        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && (operationParams.dstPtr == nullptr)));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);
        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe images to be byte-copies
        auto srcImageRedescribed = srcImage->redescribe();
        auto dstImageRedescribed = dstImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        // Set-up kernel
        kernelNoSplit3DBuilder.setKernel(kernel->getKernel(clDevice.getRootDeviceIndex()));

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
    MultiDeviceKernel *kernel = nullptr;
};

template <>
class BuiltInOp<EBuiltInOps::FillImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, ClDevice &device)
        : BuiltinDispatchInfoBuilder(kernelsLib, device) {
        populate(EBuiltInOps::FillImage3d,
                 "",
                 "FillImage3d", kernel);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder(clDevice);
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
                         image->getSurfaceFormatInfo().OCLImageFormat,
                         imageRedescribed->getSurfaceFormatInfo().OCLImageFormat);
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
    MultiDeviceKernel *kernel = nullptr;
};

BuiltinDispatchInfoBuilder &BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, ClDevice &device) {
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &builtins = *device.getDevice().getBuiltIns();
    auto clExecutionEnvironment = static_cast<ClExecutionEnvironment *>(device.getExecutionEnvironment());
    auto &operationBuilder = clExecutionEnvironment->peekBuilders(device.getRootDeviceIndex())[operationId];
    switch (operation) {
    case EBuiltInOps::CopyBufferToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::CopyBufferToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::CopyBufferRect:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferRect>>(builtins, device); });
        break;
    case EBuiltInOps::CopyBufferRectStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferRectStateless>>(builtins, device); });
        break;
    case EBuiltInOps::FillBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::FillBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::CopyBufferToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::CopyBufferToImage3dStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToImage3dStateless>>(builtins, device); });
        break;
    case EBuiltInOps::CopyImage3dToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImage3dToBuffer>>(builtins, device); });
        break;
    case EBuiltInOps::CopyImage3dToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImage3dToBufferStateless>>(builtins, device); });
        break;
    case EBuiltInOps::CopyImageToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImageToImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::FillImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillImage3d>>(builtins, device); });
        break;
    case EBuiltInOps::AuxTranslation:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::AuxTranslation>>(builtins, device); });
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
    case BuiltinCode::ECodeType::Source:
    case BuiltinCode::ECodeType::Intermediate:
        ret.reset(Program::createBuiltInFromSource(data, nullptr, deviceVector, &err));
        break;
    case BuiltinCode::ECodeType::Binary:
        ret.reset(Program::createBuiltInFromGenBinary(nullptr, deviceVector, data, dataLen, &err));
        break;
    }
    return ret;
}

} // namespace NEO
