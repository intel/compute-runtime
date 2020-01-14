/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"

#include "core/compiler_interface/compiler_interface.h"
#include "core/helpers/basic_math.h"
#include "core/helpers/debug_helpers.h"
#include "runtime/built_ins/aux_translation_builtin.h"
#include "runtime/built_ins/built_ins.inl"
#include "runtime/built_ins/sip.h"
#include "runtime/built_ins/vme_dispatch_builder.h"
#include "runtime/helpers/built_ins_helper.h"
#include "runtime/helpers/convert_color.h"
#include "runtime/helpers/dispatch_info_builder.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/image.h"
#include "runtime/program/program.h"

#include "compiler_options.h"

#include <cstdint>
#include <sstream>

namespace NEO {

BuiltIns::BuiltIns() {
    builtinsLib.reset(new BuiltinsLib());
}

BuiltIns::~BuiltIns() {
    delete static_cast<SchedulerKernel *>(schedulerBuiltIn.pKernel);
    delete schedulerBuiltIn.pProgram;
    schedulerBuiltIn.pKernel = nullptr;
    schedulerBuiltIn.pProgram = nullptr;
}

SchedulerKernel &BuiltIns::getSchedulerKernel(Context &context) {
    if (schedulerBuiltIn.pKernel) {
        return *static_cast<SchedulerKernel *>(schedulerBuiltIn.pKernel);
    }

    auto initializeSchedulerProgramAndKernel = [&] {
        cl_int retVal = CL_SUCCESS;

        auto src = context.getDevice(0)->getExecutionEnvironment()->getBuiltIns()->builtinsLib->getBuiltinCode(EBuiltInOps::Scheduler, BuiltinCode::ECodeType::Any, context.getDevice(0)->getDevice());

        auto program = Program::createFromGenBinary(*context.getDevice(0)->getExecutionEnvironment(),
                                                    &context,
                                                    src.resource.data(),
                                                    src.resource.size(),
                                                    true,
                                                    &retVal);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        DEBUG_BREAK_IF(!program);

        retVal = program->processGenBinary();
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        schedulerBuiltIn.pProgram = program;

        auto kernelInfo = schedulerBuiltIn.pProgram->getKernelInfo(SchedulerKernel::schedulerName);
        DEBUG_BREAK_IF(!kernelInfo);

        schedulerBuiltIn.pKernel = Kernel::create<SchedulerKernel>(
            schedulerBuiltIn.pProgram,
            *kernelInfo,
            &retVal);

        UNRECOVERABLE_IF(schedulerBuiltIn.pKernel->getScratchSize() != 0);

        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
    };
    std::call_once(schedulerBuiltIn.programIsInitialized, initializeSchedulerProgramAndKernel);

    UNRECOVERABLE_IF(schedulerBuiltIn.pKernel == nullptr);
    return *static_cast<SchedulerKernel *>(schedulerBuiltIn.pKernel);
}

const SipKernel &BuiltIns::getSipKernel(SipKernelType type, Device &device) {
    uint32_t kernelId = static_cast<uint32_t>(type);
    UNRECOVERABLE_IF(kernelId >= static_cast<uint32_t>(SipKernelType::COUNT));
    auto &sipBuiltIn = this->sipKernels[kernelId];

    auto initializer = [&] {
        cl_int retVal = CL_SUCCESS;

        std::vector<char> sipBinary;
        auto compilerInteface = device.getExecutionEnvironment()->getCompilerInterface();
        UNRECOVERABLE_IF(compilerInteface == nullptr);

        auto ret = compilerInteface->getSipKernelBinary(device, type, sipBinary);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);
        auto program = createProgramForSip(*device.getExecutionEnvironment(),
                                           nullptr,
                                           sipBinary,
                                           sipBinary.size(),
                                           &retVal);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        UNRECOVERABLE_IF(program == nullptr);

        program->setDevice(&device);

        retVal = program->processGenBinary();
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        sipBuiltIn.first.reset(new SipKernel(type, program));
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

// VME:
static const char *blockMotionEstimateIntelSrc = {
#include "kernels/vme_block_motion_estimate_intel_frontend.igdrcl_built_in"
};

static const char *blockAdvancedMotionEstimateCheckIntelSrc = {
#include "kernels/vme_block_advanced_motion_estimate_check_intel_frontend.igdrcl_built_in"
};

static const char *blockAdvancedMotionEstimateBidirectionalCheckIntelSrc = {
#include "kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel_frontend.igdrcl_built_in"
};

static const std::tuple<const char *, const char *> mediaBuiltIns[] = {
    std::make_tuple("block_motion_estimate_intel", blockMotionEstimateIntelSrc),
    std::make_tuple("block_advanced_motion_estimate_check_intel", blockAdvancedMotionEstimateCheckIntelSrc),
    std::make_tuple("block_advanced_motion_estimate_bidirectional_check_intel", blockAdvancedMotionEstimateBidirectionalCheckIntelSrc),
};

// Unlike other built-ins media kernels are not stored in BuiltIns object.
// Pointer to program with built in kernels is returned to the user through API
// call and user is responsible for releasing it by calling clReleaseProgram.
Program *BuiltIns::createBuiltInProgram(
    Context &context,
    Device &device,
    const char *kernelNames,
    int &errcodeRet) {
    std::string programSourceStr = "";
    std::istringstream ss(kernelNames);
    std::string currentKernelName;

    while (std::getline(ss, currentKernelName, ';')) {
        bool found = false;
        for (auto &builtInTuple : mediaBuiltIns) {
            if (currentKernelName == std::get<0>(builtInTuple)) {
                programSourceStr += std::get<1>(builtInTuple);
                found = true;
                break;
            }
        }
        if (!found) {
            errcodeRet = CL_INVALID_VALUE;
            return nullptr;
        }
    }
    if (programSourceStr.empty() == true) {
        errcodeRet = CL_INVALID_VALUE;
        return nullptr;
    }

    Program *pBuiltInProgram = nullptr;

    pBuiltInProgram = Program::create(programSourceStr.c_str(), &context, device, true, nullptr);

    if (pBuiltInProgram) {
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtinsBuilders;
        builtinsBuilders["block_motion_estimate_intel"] =
            &device.getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, context, device);
        builtinsBuilders["block_advanced_motion_estimate_check_intel"] =
            &device.getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, context, device);
        builtinsBuilders["block_advanced_motion_estimate_bidirectional_check_intel"] =
            &device.getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, context, device);
        errcodeRet = pBuiltInProgram->build(
            &device,
            mediaKernelsBuildOptions,
            enableCacheing,
            builtinsBuilders);
    } else {
        errcodeRet = CL_INVALID_VALUE;
    }
    return pBuiltInProgram;
}

template <>
class BuiltInOp<EBuiltInOps::CopyBufferToBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyBufferToBuffer,
                 "",
                 "CopyBufferToBufferLeftLeftover", kernLeftLeftover,
                 "CopyBufferToBufferMiddle", kernMiddle,
                 "CopyBufferToBufferRightLeftover", kernRightLeftover);
    }
    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::KernelSplit> kernelSplit1DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
        uintptr_t start = reinterpret_cast<uintptr_t>(operationParams.dstPtr) + operationParams.dstOffset.x;

        size_t middleAlignment = MemoryConstants::cacheLineSize;
        size_t middleElSize = sizeof(uint32_t) * 4;

        uintptr_t leftSize = start % middleAlignment;
        leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0; // calc left leftover size
        leftSize = std::min(leftSize, operationParams.size.x);        // clamp left leftover size to requested size

        uintptr_t rightSize = (start + operationParams.size.x) % middleAlignment; // calc right leftover size
        rightSize = std::min(rightSize, operationParams.size.x - leftSize);       // clamp

        uintptr_t middleSizeBytes = operationParams.size.x - leftSize - rightSize; // calc middle size

        if (!isAligned<4>(reinterpret_cast<uintptr_t>(operationParams.srcPtr) + operationParams.srcOffset.x + leftSize)) {
            //corner case - src relative to dst does not have DWORD alignment
            leftSize += middleSizeBytes;
            middleSizeBytes = 0;
        }

        auto middleSizeEls = middleSizeBytes / middleElSize; // num work items in middle walker

        // Set-up ISA
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Left, kernLeftLeftover);
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Middle, kernMiddle);
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Right, kernRightLeftover);

        // Set-up common kernel args
        if (operationParams.srcSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(0, operationParams.srcPtr, operationParams.srcSvmAlloc);
        } else if (operationParams.srcMemObj) {
            kernelSplit1DBuilder.setArg(0, operationParams.srcMemObj);
        } else {
            kernelSplit1DBuilder.setArgSvm(0, operationParams.size.x + operationParams.srcOffset.x, operationParams.srcPtr, nullptr, CL_MEM_READ_ONLY);
        }
        if (operationParams.dstSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(1, operationParams.dstPtr, operationParams.dstSvmAlloc);
        } else if (operationParams.dstMemObj) {
            kernelSplit1DBuilder.setArg(1, operationParams.dstMemObj);
        } else {
            kernelSplit1DBuilder.setArgSvm(1, operationParams.size.x + operationParams.dstOffset.x, operationParams.dstPtr, nullptr, 0u);
        }

        kernelSplit1DBuilder.setUnifiedMemorySyncRequirement(operationParams.unifiedMemoryArgsRequireMemSync);

        // Set-up srcOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 2, static_cast<OffsetType>(operationParams.srcOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 2, static_cast<OffsetType>(operationParams.srcOffset.x + leftSize + middleSizeBytes));

        // Set-up dstOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 3, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 3, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        // Set-up work sizes
        // Note for split walker, it would be just builder.SetDipatchGeometry(GWS, ELWS, OFFSET)
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Left, Vec3<size_t>{leftSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Middle, Vec3<size_t>{middleSizeEls, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.setDispatchGeometry(SplitDispatch::RegionCoordX::Right, Vec3<size_t>{rightSize, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
        kernelSplit1DBuilder.bake(multiDispatchInfo);

        return true;
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo, operationParams);
    }

  protected:
    Kernel *kernLeftLeftover = nullptr;
    Kernel *kernMiddle = nullptr;
    Kernel *kernRightLeftover = nullptr;
    BuiltInOp(BuiltIns &kernelsLib)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferToBufferStateless> : public BuiltInOp<EBuiltInOps::CopyBufferToBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltInOp<EBuiltInOps::CopyBufferToBuffer>(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyBufferToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToBufferLeftLeftover", kernLeftLeftover,
                 "CopyBufferToBufferMiddle", kernMiddle,
                 "CopyBufferToBufferRightLeftover", kernRightLeftover);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo, operationParams);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferRect> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib), kernelBytes{nullptr} {
        populate(context, device,
                 EBuiltInOps::CopyBufferRect,
                 "",
                 "CopyBufferRectBytes2d", kernelBytes[0],
                 "CopyBufferRectBytes2d", kernelBytes[1],
                 "CopyBufferRectBytes3d", kernelBytes[2]);
    }

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
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

        // Set-up ISA
        int dimensions = is3D ? 3 : 2;
        kernelNoSplit3DBuilder.setKernel(kernelBytes[dimensions - 1]);

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

        // arg1 = dst
        if (operationParams.dstMemObj) {
            kernelNoSplit3DBuilder.setArg(1, operationParams.dstMemObj);
        } else {
            void *dstPtrToSet = operationParams.dstPtr;
            if (!is3D) {
                auto dstPtr = ptrOffset(operationParams.dstPtr, operationParams.dstOffset.z * operationParams.dstSlicePitch);
                dstPtrToSet = alignDown(dstPtr, 4);
                dstOffsetFromAlignedPtr = ptrDiff(dstPtr, dstPtrToSet);
            }
            kernelNoSplit3DBuilder.setArgSvm(1, hostPtrSize, dstPtrToSet, nullptr, 0u);
        }

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

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo, operationParams);
    }

  protected:
    Kernel *kernelBytes[3];
    BuiltInOp(BuiltIns &kernelsLib) : BuiltinDispatchInfoBuilder(kernelsLib), kernelBytes{nullptr} {};
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferRectStateless> : public BuiltInOp<EBuiltInOps::CopyBufferRect> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltInOp<EBuiltInOps::CopyBufferRect>(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyBufferRectStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferRectBytes2d", kernelBytes[0],
                 "CopyBufferRectBytes2d", kernelBytes[1],
                 "CopyBufferRectBytes3d", kernelBytes[2]);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo, operationParams);
    }
};

template <>
class BuiltInOp<EBuiltInOps::FillBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::FillBuffer,
                 "",
                 "FillBufferLeftLeftover", kernLeftLeftover,
                 "FillBufferMiddle", kernMiddle,
                 "FillBufferRightLeftover", kernRightLeftover);
    }

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::KernelSplit> kernelSplit1DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
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

        // Set-up ISA
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Left, kernLeftLeftover);
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Middle, kernMiddle);
        kernelSplit1DBuilder.setKernel(SplitDispatch::RegionCoordX::Right, kernRightLeftover);

        DEBUG_BREAK_IF((operationParams.srcMemObj == nullptr) || (operationParams.srcOffset != 0));
        DEBUG_BREAK_IF((operationParams.dstMemObj == nullptr) && (operationParams.dstSvmAlloc == nullptr));

        // Set-up dstMemObj with buffer
        if (operationParams.dstSvmAlloc) {
            kernelSplit1DBuilder.setArgSvmAlloc(0, operationParams.dstPtr, operationParams.dstSvmAlloc);
        } else {
            kernelSplit1DBuilder.setArg(0, operationParams.dstMemObj);
        }

        // Set-up dstOffset
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Left, 1, static_cast<OffsetType>(operationParams.dstOffset.x));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Middle, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize));
        kernelSplit1DBuilder.setArg(SplitDispatch::RegionCoordX::Right, 1, static_cast<OffsetType>(operationParams.dstOffset.x + leftSize + middleSizeBytes));

        // Set-up srcMemObj with pattern
        kernelSplit1DBuilder.setArgSvm(2, operationParams.srcMemObj->getSize(), operationParams.srcMemObj->getGraphicsAllocation()->getUnderlyingBuffer(), operationParams.srcMemObj->getGraphicsAllocation(), CL_MEM_READ_ONLY);

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

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo, operationParams);
    }

  protected:
    Kernel *kernLeftLeftover = nullptr;
    Kernel *kernMiddle = nullptr;
    Kernel *kernRightLeftover = nullptr;

    BuiltInOp(BuiltIns &kernelsLib) : BuiltinDispatchInfoBuilder(kernelsLib) {}
};

template <>
class BuiltInOp<EBuiltInOps::FillBufferStateless> : public BuiltInOp<EBuiltInOps::FillBuffer> {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device) : BuiltInOp<EBuiltInOps::FillBuffer>(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::FillBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "FillBufferLeftLeftover", kernLeftLeftover,
                 "FillBufferMiddle", kernMiddle,
                 "FillBufferRightLeftover", kernRightLeftover);
    }
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo, operationParams);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyBufferToImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyBufferToImage3d,
                 "",
                 "CopyBufferToImage3dBytes", kernelBytes[0],
                 "CopyBufferToImage3d2Bytes", kernelBytes[1],
                 "CopyBufferToImage3d4Bytes", kernelBytes[2],
                 "CopyBufferToImage3d8Bytes", kernelBytes[3],
                 "CopyBufferToImage3d16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo, operationParams);
    }

  protected:
    Kernel *kernelBytes[5] = {nullptr};
    BuiltInOp(BuiltIns &kernelsLib) : BuiltinDispatchInfoBuilder(kernelsLib){};

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
        DEBUG_BREAK_IF(!(((operationParams.srcPtr != nullptr) || (operationParams.srcMemObj != nullptr)) && (operationParams.dstPtr == nullptr)));

        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe image to be byte-copy
        auto dstImageRedescribed = dstImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        // Calculate srcRowPitch and srcSlicePitch
        auto bytesPerPixel = dstImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;

        size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};

        auto srcRowPitch = operationParams.dstRowPitch ? operationParams.dstRowPitch : region[0] * bytesPerPixel;

        auto srcSlicePitch =
            operationParams.dstSlicePitch ? operationParams.dstSlicePitch : ((dstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * srcRowPitch);

        // Determine size of host ptr surface for residency purposes
        size_t hostPtrSize = operationParams.srcPtr ? Image::calculateHostPtrSize(region, srcRowPitch, srcSlicePitch, bytesPerPixel, dstImage->getImageDesc().image_type) : 0;
        hostPtrSize += operationParams.srcOffset.x;

        // Set-up kernel
        auto bytesExponent = Math::log2(bytesPerPixel);
        DEBUG_BREAK_IF(bytesExponent >= 5);
        kernelNoSplit3DBuilder.setKernel(kernelBytes[bytesExponent]);

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
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltInOp<EBuiltInOps::CopyBufferToImage3d>(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyBufferToImage3dStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyBufferToImage3dBytes", kernelBytes[0],
                 "CopyBufferToImage3d2Bytes", kernelBytes[1],
                 "CopyBufferToImage3d4Bytes", kernelBytes[2],
                 "CopyBufferToImage3d8Bytes", kernelBytes[3],
                 "CopyBufferToImage3d16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo, operationParams);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyImage3dToBuffer> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyImage3dToBuffer,
                 "",
                 "CopyImage3dToBufferBytes", kernelBytes[0],
                 "CopyImage3dToBuffer2Bytes", kernelBytes[1],
                 "CopyImage3dToBuffer4Bytes", kernelBytes[2],
                 "CopyImage3dToBuffer8Bytes", kernelBytes[3],
                 "CopyImage3dToBuffer16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint32_t>(multiDispatchInfo, operationParams);
    }

  protected:
    Kernel *kernelBytes[5] = {nullptr};

    BuiltInOp(BuiltIns &kernelsLib) : BuiltinDispatchInfoBuilder(kernelsLib) {}

    template <typename OffsetType>
    bool buildDispatchInfosTyped(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && ((operationParams.dstPtr != nullptr) || (operationParams.dstMemObj != nullptr))));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);

        // Redescribe image to be byte-copy
        auto srcImageRedescribed = srcImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's

        // Calculate dstRowPitch and dstSlicePitch
        auto bytesPerPixel = srcImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;

        size_t region[] = {operationParams.size.x, operationParams.size.y, operationParams.size.z};

        auto dstRowPitch = operationParams.srcRowPitch ? operationParams.srcRowPitch : region[0] * bytesPerPixel;

        auto dstSlicePitch =
            operationParams.srcSlicePitch ? operationParams.srcSlicePitch : ((srcImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

        // Determine size of host ptr surface for residency purposes
        size_t hostPtrSize = operationParams.dstPtr ? Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, srcImage->getImageDesc().image_type) : 0;
        hostPtrSize += operationParams.dstOffset.x;

        // Set-up ISA
        auto bytesExponent = Math::log2(bytesPerPixel);
        DEBUG_BREAK_IF(bytesExponent >= 5);
        kernelNoSplit3DBuilder.setKernel(kernelBytes[bytesExponent]);

        // Set-up source image
        kernelNoSplit3DBuilder.setArg(0, srcImageRedescribed, operationParams.srcMipLevel);

        // Set-up destination host ptr / buffer
        if (operationParams.dstPtr) {
            kernelNoSplit3DBuilder.setArgSvm(1, hostPtrSize, operationParams.dstPtr, nullptr, 0u);
        } else {
            kernelNoSplit3DBuilder.setArg(1, operationParams.dstMemObj);
        }

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
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltInOp<EBuiltInOps::CopyImage3dToBuffer>(kernelsLib) {
        populate(context, device,
                 EBuiltInOps::CopyImage3dToBufferStateless,
                 CompilerOptions::greaterThan4gbBuffersRequired,
                 "CopyImage3dToBufferBytes", kernelBytes[0],
                 "CopyImage3dToBuffer2Bytes", kernelBytes[1],
                 "CopyImage3dToBuffer4Bytes", kernelBytes[2],
                 "CopyImage3dToBuffer8Bytes", kernelBytes[3],
                 "CopyImage3dToBuffer16Bytes", kernelBytes[4]);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        return buildDispatchInfosTyped<uint64_t>(multiDispatchInfo, operationParams);
    }
};

template <>
class BuiltInOp<EBuiltInOps::CopyImageToImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib), kernel(nullptr) {
        populate(context, device,
                 EBuiltInOps::CopyImageToImage3d,
                 "",
                 "CopyImageToImage3d", kernel);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
        DEBUG_BREAK_IF(!((operationParams.srcPtr == nullptr) && (operationParams.dstPtr == nullptr)));

        auto srcImage = castToObjectOrAbort<Image>(operationParams.srcMemObj);
        auto dstImage = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe images to be byte-copies
        auto srcImageRedescribed = srcImage->redescribe();
        auto dstImageRedescribed = dstImage->redescribe();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(srcImageRedescribed)); // life range same as mdi's
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(dstImageRedescribed)); // life range same as mdi's

        // Set-up kernel
        kernelNoSplit3DBuilder.setKernel(kernel);

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
    Kernel *kernel;
};

template <>
class BuiltInOp<EBuiltInOps::FillImage3d> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : BuiltinDispatchInfoBuilder(kernelsLib), kernel(nullptr) {
        populate(context, device,
                 EBuiltInOps::FillImage3d,
                 "",
                 "FillImage3d", kernel);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override {
        DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::NoSplit> kernelNoSplit3DBuilder;
        multiDispatchInfo.setBuiltinOpParams(operationParams);
        DEBUG_BREAK_IF(!((operationParams.srcMemObj == nullptr) && (operationParams.srcPtr != nullptr) && (operationParams.dstPtr == nullptr)));

        auto image = castToObjectOrAbort<Image>(operationParams.dstMemObj);

        // Redescribe image to be byte-copy
        auto imageRedescribed = image->redescribeFillImage();
        multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(imageRedescribed));

        // Set-up kernel
        kernelNoSplit3DBuilder.setKernel(kernel);

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
    Kernel *kernel;
};

BuiltinDispatchInfoBuilder &BuiltIns::getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, Context &context, Device &device) {
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &operationBuilder = BuiltinOpsBuilders[operationId];
    switch (operation) {
    default:
        throw std::runtime_error("getBuiltinDispatchInfoBuilder failed");
    case EBuiltInOps::CopyBufferToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToBuffer>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyBufferToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToBufferStateless>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyBufferRect:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferRect>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyBufferRectStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferRectStateless>>(*this, context, device); });
        break;
    case EBuiltInOps::FillBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillBuffer>>(*this, context, device); });
        break;
    case EBuiltInOps::FillBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillBufferStateless>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyBufferToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToImage3d>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyBufferToImage3dStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyBufferToImage3dStateless>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyImage3dToBuffer:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImage3dToBuffer>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyImage3dToBufferStateless:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImage3dToBufferStateless>>(*this, context, device); });
        break;
    case EBuiltInOps::CopyImageToImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::CopyImageToImage3d>>(*this, context, device); });
        break;
    case EBuiltInOps::FillImage3d:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::FillImage3d>>(*this, context, device); });
        break;
    case EBuiltInOps::VmeBlockMotionEstimateIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel>>(*this, context, device); });
        break;
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel>>(*this, context, device); });
        break;
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel>>(*this, context, device); });
        break;
    case EBuiltInOps::AuxTranslation:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::AuxTranslation>>(*this, context, device); });
        break;
    }
    return *operationBuilder.first;
}

BuiltInOwnershipWrapper::BuiltInOwnershipWrapper(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    takeOwnership(inputBuilder, context);
}
BuiltInOwnershipWrapper::~BuiltInOwnershipWrapper() {
    if (builder) {
        for (auto &kernel : builder->peekUsedKernels()) {
            kernel->setContext(nullptr);
            kernel->releaseOwnership();
        }
    }
}
void BuiltInOwnershipWrapper::takeOwnership(BuiltinDispatchInfoBuilder &inputBuilder, Context *context) {
    UNRECOVERABLE_IF(builder);
    builder = &inputBuilder;
    for (auto &kernel : builder->peekUsedKernels()) {
        kernel->takeOwnership();
        kernel->setContext(context);
    }
}

} // namespace NEO
