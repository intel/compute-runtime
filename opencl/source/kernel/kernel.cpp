/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_vme.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/kernel_info.h"

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/image_transformer.h"
#include "opencl/source/kernel/kernel_info_cl.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/source/sampler/sampler.h"

#include "patch_list.h"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace iOpenCL;

namespace NEO {
class Surface;

uint32_t Kernel::dummyPatchLocation = 0xbaddf00d;

Kernel::Kernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg)
    : executionEnvironment(programArg->getExecutionEnvironment()),
      program(programArg),
      clDevice(clDeviceArg),
      kernelInfo(kernelInfoArg) {
    program->retain();
    program->retainForKernel();
    imageTransformer.reset(new ImageTransformer);
    auto &deviceInfo = getDevice().getDevice().getDeviceInfo();
    if (kernelInfoArg.kernelDescriptor.kernelAttributes.simdSize == 1u) {
        auto &hwInfoConfig = *HwInfoConfig::get(getHardwareInfo().platform.eProductFamily);
        maxKernelWorkGroupSize = hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(getHardwareInfo(), static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice));
    } else {
        maxKernelWorkGroupSize = static_cast<uint32_t>(deviceInfo.maxWorkGroupSize);
    }
    slmTotalSize = kernelInfoArg.kernelDescriptor.kernelAttributes.slmInlineSize;
}

Kernel::~Kernel() {
    delete[] crossThreadData;
    crossThreadData = nullptr;
    crossThreadDataSize = 0;

    if (privateSurface) {
        program->peekExecutionEnvironment().memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(privateSurface);
        privateSurface = nullptr;
    }

    for (uint32_t i = 0; i < patchedArgumentsNum; i++) {
        if (SAMPLER_OBJ == getKernelArguments()[i].type) {
            auto sampler = castToObject<Sampler>(kernelArguments.at(i).object);
            if (sampler) {
                sampler->decRefInternal();
            }
        }
    }

    kernelArgHandlers.clear();
    program->releaseForKernel();
    program->release();
}
// If dstOffsetBytes is not an invalid offset, then patches dst at dstOffsetBytes
// with src casted to DstT type.
template <typename DstT, typename SrcT>
inline void patch(const SrcT &src, void *dst, CrossThreadDataOffset dstOffsetBytes) {
    if (isValidOffset(dstOffsetBytes)) {
        DstT *patchLocation = reinterpret_cast<DstT *>(ptrOffset(dst, dstOffsetBytes));
        *patchLocation = static_cast<DstT>(src);
    }
}

void Kernel::patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const ArgDescPointer &arg) {
    if ((nullptr != crossThreadData) && isValidOffset(arg.stateless)) {
        auto pp = ptrOffset(crossThreadData, arg.stateless);
        uintptr_t addressToPatch = reinterpret_cast<uintptr_t>(ptrToPatchInCrossThreadData);
        patchWithRequiredSize(pp, arg.pointerSize, addressToPatch);
        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            PatchInfoData patchInfoData(addressToPatch, 0u, PatchInfoAllocationType::KernelArg, reinterpret_cast<uint64_t>(crossThreadData), arg.stateless, PatchInfoAllocationType::IndirectObjectHeap, arg.pointerSize);
            this->patchInfoDataList.push_back(patchInfoData);
        }
    }

    void *ssh = getSurfaceStateHeap();
    if ((nullptr != ssh) && isValidOffset(arg.bindful)) {
        auto surfaceState = ptrOffset(ssh, arg.bindful);
        void *addressToPatch = reinterpret_cast<void *>(allocation.getGpuAddressToPatch());
        size_t sizeToPatch = allocation.getUnderlyingBufferSize();
        Buffer::setSurfaceState(&clDevice.getDevice(), surfaceState, false, false, sizeToPatch, addressToPatch, 0, &allocation, 0, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }
}

cl_int Kernel::initialize() {
    this->kernelHasIndirectAccess = false;
    auto pClDevice = &getDevice();
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    reconfigureKernel();
    auto &hwInfo = pClDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    const auto &implicitArgs = kernelDescriptor.payloadMappings.implicitArgs;
    const auto &explicitArgs = kernelDescriptor.payloadMappings.explicitArgs;
    auto maxSimdSize = kernelInfo.getMaxSimdSize();
    const auto &heapInfo = kernelInfo.heapInfo;

    if (maxSimdSize != 1 && maxSimdSize < hwHelper.getMinimalSIMDSize()) {
        return CL_INVALID_KERNEL;
    }

    if (kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs) {
        pImplicitArgs = std::make_unique<ImplicitArgs>();
        *pImplicitArgs = {};
        pImplicitArgs->structSize = sizeof(ImplicitArgs);
        pImplicitArgs->structVersion = 0;
        pImplicitArgs->simdWidth = maxSimdSize;
    }

    crossThreadDataSize = kernelDescriptor.kernelAttributes.crossThreadDataSize;

    // now allocate our own cross-thread data, if necessary
    if (crossThreadDataSize) {
        crossThreadData = new char[crossThreadDataSize];

        if (kernelInfo.crossThreadData) {
            memcpy_s(crossThreadData, crossThreadDataSize,
                     kernelInfo.crossThreadData, crossThreadDataSize);
        } else {
            memset(crossThreadData, 0x00, crossThreadDataSize);
        }

        auto crossThread = reinterpret_cast<uint32_t *>(crossThreadData);
        auto setArgsIfValidOffset = [&](uint32_t *&crossThreadData, NEO::CrossThreadDataOffset offset, uint32_t value) {
            if (isValidOffset(offset)) {
                crossThreadData = ptrOffset(crossThread, offset);
                *crossThreadData = value;
            }
        };
        setArgsIfValidOffset(maxWorkGroupSizeForCrossThreadData, implicitArgs.maxWorkGroupSize, maxKernelWorkGroupSize);
        setArgsIfValidOffset(dataParameterSimdSize, implicitArgs.simdSize, maxSimdSize);
        setArgsIfValidOffset(preferredWkgMultipleOffset, implicitArgs.preferredWkgMultiple, maxSimdSize);
        setArgsIfValidOffset(parentEventOffset, implicitArgs.deviceSideEnqueueParentEvent, undefined<uint32_t>);
    }

    // allocate our own SSH, if necessary
    sshLocalSize = heapInfo.SurfaceStateHeapSize;
    if (sshLocalSize) {
        pSshLocal = std::make_unique<char[]>(sshLocalSize);

        // copy the ssh into our local copy
        memcpy_s(pSshLocal.get(), sshLocalSize,
                 heapInfo.pSsh, heapInfo.SurfaceStateHeapSize);
    }
    numberOfBindingTableStates = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    localBindingTableOffset = kernelDescriptor.payloadMappings.bindingTable.tableOffset;

    // patch crossthread data and ssh with inline surfaces, if necessary
    auto status = patchPrivateSurface();
    if (CL_SUCCESS != status) {
        return status;
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless)) {
        DEBUG_BREAK_IF(program->getConstantSurface(rootDeviceIndex) == nullptr);
        uintptr_t constMemory = isBuiltIn ? (uintptr_t)program->getConstantSurface(rootDeviceIndex)->getUnderlyingBuffer() : (uintptr_t)program->getConstantSurface(rootDeviceIndex)->getGpuAddressToPatch();

        const auto &arg = kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress;
        patchWithImplicitSurface(reinterpret_cast<void *>(constMemory), *program->getConstantSurface(rootDeviceIndex), arg);
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless)) {
        DEBUG_BREAK_IF(program->getGlobalSurface(rootDeviceIndex) == nullptr);
        uintptr_t globalMemory = isBuiltIn ? (uintptr_t)program->getGlobalSurface(rootDeviceIndex)->getUnderlyingBuffer() : (uintptr_t)program->getGlobalSurface(rootDeviceIndex)->getGpuAddressToPatch();

        const auto &arg = kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress;
        patchWithImplicitSurface(reinterpret_cast<void *>(globalMemory), *program->getGlobalSurface(rootDeviceIndex), arg);
    }

    // Patch Surface State Heap
    bool useGlobalAtomics = kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics;

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                      kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress.bindful);
        Buffer::setSurfaceState(&pClDevice->getDevice(), surfaceState, false, false, 0, nullptr, 0, nullptr, 0, 0, useGlobalAtomics, areMultipleSubDevicesInContext());
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                      kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful);
        Buffer::setSurfaceState(&pClDevice->getDevice(), surfaceState, false, false, 0, nullptr, 0, nullptr, 0, 0, useGlobalAtomics, areMultipleSubDevicesInContext());
    }

    auto &threadArbitrationPolicy = const_cast<ThreadArbitrationPolicy &>(kernelInfo.kernelDescriptor.kernelAttributes.threadArbitrationPolicy);
    if (threadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) {
        threadArbitrationPolicy = static_cast<ThreadArbitrationPolicy>(hwHelper.getDefaultThreadArbitrationPolicy());
    }
    if (false == kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    }

    auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);

    auxTranslationRequired = !program->getIsBuiltIn() && HwHelper::compressedBuffersSupported(hwInfo) && clHwHelper.requiresAuxResolves(kernelInfo, hwInfo);

    if (DebugManager.flags.ForceAuxTranslationEnabled.get() != -1) {
        auxTranslationRequired &= !!DebugManager.flags.ForceAuxTranslationEnabled.get();
    }
    if (auxTranslationRequired) {
        program->getContextPtr()->setResolvesRequiredInKernels(true);
    }

    if (program->isKernelDebugEnabled() && isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful)) {
        debugEnabled = true;
    }
    auto numArgs = explicitArgs.size();
    slmSizes.resize(numArgs);

    this->kernelHasIndirectAccess |= kernelInfo.kernelDescriptor.kernelAttributes.hasNonKernelArgLoad ||
                                     kernelInfo.kernelDescriptor.kernelAttributes.hasNonKernelArgStore ||
                                     kernelInfo.kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic;

    provideInitializationHints();
    // resolve the new kernel info to account for kernel handlers
    // I think by this time we have decoded the binary and know the number of args etc.
    // double check this assumption
    bool usingBuffers = false;
    kernelArguments.resize(numArgs);
    kernelArgHandlers.resize(numArgs);
    kernelArgRequiresCacheFlush.resize(numArgs);

    for (uint32_t i = 0; i < numArgs; ++i) {
        storeKernelArg(i, NONE_OBJ, nullptr, nullptr, 0);

        // set the argument handler
        const auto &arg = explicitArgs[i];
        if (arg.is<ArgDescriptor::ArgTPointer>()) {
            if (arg.getTraits().addressQualifier == KernelArgMetadata::AddrLocal) {
                kernelArgHandlers[i] = &Kernel::setArgLocal;
            } else if (arg.getTraits().typeQualifiers.pipeQual) {
                kernelArgHandlers[i] = &Kernel::setArgPipe;
                kernelArguments[i].type = PIPE_OBJ;
            } else {
                kernelArgHandlers[i] = &Kernel::setArgBuffer;
                kernelArguments[i].type = BUFFER_OBJ;
                usingBuffers = true;
                allBufferArgsStateful &= static_cast<uint32_t>(arg.as<ArgDescPointer>().isPureStateful());
            }
        } else if (arg.is<ArgDescriptor::ArgTImage>()) {
            kernelArgHandlers[i] = &Kernel::setArgImage;
            kernelArguments[i].type = IMAGE_OBJ;
            usingImages = true;
        } else if (arg.is<ArgDescriptor::ArgTSampler>()) {
            if (arg.getExtendedTypeInfo().isAccelerator) {
                kernelArgHandlers[i] = &Kernel::setArgAccelerator;
            } else {
                kernelArgHandlers[i] = &Kernel::setArgSampler;
                kernelArguments[i].type = SAMPLER_OBJ;
            }
        } else {
            kernelArgHandlers[i] = &Kernel::setArgImmediate;
        }
    }

    if (usingImages && !usingBuffers) {
        usingImagesOnly = true;
    }

    return CL_SUCCESS;
}

cl_int Kernel::patchPrivateSurface() {
    auto pClDevice = &getDevice();
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    auto perHwThreadPrivateMemorySize = kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize;
    if (perHwThreadPrivateMemorySize) {
        if (!privateSurface) {
            privateSurfaceSize = KernelHelper::getPrivateSurfaceSize(perHwThreadPrivateMemorySize, pClDevice->getSharedDeviceInfo().computeUnitsUsedForScratch);
            DEBUG_BREAK_IF(privateSurfaceSize == 0);

            if (privateSurfaceSize > std::numeric_limits<uint32_t>::max()) {
                return CL_OUT_OF_RESOURCES;
            }
            privateSurface = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(
                {rootDeviceIndex,
                 static_cast<size_t>(privateSurfaceSize),
                 AllocationType::PRIVATE_SURFACE,
                 pClDevice->getDeviceBitfield()});
            if (privateSurface == nullptr) {
                return CL_OUT_OF_RESOURCES;
            }
        }

        const auto &privateMemoryAddress = kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress;
        patchWithImplicitSurface(reinterpret_cast<void *>(privateSurface->getGpuAddressToPatch()), *privateSurface, privateMemoryAddress);
    }
    return CL_SUCCESS;
}

cl_int Kernel::cloneKernel(Kernel *pSourceKernel) {
    // copy cross thread data to store arguments set to source kernel with clSetKernelArg on immediate data (non-pointer types)
    memcpy_s(crossThreadData, crossThreadDataSize,
             pSourceKernel->crossThreadData, pSourceKernel->crossThreadDataSize);
    DEBUG_BREAK_IF(pSourceKernel->crossThreadDataSize != crossThreadDataSize);

    [[maybe_unused]] auto status = patchPrivateSurface();
    DEBUG_BREAK_IF(status != CL_SUCCESS);

    // copy arguments set to source kernel with clSetKernelArg or clSetKernelArgSVMPointer
    for (uint32_t i = 0; i < pSourceKernel->kernelArguments.size(); i++) {
        if (0 == pSourceKernel->getKernelArgInfo(i).size) {
            // skip copying arguments that haven't been set to source kernel
            continue;
        }
        switch (pSourceKernel->kernelArguments[i].type) {
        case NONE_OBJ:
            // all arguments with immediate data (non-pointer types) have been copied in cross thread data
            storeKernelArg(i, NONE_OBJ, nullptr, nullptr, pSourceKernel->getKernelArgInfo(i).size);
            patchedArgumentsNum++;
            kernelArguments[i].isPatched = true;
            break;
        case SVM_OBJ:
            setArgSvm(i, pSourceKernel->getKernelArgInfo(i).size, const_cast<void *>(pSourceKernel->getKernelArgInfo(i).value),
                      pSourceKernel->getKernelArgInfo(i).svmAllocation, pSourceKernel->getKernelArgInfo(i).svmFlags);
            break;
        case SVM_ALLOC_OBJ:
            setArgSvmAlloc(i, const_cast<void *>(pSourceKernel->getKernelArgInfo(i).value),
                           (GraphicsAllocation *)pSourceKernel->getKernelArgInfo(i).object,
                           pSourceKernel->getKernelArgInfo(i).allocId);
            break;
        default:
            setArg(i, pSourceKernel->getKernelArgInfo(i).size, pSourceKernel->getKernelArgInfo(i).value);
            break;
        }
    }

    // copy additional information other than argument values set to source kernel with clSetKernelExecInfo
    for (auto &gfxAlloc : pSourceKernel->kernelSvmGfxAllocations) {
        kernelSvmGfxAllocations.push_back(gfxAlloc);
    }
    for (auto &gfxAlloc : pSourceKernel->kernelUnifiedMemoryGfxAllocations) {
        kernelUnifiedMemoryGfxAllocations.push_back(gfxAlloc);
    }

    if (pImplicitArgs) {
        memcpy_s(pImplicitArgs.get(), sizeof(ImplicitArgs), pSourceKernel->getImplicitArgs(), sizeof(ImplicitArgs));
    }
    this->isBuiltIn = pSourceKernel->isBuiltIn;

    return CL_SUCCESS;
}

cl_int Kernel::getInfo(cl_kernel_info paramName, size_t paramValueSize,
                       void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    cl_uint numArgs = 0;
    const _cl_program *prog;
    const _cl_context *ctxt;
    cl_uint refCount = 0;
    uint64_t nonCannonizedGpuAddress = 0llu;
    auto gmmHelper = clDevice.getDevice().getGmmHelper();

    switch (paramName) {
    case CL_KERNEL_FUNCTION_NAME:
        pSrc = kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str();
        srcSize = kernelInfo.kernelDescriptor.kernelMetadata.kernelName.length() + 1;
        break;

    case CL_KERNEL_NUM_ARGS:
        srcSize = sizeof(cl_uint);
        numArgs = static_cast<cl_uint>(kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size());
        pSrc = &numArgs;
        break;

    case CL_KERNEL_CONTEXT:
        ctxt = &program->getContext();
        srcSize = sizeof(ctxt);
        pSrc = &ctxt;
        break;

    case CL_KERNEL_PROGRAM:
        prog = program;
        srcSize = sizeof(prog);
        pSrc = &prog;
        break;

    case CL_KERNEL_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(pMultiDeviceKernel->getRefApiCount());
        srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_KERNEL_ATTRIBUTES:
        pSrc = kernelInfo.kernelDescriptor.kernelMetadata.kernelLanguageAttributes.c_str();
        srcSize = kernelInfo.kernelDescriptor.kernelMetadata.kernelLanguageAttributes.length() + 1;
        break;

    case CL_KERNEL_BINARY_PROGRAM_INTEL:
        pSrc = getKernelHeap();
        srcSize = getKernelHeapSize();
        break;
    case CL_KERNEL_BINARY_GPU_ADDRESS_INTEL:
        nonCannonizedGpuAddress = gmmHelper->decanonize(kernelInfo.kernelAllocation->getGpuAddress());
        pSrc = &nonCannonizedGpuAddress;
        srcSize = sizeof(nonCannonizedGpuAddress);
        break;
    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
}

cl_int Kernel::getArgInfo(cl_uint argIndex, cl_kernel_arg_info paramName, size_t paramValueSize,
                          void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    const auto &args = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs;

    if (argIndex >= args.size()) {
        retVal = CL_INVALID_ARG_INDEX;
        return retVal;
    }

    const auto &argTraits = args[argIndex].getTraits();
    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[argIndex];

    cl_kernel_arg_address_qualifier addressQualifier;
    cl_kernel_arg_access_qualifier accessQualifier;
    cl_kernel_arg_type_qualifier typeQualifier;

    switch (paramName) {
    case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
        addressQualifier = asClKernelArgAddressQualifier(argTraits.getAddressQualifier());
        srcSize = sizeof(addressQualifier);
        pSrc = &addressQualifier;
        break;

    case CL_KERNEL_ARG_ACCESS_QUALIFIER:
        accessQualifier = asClKernelArgAccessQualifier(argTraits.getAccessQualifier());
        srcSize = sizeof(accessQualifier);
        pSrc = &accessQualifier;
        break;

    case CL_KERNEL_ARG_TYPE_QUALIFIER:
        typeQualifier = asClKernelArgTypeQualifier(argTraits.typeQualifiers);
        srcSize = sizeof(typeQualifier);
        pSrc = &typeQualifier;
        break;

    case CL_KERNEL_ARG_TYPE_NAME:
        srcSize = argMetadata.type.length() + 1;
        pSrc = argMetadata.type.c_str();
        break;

    case CL_KERNEL_ARG_NAME:
        srcSize = argMetadata.argName.length() + 1;
        pSrc = argMetadata.argName.c_str();
        break;

    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
}

cl_int Kernel::getWorkGroupInfo(cl_kernel_work_group_info paramName,
                                size_t paramValueSize, void *paramValue,
                                size_t *paramValueSizeRet) const {
    cl_int retVal = CL_INVALID_VALUE;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    struct size_t3 {
        size_t val[3];
    } requiredWorkGroupSize;
    cl_ulong localMemorySize;
    const auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    size_t preferredWorkGroupSizeMultiple = 0;
    cl_ulong scratchSize;
    cl_ulong privateMemSize;
    size_t maxWorkgroupSize;
    const auto &hwInfo = clDevice.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    switch (paramName) {
    case CL_KERNEL_WORK_GROUP_SIZE:
        maxWorkgroupSize = maxKernelWorkGroupSize;
        if (DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()) {
            auto divisionSize = CommonConstants::maximalSimdSize / kernelInfo.getMaxSimdSize();
            maxWorkgroupSize /= divisionSize;
        }
        srcSize = sizeof(maxWorkgroupSize);
        pSrc = &maxWorkgroupSize;
        break;

    case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
        requiredWorkGroupSize.val[0] = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
        requiredWorkGroupSize.val[1] = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
        requiredWorkGroupSize.val[2] = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];
        srcSize = sizeof(requiredWorkGroupSize);
        pSrc = &requiredWorkGroupSize;
        break;

    case CL_KERNEL_LOCAL_MEM_SIZE:
        localMemorySize = kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize;
        srcSize = sizeof(localMemorySize);
        pSrc = &localMemorySize;
        break;

    case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
        preferredWorkGroupSizeMultiple = kernelInfo.getMaxSimdSize();
        if (hwHelper.isFusedEuDispatchEnabled(hwInfo, kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion)) {
            preferredWorkGroupSizeMultiple *= 2;
        }
        srcSize = sizeof(preferredWorkGroupSizeMultiple);
        pSrc = &preferredWorkGroupSizeMultiple;
        break;

    case CL_KERNEL_SPILL_MEM_SIZE_INTEL:
        scratchSize = kernelDescriptor.kernelAttributes.perThreadScratchSize[0];
        srcSize = sizeof(scratchSize);
        pSrc = &scratchSize;
        break;
    case CL_KERNEL_PRIVATE_MEM_SIZE:
        privateMemSize = clHwHelper.getKernelPrivateMemSize(kernelInfo);
        srcSize = sizeof(privateMemSize);
        pSrc = &privateMemSize;
        break;
    case CL_KERNEL_EU_THREAD_COUNT_INTEL:
        srcSize = sizeof(cl_uint);
        pSrc = &this->getKernelInfo().kernelDescriptor.kernelAttributes.numThreadsRequired;
        break;
    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
}

cl_int Kernel::getSubGroupInfo(cl_kernel_sub_group_info paramName,
                               size_t inputValueSize, const void *inputValue,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet) const {
    size_t numDimensions = 0;
    size_t wgs = 1;
    auto maxSimdSize = static_cast<size_t>(kernelInfo.getMaxSimdSize());
    auto maxRequiredWorkGroupSize = static_cast<size_t>(kernelInfo.getMaxRequiredWorkGroupSize(getMaxKernelWorkGroupSize()));
    auto largestCompiledSIMDSize = static_cast<size_t>(kernelInfo.getMaxSimdSize());

    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    if ((paramName == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ||
        (paramName == CL_KERNEL_MAX_NUM_SUB_GROUPS) ||
        (paramName == CL_KERNEL_COMPILE_NUM_SUB_GROUPS)) {
        if (clDevice.areOcl21FeaturesEnabled() == false) {
            return CL_INVALID_OPERATION;
        }
    }

    if ((paramName == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR) ||
        (paramName == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR)) {
        if (!inputValue) {
            return CL_INVALID_VALUE;
        }
        if (inputValueSize % sizeof(size_t) != 0) {
            return CL_INVALID_VALUE;
        }
        numDimensions = inputValueSize / sizeof(size_t);
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(clDevice.getDeviceInfo().maxWorkItemDimensions)) {
            return CL_INVALID_VALUE;
        }
    }

    if (paramName == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) {
        if (!paramValue) {
            return CL_INVALID_VALUE;
        }
        if (paramValueSize % sizeof(size_t) != 0) {
            return CL_INVALID_VALUE;
        }
        numDimensions = paramValueSize / sizeof(size_t);
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(clDevice.getDeviceInfo().maxWorkItemDimensions)) {
            return CL_INVALID_VALUE;
        }
    }

    switch (paramName) {
    case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR: {
        return changeGetInfoStatusToCLResultType(info.set<size_t>(maxSimdSize));
    }
    case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR: {
        for (size_t i = 0; i < numDimensions; i++) {
            wgs *= ((size_t *)inputValue)[i];
        }
        return changeGetInfoStatusToCLResultType(
            info.set<size_t>((wgs / maxSimdSize) + std::min(static_cast<size_t>(1), wgs % maxSimdSize))); // add 1 if WGS % maxSimdSize != 0
    }
    case CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT: {
        auto subGroupsNum = *(size_t *)inputValue;
        auto workGroupSize = subGroupsNum * largestCompiledSIMDSize;
        // return workgroup size in first dimension, the rest shall be 1 in positive case
        if (workGroupSize > maxRequiredWorkGroupSize) {
            workGroupSize = 0;
        }
        // If no work group size can accommodate the requested number of subgroups, return 0 in each element of the returned array.
        switch (numDimensions) {
        case 1:
            return changeGetInfoStatusToCLResultType(info.set<size_t>(workGroupSize));
        case 2:
            struct size_t2 {
                size_t val[2];
            } workGroupSize2;
            workGroupSize2.val[0] = workGroupSize;
            workGroupSize2.val[1] = (workGroupSize > 0) ? 1 : 0;
            return changeGetInfoStatusToCLResultType(info.set<size_t2>(workGroupSize2));
        default:
            struct size_t3 {
                size_t val[3];
            } workGroupSize3;
            workGroupSize3.val[0] = workGroupSize;
            workGroupSize3.val[1] = (workGroupSize > 0) ? 1 : 0;
            workGroupSize3.val[2] = (workGroupSize > 0) ? 1 : 0;
            return changeGetInfoStatusToCLResultType(info.set<size_t3>(workGroupSize3));
        }
    }
    case CL_KERNEL_MAX_NUM_SUB_GROUPS: {
        // round-up maximum number of subgroups
        return changeGetInfoStatusToCLResultType(info.set<size_t>(Math::divideAndRoundUp(maxRequiredWorkGroupSize, largestCompiledSIMDSize)));
    }
    case CL_KERNEL_COMPILE_NUM_SUB_GROUPS: {
        return changeGetInfoStatusToCLResultType(info.set<size_t>(static_cast<size_t>(kernelInfo.kernelDescriptor.kernelMetadata.compiledSubGroupsNumber)));
    }
    case CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL: {
        return changeGetInfoStatusToCLResultType(info.set<size_t>(kernelInfo.kernelDescriptor.kernelMetadata.requiredSubGroupSize));
    }
    default:
        return CL_INVALID_VALUE;
    }
}

const void *Kernel::getKernelHeap() const {
    return kernelInfo.heapInfo.pKernelHeap;
}

size_t Kernel::getKernelHeapSize() const {
    return kernelInfo.heapInfo.KernelHeapSize;
}

void Kernel::substituteKernelHeap(void *newKernelHeap, size_t newKernelHeapSize) {
    KernelInfo *pKernelInfo = const_cast<KernelInfo *>(&kernelInfo);
    void **pKernelHeap = const_cast<void **>(&pKernelInfo->heapInfo.pKernelHeap);
    *pKernelHeap = newKernelHeap;
    auto &heapInfo = pKernelInfo->heapInfo;
    heapInfo.KernelHeapSize = static_cast<uint32_t>(newKernelHeapSize);
    pKernelInfo->isKernelHeapSubstituted = true;
    auto memoryManager = executionEnvironment.memoryManager.get();

    auto currentAllocationSize = pKernelInfo->kernelAllocation->getUnderlyingBufferSize();
    bool status = false;

    const auto &hwInfo = clDevice.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    size_t isaPadding = hwHelper.getPaddingForISAAllocation();
    if (currentAllocationSize >= newKernelHeapSize + isaPadding) {
        auto &hwInfo = clDevice.getDevice().getHardwareInfo();
        const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
        status = MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *pKernelInfo->getGraphicsAllocation()),
                                                                  clDevice.getDevice(), pKernelInfo->getGraphicsAllocation(), 0, newKernelHeap,
                                                                  static_cast<size_t>(newKernelHeapSize));
    } else {
        memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(pKernelInfo->kernelAllocation);
        pKernelInfo->kernelAllocation = nullptr;
        status = pKernelInfo->createKernelAllocation(clDevice.getDevice(), isBuiltIn);
    }
    UNRECOVERABLE_IF(!status);
}

bool Kernel::isKernelHeapSubstituted() const {
    return kernelInfo.isKernelHeapSubstituted;
}

uint64_t Kernel::getKernelId() const {
    return kernelInfo.kernelId;
}

void Kernel::setKernelId(uint64_t newKernelId) {
    KernelInfo *pKernelInfo = const_cast<KernelInfo *>(&kernelInfo);
    pKernelInfo->kernelId = newKernelId;
}
uint32_t Kernel::getStartOffset() const {
    return this->startOffset;
}
void Kernel::setStartOffset(uint32_t offset) {
    this->startOffset = offset;
}

void *Kernel::getSurfaceStateHeap() const {
    return pSshLocal.get();
}

size_t Kernel::getDynamicStateHeapSize() const {
    return kernelInfo.heapInfo.DynamicStateHeapSize;
}

const void *Kernel::getDynamicStateHeap() const {
    return kernelInfo.heapInfo.pDsh;
}

size_t Kernel::getSurfaceStateHeapSize() const {
    return sshLocalSize;
}

size_t Kernel::getNumberOfBindingTableStates() const {
    return numberOfBindingTableStates;
}

void Kernel::resizeSurfaceStateHeap(void *pNewSsh, size_t newSshSize, size_t newBindingTableCount, size_t newBindingTableOffset) {
    pSshLocal.reset(static_cast<char *>(pNewSsh));
    sshLocalSize = static_cast<uint32_t>(newSshSize);
    numberOfBindingTableStates = newBindingTableCount;
    localBindingTableOffset = newBindingTableOffset;
}

void Kernel::markArgPatchedAndResolveArgs(uint32_t argIndex) {
    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
    }
    if (program->getContextPtr() && getContext().getRootDeviceIndices().size() > 1u && Kernel::isMemObj(kernelArguments[argIndex].type) && kernelArguments[argIndex].object) {
        auto argMemObj = castToObjectOrAbort<MemObj>(reinterpret_cast<cl_mem>(kernelArguments[argIndex].object));
        auto memObj = argMemObj->getHighestRootMemObj();
        auto migrateRequiredForArg = memObj->getMultiGraphicsAllocation().requiresMigrations();

        if (migratableArgsMap.find(argIndex) == migratableArgsMap.end() && migrateRequiredForArg) {
            migratableArgsMap.insert({argIndex, memObj});
        } else if (migrateRequiredForArg) {
            migratableArgsMap[argIndex] = memObj;
        } else {
            migratableArgsMap.erase(argIndex);
        }
    }

    resolveArgs();
}

cl_int Kernel::setArg(uint32_t argIndex, size_t argSize, const void *argVal) {
    cl_int retVal = CL_SUCCESS;
    bool updateExposedKernel = true;
    auto argWasUncacheable = false;
    if (kernelInfo.builtinDispatchBuilder != nullptr) {
        updateExposedKernel = kernelInfo.builtinDispatchBuilder->setExplicitArg(argIndex, argSize, argVal, retVal);
    }
    if (updateExposedKernel) {
        if (argIndex >= kernelArgHandlers.size()) {
            return CL_INVALID_ARG_INDEX;
        }
        argWasUncacheable = kernelArguments[argIndex].isStatelessUncacheable;
        auto argHandler = kernelArgHandlers[argIndex];
        retVal = (this->*argHandler)(argIndex, argSize, argVal);
    }
    if (retVal == CL_SUCCESS) {
        auto argIsUncacheable = kernelArguments[argIndex].isStatelessUncacheable;
        statelessUncacheableArgsCount += (argIsUncacheable ? 1 : 0) - (argWasUncacheable ? 1 : 0);
        markArgPatchedAndResolveArgs(argIndex);
    }
    return retVal;
}

cl_int Kernel::setArg(uint32_t argIndex, uint32_t argVal) {
    return setArg(argIndex, sizeof(argVal), &argVal);
}

cl_int Kernel::setArg(uint32_t argIndex, uint64_t argVal) {
    return setArg(argIndex, sizeof(argVal), &argVal);
}

cl_int Kernel::setArg(uint32_t argIndex, cl_mem argVal) {
    return setArg(argIndex, sizeof(argVal), &argVal);
}

cl_int Kernel::setArg(uint32_t argIndex, cl_mem argVal, uint32_t mipLevel) {
    auto retVal = setArgImageWithMipLevel(argIndex, sizeof(argVal), &argVal, mipLevel);
    if (retVal == CL_SUCCESS) {
        markArgPatchedAndResolveArgs(argIndex);
    }
    return retVal;
}

void *Kernel::patchBufferOffset(const ArgDescPointer &argAsPtr, void *svmPtr, GraphicsAllocation *svmAlloc) {
    if (isUndefinedOffset(argAsPtr.bufferOffset)) {
        return svmPtr;
    }
    void *ptrToPatch = svmPtr;
    if (svmAlloc != nullptr) {
        ptrToPatch = reinterpret_cast<void *>(svmAlloc->getGpuAddressToPatch());
    }

    constexpr uint32_t minimumAlignment = 4;
    ptrToPatch = alignDown(ptrToPatch, minimumAlignment);
    DEBUG_BREAK_IF(ptrDiff(svmPtr, ptrToPatch) != static_cast<uint32_t>(ptrDiff(svmPtr, ptrToPatch)));
    uint32_t offsetToPatch = static_cast<uint32_t>(ptrDiff(svmPtr, ptrToPatch));

    patch<uint32_t, uint32_t>(offsetToPatch, getCrossThreadData(), argAsPtr.bufferOffset);
    return ptrToPatch;
}

cl_int Kernel::setArgSvm(uint32_t argIndex, size_t svmAllocSize, void *svmPtr, GraphicsAllocation *svmAlloc, cl_mem_flags svmFlags) {
    const auto &argAsPtr = getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescPointer>();

    auto patchLocation = ptrOffset(getCrossThreadData(), argAsPtr.stateless);
    patchWithRequiredSize(patchLocation, argAsPtr.pointerSize, reinterpret_cast<uintptr_t>(svmPtr));

    void *ptrToPatch = patchBufferOffset(argAsPtr, svmPtr, svmAlloc);
    if (isValidOffset(argAsPtr.bindful)) {
        auto surfaceState = ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful);
        Buffer::setSurfaceState(&getDevice().getDevice(), surfaceState, false, false, svmAllocSize + ptrDiff(svmPtr, ptrToPatch), ptrToPatch, 0, svmAlloc, svmFlags, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }

    storeKernelArg(argIndex, SVM_OBJ, nullptr, svmPtr, sizeof(void *), svmAlloc, svmFlags);
    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
    }
    if (svmPtr != nullptr && isBuiltIn == false) {
        this->anyKernelArgumentUsingSystemMemory |= true;
    }
    addAllocationToCacheFlushVector(argIndex, svmAlloc);
    return CL_SUCCESS;
}

cl_int Kernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc, uint32_t allocId) {
    DBG_LOG_INPUTS("setArgBuffer svm_alloc", svmAlloc);

    const auto &argAsPtr = getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescPointer>();

    auto patchLocation = ptrOffset(getCrossThreadData(), argAsPtr.stateless);
    patchWithRequiredSize(patchLocation, argAsPtr.pointerSize, reinterpret_cast<uintptr_t>(svmPtr));

    auto &kernelArgInfo = kernelArguments[argIndex];

    bool disableL3 = false;
    bool forceNonAuxMode = false;
    bool isAuxTranslationKernel = (AuxTranslationDirection::None != auxTranslationDirection);
    auto &hwInfo = getDevice().getHardwareInfo();
    auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (isAuxTranslationKernel) {
        if (((AuxTranslationDirection::AuxToNonAux == auxTranslationDirection) && argIndex == 1) ||
            ((AuxTranslationDirection::NonAuxToAux == auxTranslationDirection) && argIndex == 0)) {
            forceNonAuxMode = true;
        }
        disableL3 = (argIndex == 0);
    } else if (svmAlloc && svmAlloc->isCompressionEnabled() && clHwHelper.requiresNonAuxMode(argAsPtr, hwInfo)) {
        forceNonAuxMode = true;
    }

    bool argWasUncacheable = kernelArgInfo.isStatelessUncacheable;
    bool argIsUncacheable = svmAlloc ? svmAlloc->isUncacheable() : false;
    statelessUncacheableArgsCount += (argIsUncacheable ? 1 : 0) - (argWasUncacheable ? 1 : 0);

    void *ptrToPatch = patchBufferOffset(argAsPtr, svmPtr, svmAlloc);
    if (isValidOffset(argAsPtr.bindful)) {
        auto surfaceState = ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful);
        size_t allocSize = 0;
        size_t offset = 0;
        if (svmAlloc != nullptr) {
            allocSize = svmAlloc->getUnderlyingBufferSize();
            offset = ptrDiff(ptrToPatch, svmAlloc->getGpuAddressToPatch());
            allocSize -= offset;
        }
        Buffer::setSurfaceState(&getDevice().getDevice(), surfaceState, forceNonAuxMode, disableL3, allocSize, ptrToPatch, offset, svmAlloc, 0, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }

    storeKernelArg(argIndex, SVM_ALLOC_OBJ, svmAlloc, svmPtr, sizeof(uintptr_t));
    kernelArgInfo.allocId = allocId;
    kernelArgInfo.allocIdMemoryManagerCounter = allocId ? this->getContext().getSVMAllocsManager()->allocationsCounter.load() : 0u;
    kernelArgInfo.isSetToNullptr = nullptr == svmPtr;
    if (!kernelArgInfo.isPatched) {
        patchedArgumentsNum++;
        kernelArgInfo.isPatched = true;
    }
    if (!kernelArgInfo.isSetToNullptr && isBuiltIn == false) {
        if (svmAlloc != nullptr) {
            this->anyKernelArgumentUsingSystemMemory |= Kernel::graphicsAllocationTypeUseSystemMemory(svmAlloc->getAllocationType());
        } else {
            this->anyKernelArgumentUsingSystemMemory |= true;
        }
    }
    addAllocationToCacheFlushVector(argIndex, svmAlloc);
    return CL_SUCCESS;
}

void Kernel::storeKernelArg(uint32_t argIndex, kernelArgType argType, void *argObject,
                            const void *argValue, size_t argSize,
                            GraphicsAllocation *argSvmAlloc, cl_mem_flags argSvmFlags) {
    kernelArguments[argIndex].type = argType;
    kernelArguments[argIndex].object = argObject;
    kernelArguments[argIndex].value = argValue;
    kernelArguments[argIndex].size = argSize;
    kernelArguments[argIndex].svmAllocation = argSvmAlloc;
    kernelArguments[argIndex].svmFlags = argSvmFlags;
}

void Kernel::storeKernelArgAllocIdMemoryManagerCounter(uint32_t argIndex, uint32_t allocIdMemoryManagerCounter) {
    kernelArguments[argIndex].allocIdMemoryManagerCounter = allocIdMemoryManagerCounter;
}

const void *Kernel::getKernelArg(uint32_t argIndex) const {
    return kernelArguments[argIndex].object;
}

const Kernel::SimpleKernelArgInfo &Kernel::getKernelArgInfo(uint32_t argIndex) const {
    return kernelArguments[argIndex];
}

void Kernel::setSvmKernelExecInfo(GraphicsAllocation *argValue) {
    kernelSvmGfxAllocations.push_back(argValue);
    if (allocationForCacheFlush(argValue)) {
        svmAllocationsRequireCacheFlush = true;
    }
}

void Kernel::clearSvmKernelExecInfo() {
    kernelSvmGfxAllocations.clear();
    svmAllocationsRequireCacheFlush = false;
}

void Kernel::setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) {
    if (infoType == CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL) {
        this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = infoValue;
        return;
    }
    if (infoType == CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL) {
        this->unifiedMemoryControls.indirectHostAllocationsAllowed = infoValue;
        return;
    }
    if (infoType == CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL) {
        this->unifiedMemoryControls.indirectSharedAllocationsAllowed = infoValue;
        return;
    }
}

void Kernel::setUnifiedMemoryExecInfo(GraphicsAllocation *unifiedMemoryAllocation) {
    kernelUnifiedMemoryGfxAllocations.push_back(unifiedMemoryAllocation);
}

void Kernel::clearUnifiedMemoryExecInfo() {
    kernelUnifiedMemoryGfxAllocations.clear();
}

cl_int Kernel::setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) {
    switch (executionType) {
    case CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL:
        this->executionType = KernelExecutionType::Default;
        break;
    case CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL:
        this->executionType = KernelExecutionType::Concurrent;
        break;
    default: {
        return CL_INVALID_VALUE;
    }
    }
    return CL_SUCCESS;
}

void Kernel::getSuggestedLocalWorkSize(const cl_uint workDim, const size_t *globalWorkSize, const size_t *globalWorkOffset,
                                       size_t *localWorkSize) {
    UNRECOVERABLE_IF((workDim == 0) || (workDim > 3));
    UNRECOVERABLE_IF(globalWorkSize == nullptr);
    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{
        globalWorkSize[0],
        (workDim > 1) ? globalWorkSize[1] : 1,
        (workDim > 2) ? globalWorkSize[2] : 1};
    Vec3<size_t> offset{0, 0, 0};
    if (globalWorkOffset) {
        offset.x = globalWorkOffset[0];
        if (workDim > 1) {
            offset.y = globalWorkOffset[1];
            if (workDim > 2) {
                offset.z = globalWorkOffset[2];
            }
        }
    }

    Vec3<size_t> suggestedLws{0, 0, 0};

    if (kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] != 0) {
        suggestedLws.x = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
        suggestedLws.y = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
        suggestedLws.z = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];
    } else {
        uint32_t dispatchWorkDim = std::max(1U, std::max(gws.getSimplifiedDim(), offset.getSimplifiedDim()));
        const DispatchInfo dispatchInfo{&clDevice, this, dispatchWorkDim, gws, elws, offset};
        suggestedLws = computeWorkgroupSize(dispatchInfo);
    }

    localWorkSize[0] = suggestedLws.x;
    if (workDim > 1)
        localWorkSize[1] = suggestedLws.y;
    if (workDim > 2)
        localWorkSize[2] = suggestedLws.z;
}

uint32_t Kernel::getMaxWorkGroupCount(const cl_uint workDim, const size_t *localWorkSize, const CommandQueue *commandQueue) const {
    auto &hardwareInfo = getHardwareInfo();
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    auto engineGroupType = hwHelper.getEngineGroupType(commandQueue->getGpgpuEngine().getEngineType(),
                                                       commandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);

    const auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    auto dssCount = hardwareInfo.gtSystemInfo.DualSubSliceCount;
    if (dssCount == 0) {
        dssCount = hardwareInfo.gtSystemInfo.SubSliceCount;
    }
    auto availableThreadCount = hwHelper.calculateAvailableThreadCount(hardwareInfo, kernelDescriptor.kernelAttributes.numGrfRequired);

    auto barrierCount = kernelDescriptor.kernelAttributes.barrierCount;
    auto maxWorkGroupCount = KernelHelper::getMaxWorkGroupCount(kernelInfo.getMaxSimdSize(),
                                                                availableThreadCount,
                                                                dssCount,
                                                                dssCount * KB * hardwareInfo.capabilityTable.slmSize,
                                                                hwHelper.alignSlmSize(slmTotalSize),
                                                                static_cast<uint32_t>(hwHelper.getMaxBarrierRegisterPerSlice()),
                                                                barrierCount,
                                                                workDim,
                                                                localWorkSize);
    auto isEngineInstanced = commandQueue->getGpgpuCommandStreamReceiver().getOsContext().isEngineInstanced();
    maxWorkGroupCount = hwHelper.adjustMaxWorkGroupCount(maxWorkGroupCount, engineGroupType, hardwareInfo, isEngineInstanced);
    return maxWorkGroupCount;
}

inline void Kernel::makeArgsResident(CommandStreamReceiver &commandStreamReceiver) {
    auto numArgs = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                auto pageFaultManager = executionEnvironment.memoryManager->getPageFaultManager();
                if (pageFaultManager &&
                    this->isUnifiedMemorySyncRequired) {
                    pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(pSVMAlloc->getGpuAddress()));
                }
                commandStreamReceiver.makeResident(*pSVMAlloc);
            } else if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObjectOrAbort<MemObj>(clMem);
                auto image = castToObject<Image>(clMem);
                if (image && image->isImageFromImage()) {
                    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
                }
                commandStreamReceiver.makeResident(*memObj->getGraphicsAllocation(commandStreamReceiver.getRootDeviceIndex()));
                if (memObj->getMcsAllocation()) {
                    commandStreamReceiver.makeResident(*memObj->getMcsAllocation());
                }
            }
        }
    }
}

void Kernel::performKernelTuning(CommandStreamReceiver &commandStreamReceiver, const Vec3<size_t> &lws, const Vec3<size_t> &gws, const Vec3<size_t> &offsets, TimestampPacketContainer *timestampContainer) {
    auto performTunning = TunningType::DISABLED;

    if (DebugManager.flags.EnableKernelTunning.get() != -1) {
        performTunning = static_cast<TunningType>(DebugManager.flags.EnableKernelTunning.get());
    }

    if (performTunning == TunningType::SIMPLE) {
        this->singleSubdevicePreferredInCurrentEnqueue = !this->kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics;

    } else if (performTunning == TunningType::FULL) {
        KernelConfig config{gws, lws, offsets};

        auto submissionDataIt = this->kernelSubmissionMap.find(config);
        if (submissionDataIt == this->kernelSubmissionMap.end()) {
            KernelSubmissionData submissionData;
            submissionData.kernelStandardTimestamps = std::make_unique<TimestampPacketContainer>();
            submissionData.kernelSubdeviceTimestamps = std::make_unique<TimestampPacketContainer>();
            submissionData.status = TunningStatus::STANDARD_TUNNING_IN_PROGRESS;
            submissionData.kernelStandardTimestamps->assignAndIncrementNodesRefCounts(*timestampContainer);
            this->kernelSubmissionMap[config] = std::move(submissionData);
            this->singleSubdevicePreferredInCurrentEnqueue = false;
            return;
        }

        auto &submissionData = submissionDataIt->second;

        if (submissionData.status == TunningStatus::TUNNING_DONE) {
            this->singleSubdevicePreferredInCurrentEnqueue = submissionData.singleSubdevicePreferred;
        }

        if (submissionData.status == TunningStatus::SUBDEVICE_TUNNING_IN_PROGRESS) {
            if (this->hasTunningFinished(submissionData)) {
                submissionData.status = TunningStatus::TUNNING_DONE;
                submissionData.kernelStandardTimestamps.reset();
                submissionData.kernelSubdeviceTimestamps.reset();
                this->singleSubdevicePreferredInCurrentEnqueue = submissionData.singleSubdevicePreferred;
            } else {
                this->singleSubdevicePreferredInCurrentEnqueue = false;
            }
        }

        if (submissionData.status == TunningStatus::STANDARD_TUNNING_IN_PROGRESS) {
            submissionData.status = TunningStatus::SUBDEVICE_TUNNING_IN_PROGRESS;
            submissionData.kernelSubdeviceTimestamps->assignAndIncrementNodesRefCounts(*timestampContainer);
            this->singleSubdevicePreferredInCurrentEnqueue = true;
        }
    }
}

bool Kernel::hasTunningFinished(KernelSubmissionData &submissionData) {
    if (!this->hasRunFinished(submissionData.kernelStandardTimestamps.get()) ||
        !this->hasRunFinished(submissionData.kernelSubdeviceTimestamps.get())) {
        return false;
    }

    uint64_t globalStartTS = 0u;
    uint64_t globalEndTS = 0u;

    Event::getBoundaryTimestampValues(submissionData.kernelStandardTimestamps.get(), globalStartTS, globalEndTS);
    auto standardTSDiff = globalEndTS - globalStartTS;

    Event::getBoundaryTimestampValues(submissionData.kernelSubdeviceTimestamps.get(), globalStartTS, globalEndTS);
    auto subdeviceTSDiff = globalEndTS - globalStartTS;

    submissionData.singleSubdevicePreferred = standardTSDiff > subdeviceTSDiff;

    return true;
}

bool Kernel::hasRunFinished(TimestampPacketContainer *timestampContainer) {
    for (const auto &node : timestampContainer->peekNodes()) {
        for (uint32_t i = 0; i < node->getPacketsUsed(); i++) {
            if (node->getContextEndValue(i) == 1) {
                return false;
            }
        }
    }
    return true;
}

bool Kernel::isSingleSubdevicePreferred() const {
    return this->singleSubdevicePreferredInCurrentEnqueue || this->usesSyncBuffer();
}

void Kernel::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    auto rootDeviceIndex = commandStreamReceiver.getRootDeviceIndex();
    if (privateSurface) {
        commandStreamReceiver.makeResident(*privateSurface);
    }

    if (program->getConstantSurface(rootDeviceIndex)) {
        commandStreamReceiver.makeResident(*(program->getConstantSurface(rootDeviceIndex)));
    }

    if (program->getGlobalSurface(rootDeviceIndex)) {
        commandStreamReceiver.makeResident(*(program->getGlobalSurface(rootDeviceIndex)));
    }

    if (program->getExportedFunctionsSurface(rootDeviceIndex)) {
        commandStreamReceiver.makeResident(*(program->getExportedFunctionsSurface(rootDeviceIndex)));
    }

    for (auto gfxAlloc : kernelSvmGfxAllocations) {
        commandStreamReceiver.makeResident(*gfxAlloc);
    }

    auto pageFaultManager = program->peekExecutionEnvironment().memoryManager->getPageFaultManager();

    for (auto gfxAlloc : kernelUnifiedMemoryGfxAllocations) {
        commandStreamReceiver.makeResident(*gfxAlloc);
        if (pageFaultManager) {
            pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(gfxAlloc->getGpuAddress()));
        }
    }

    if (unifiedMemoryControls.indirectSharedAllocationsAllowed && pageFaultManager) {
        pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(this->getContext().getSVMAllocsManager());
    }
    makeArgsResident(commandStreamReceiver);

    auto kernelIsaAllocation = this->kernelInfo.kernelAllocation;
    if (kernelIsaAllocation) {
        commandStreamReceiver.makeResident(*kernelIsaAllocation);
    }

    gtpinNotifyMakeResident(this, &commandStreamReceiver);

    if (unifiedMemoryControls.indirectDeviceAllocationsAllowed ||
        unifiedMemoryControls.indirectHostAllocationsAllowed ||
        unifiedMemoryControls.indirectSharedAllocationsAllowed) {
        this->getContext().getSVMAllocsManager()->makeInternalAllocationsResident(commandStreamReceiver, unifiedMemoryControls.generateMask());
    }
}

void Kernel::getResidency(std::vector<Surface *> &dst) {
    if (privateSurface) {
        GeneralSurface *surface = new GeneralSurface(privateSurface);
        dst.push_back(surface);
    }

    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    if (program->getConstantSurface(rootDeviceIndex)) {
        GeneralSurface *surface = new GeneralSurface(program->getConstantSurface(rootDeviceIndex));
        dst.push_back(surface);
    }

    if (program->getGlobalSurface(rootDeviceIndex)) {
        GeneralSurface *surface = new GeneralSurface(program->getGlobalSurface(rootDeviceIndex));
        dst.push_back(surface);
    }

    if (program->getExportedFunctionsSurface(rootDeviceIndex)) {
        GeneralSurface *surface = new GeneralSurface(program->getExportedFunctionsSurface(rootDeviceIndex));
        dst.push_back(surface);
    }

    for (auto gfxAlloc : kernelSvmGfxAllocations) {
        GeneralSurface *surface = new GeneralSurface(gfxAlloc);
        dst.push_back(surface);
    }

    auto numArgs = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                bool needsMigration = false;
                auto pageFaultManager = executionEnvironment.memoryManager->getPageFaultManager();
                if (pageFaultManager &&
                    this->isUnifiedMemorySyncRequired) {
                    needsMigration = true;
                }
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                dst.push_back(new GeneralSurface(pSVMAlloc, needsMigration));
            } else if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObject<MemObj>(clMem);
                DEBUG_BREAK_IF(memObj == nullptr);
                dst.push_back(new MemObjSurface(memObj));
            }
        }
    }

    auto kernelIsaAllocation = this->kernelInfo.kernelAllocation;
    if (kernelIsaAllocation) {
        GeneralSurface *surface = new GeneralSurface(kernelIsaAllocation);
        dst.push_back(surface);
    }

    gtpinNotifyUpdateResidencyList(this, &dst);
}

bool Kernel::requiresCoherency() {
    auto numArgs = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                if (pSVMAlloc->isCoherent()) {
                    return true;
                }
            }

            if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObjectOrAbort<MemObj>(clMem);
                if (memObj->getMultiGraphicsAllocation().isCoherent()) {
                    return true;
                }
            }
        }
    }
    return false;
}

cl_int Kernel::setArgLocal(uint32_t argIndexIn,
                           size_t argSize,
                           const void *argVal) {
    storeKernelArg(argIndexIn, SLM_OBJ, nullptr, argVal, argSize);
    uint32_t *crossThreadData = reinterpret_cast<uint32_t *>(this->crossThreadData);
    uint32_t argIndex = argIndexIn;

    const auto &args = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs;
    const auto &currArg = args[argIndex];
    UNRECOVERABLE_IF(currArg.getTraits().getAddressQualifier() != KernelArgMetadata::AddrLocal);

    slmSizes[argIndex] = static_cast<uint32_t>(argSize);

    UNRECOVERABLE_IF(isUndefinedOffset(currArg.as<NEO::ArgDescPointer>().slmOffset));
    auto slmOffset = *ptrOffset(crossThreadData, currArg.as<ArgDescPointer>().slmOffset);
    slmOffset += static_cast<uint32_t>(argSize);

    ++argIndex;
    while (argIndex < slmSizes.size()) {
        if (args[argIndex].getTraits().getAddressQualifier() != KernelArgMetadata::AddrLocal) {
            ++argIndex;
            continue;
        }

        const auto &nextArg = args[argIndex].as<ArgDescPointer>();
        UNRECOVERABLE_IF(0 == nextArg.requiredSlmAlignment);

        slmOffset = alignUp<uint32_t>(slmOffset, nextArg.requiredSlmAlignment);

        auto patchLocation = ptrOffset(crossThreadData, nextArg.slmOffset);
        *patchLocation = slmOffset;

        slmOffset += static_cast<uint32_t>(slmSizes[argIndex]);
        ++argIndex;
    }

    slmTotalSize = kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize + alignUp(slmOffset, KB);

    return CL_SUCCESS;
}

cl_int Kernel::setArgBuffer(uint32_t argIndex,
                            size_t argSize,
                            const void *argVal) {

    if (argSize != sizeof(cl_mem *)) {
        return CL_INVALID_ARG_SIZE;
    }

    auto clMem = reinterpret_cast<const cl_mem *>(argVal);
    auto pClDevice = &getDevice();
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
    const auto &argAsPtr = arg.as<ArgDescPointer>();

    if (clMem && *clMem) {
        auto clMemObj = *clMem;
        DBG_LOG_INPUTS("setArgBuffer cl_mem", clMemObj);

        storeKernelArg(argIndex, BUFFER_OBJ, clMemObj, argVal, argSize);

        auto buffer = castToObject<Buffer>(clMemObj);
        if (!buffer) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto gfxAllocationType = buffer->getGraphicsAllocation(rootDeviceIndex)->getAllocationType();
        if (!isBuiltIn) {
            this->anyKernelArgumentUsingSystemMemory |= Kernel::graphicsAllocationTypeUseSystemMemory(gfxAllocationType);
        }

        if (buffer->peekSharingHandler()) {
            usingSharedObjArgs = true;
        }
        patchBufferOffset(argAsPtr, nullptr, nullptr);

        if (isValidOffset(argAsPtr.stateless)) {
            auto patchLocation = ptrOffset(crossThreadData, argAsPtr.stateless);
            uint64_t addressToPatch = buffer->setArgStateless(patchLocation, argAsPtr.pointerSize, rootDeviceIndex, !this->isBuiltIn);

            if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
                PatchInfoData patchInfoData(addressToPatch - buffer->getOffset(), static_cast<uint64_t>(buffer->getOffset()),
                                            PatchInfoAllocationType::KernelArg, reinterpret_cast<uint64_t>(crossThreadData),
                                            static_cast<uint64_t>(argAsPtr.stateless),
                                            PatchInfoAllocationType::IndirectObjectHeap, argAsPtr.pointerSize);
                this->patchInfoDataList.push_back(patchInfoData);
            }
        }

        bool disableL3 = false;
        bool forceNonAuxMode = false;
        bool isAuxTranslationKernel = (AuxTranslationDirection::None != auxTranslationDirection);
        auto graphicsAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
        auto &hwInfo = pClDevice->getHardwareInfo();
        auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);

        if (isAuxTranslationKernel) {
            if (((AuxTranslationDirection::AuxToNonAux == auxTranslationDirection) && argIndex == 1) ||
                ((AuxTranslationDirection::NonAuxToAux == auxTranslationDirection) && argIndex == 0)) {
                forceNonAuxMode = true;
            }
            disableL3 = (argIndex == 0);
        } else if (graphicsAllocation->isCompressionEnabled() && clHwHelper.requiresNonAuxMode(argAsPtr, hwInfo)) {
            forceNonAuxMode = true;
        }

        if (isValidOffset(argAsPtr.bindful)) {
            buffer->setArgStateful(ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful), forceNonAuxMode,
                                   disableL3, isAuxTranslationKernel, arg.isReadOnly(), pClDevice->getDevice(),
                                   kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
        } else if (isValidOffset(argAsPtr.bindless)) {
            buffer->setArgStateful(patchBindlessSurfaceState(graphicsAllocation, argAsPtr.bindless), forceNonAuxMode,
                                   disableL3, isAuxTranslationKernel, arg.isReadOnly(), pClDevice->getDevice(),
                                   kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
        }

        kernelArguments[argIndex].isStatelessUncacheable = argAsPtr.isPureStateful() ? false : buffer->isMemObjUncacheable();

        auto allocationForCacheFlush = graphicsAllocation;

        // if we make object uncacheable for surface state and there are only stateful accesses, then don't flush caches
        if (buffer->isMemObjUncacheableForSurfaceState() && argAsPtr.isPureStateful()) {
            allocationForCacheFlush = nullptr;
        }

        addAllocationToCacheFlushVector(argIndex, allocationForCacheFlush);
        return CL_SUCCESS;
    } else {
        storeKernelArg(argIndex, BUFFER_OBJ, nullptr, argVal, argSize);
        if (isValidOffset(argAsPtr.stateless)) {
            auto patchLocation = ptrOffset(getCrossThreadData(), argAsPtr.stateless);
            patchWithRequiredSize(patchLocation, argAsPtr.pointerSize, 0u);
        }

        if (isValidOffset(argAsPtr.bindful)) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful);
            Buffer::setSurfaceState(&pClDevice->getDevice(), surfaceState, false, false, 0, nullptr, 0, nullptr, 0, 0,
                                    kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
        }

        return CL_SUCCESS;
    }
}

cl_int Kernel::setArgPipe(uint32_t argIndex,
                          size_t argSize,
                          const void *argVal) {

    if (argSize != sizeof(cl_mem *)) {
        return CL_INVALID_ARG_SIZE;
    }

    auto clMem = reinterpret_cast<const cl_mem *>(argVal);

    if (clMem && *clMem) {
        auto clMemObj = *clMem;
        DBG_LOG_INPUTS("setArgPipe cl_mem", clMemObj);

        storeKernelArg(argIndex, PIPE_OBJ, clMemObj, argVal, argSize);

        auto memObj = castToObject<MemObj>(clMemObj);
        if (!memObj) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto pipe = castToObject<Pipe>(clMemObj);
        if (!pipe) {
            return CL_INVALID_ARG_VALUE;
        }

        if (memObj->getContext() != &(this->getContext())) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto rootDeviceIndex = getDevice().getRootDeviceIndex();
        const auto &argAsPtr = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescPointer>();

        auto patchLocation = ptrOffset(getCrossThreadData(), argAsPtr.stateless);
        pipe->setPipeArg(patchLocation, argAsPtr.pointerSize, rootDeviceIndex);

        if (isValidOffset(argAsPtr.bindful)) {
            auto graphicsAllocation = pipe->getGraphicsAllocation(rootDeviceIndex);
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful);
            Buffer::setSurfaceState(&getDevice().getDevice(), surfaceState, false, false,
                                    pipe->getSize(), pipe->getCpuAddress(), 0,
                                    graphicsAllocation, 0, 0,
                                    kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
        }

        return CL_SUCCESS;
    } else {
        return CL_INVALID_MEM_OBJECT;
    }
}

cl_int Kernel::setArgImage(uint32_t argIndex,
                           size_t argSize,
                           const void *argVal) {
    return setArgImageWithMipLevel(argIndex, argSize, argVal, 0u);
}

cl_int Kernel::setArgImageWithMipLevel(uint32_t argIndex,
                                       size_t argSize,
                                       const void *argVal, uint32_t mipLevel) {
    auto retVal = CL_INVALID_ARG_VALUE;
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();

    const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
    const auto &argAsImg = arg.as<ArgDescImage>();

    uint32_t *crossThreadData = reinterpret_cast<uint32_t *>(this->crossThreadData);
    auto clMemObj = *(static_cast<const cl_mem *>(argVal));
    auto pImage = castToObject<Image>(clMemObj);

    if (pImage && argSize == sizeof(cl_mem *)) {
        if (pImage->peekSharingHandler()) {
            usingSharedObjArgs = true;
        }

        DBG_LOG_INPUTS("setArgImage cl_mem", clMemObj);

        storeKernelArg(argIndex, IMAGE_OBJ, clMemObj, argVal, argSize);

        void *surfaceState = nullptr;
        if (isValidOffset(argAsImg.bindless)) {
            surfaceState = patchBindlessSurfaceState(pImage->getGraphicsAllocation(rootDeviceIndex), argAsImg.bindless);
        } else {
            DEBUG_BREAK_IF(isUndefinedOffset(argAsImg.bindful));
            surfaceState = ptrOffset(getSurfaceStateHeap(), argAsImg.bindful);
        }

        // Sets SS structure
        if (arg.getExtendedTypeInfo().isMediaImage) {
            DEBUG_BREAK_IF(!kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme);
            pImage->setMediaImageArg(surfaceState, rootDeviceIndex);
        } else {
            pImage->setImageArg(surfaceState, arg.getExtendedTypeInfo().isMediaBlockImage, mipLevel, rootDeviceIndex,
                                getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics);
        }

        auto &imageDesc = pImage->getImageDesc();
        auto &imageFormat = pImage->getImageFormat();
        auto graphicsAllocation = pImage->getGraphicsAllocation(rootDeviceIndex);

        if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
            imageTransformer->registerImage3d(argIndex);
        }

        patch<uint32_t, cl_uint>(imageDesc.num_samples, crossThreadData, argAsImg.metadataPayload.numSamples);
        patch<uint32_t, cl_uint>(imageDesc.num_mip_levels, crossThreadData, argAsImg.metadataPayload.numMipLevels);
        patch<uint32_t, uint64_t>(imageDesc.image_width, crossThreadData, argAsImg.metadataPayload.imgWidth);
        patch<uint32_t, uint64_t>(imageDesc.image_height, crossThreadData, argAsImg.metadataPayload.imgHeight);
        patch<uint32_t, uint64_t>(imageDesc.image_depth, crossThreadData, argAsImg.metadataPayload.imgDepth);
        patch<uint32_t, uint64_t>(imageDesc.image_array_size, crossThreadData, argAsImg.metadataPayload.arraySize);
        patch<uint32_t, cl_channel_type>(imageFormat.image_channel_data_type, crossThreadData, argAsImg.metadataPayload.channelDataType);
        patch<uint32_t, cl_channel_order>(imageFormat.image_channel_order, crossThreadData, argAsImg.metadataPayload.channelOrder);

        auto pixelSize = pImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
        patch<uint64_t, uint64_t>(graphicsAllocation->getGpuAddress(), crossThreadData, argAsImg.metadataPayload.flatBaseOffset);
        patch<uint32_t, uint64_t>((imageDesc.image_width * pixelSize) - 1, crossThreadData, argAsImg.metadataPayload.flatWidth);
        patch<uint32_t, uint64_t>((imageDesc.image_height * pixelSize) - 1, crossThreadData, argAsImg.metadataPayload.flatHeight);
        patch<uint32_t, uint64_t>(imageDesc.image_row_pitch - 1, crossThreadData, argAsImg.metadataPayload.flatPitch);

        addAllocationToCacheFlushVector(argIndex, graphicsAllocation);
        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgImmediate(uint32_t argIndex,
                               size_t argSize,
                               const void *argVal) {

    auto retVal = CL_INVALID_ARG_VALUE;

    if (argVal) {
        storeKernelArg(argIndex, NONE_OBJ, nullptr, nullptr, argSize);

        [[maybe_unused]] auto crossThreadDataEnd = ptrOffset(crossThreadData, crossThreadDataSize);
        const auto &argAsVal = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescValue>();
        for (const auto &element : argAsVal.elements) {
            DEBUG_BREAK_IF(element.size <= 0);

            auto pDst = ptrOffset(crossThreadData, element.offset);
            auto pSrc = ptrOffset(argVal, element.sourceOffset);

            DEBUG_BREAK_IF(!(ptrOffset(pDst, element.size) <= crossThreadDataEnd));

            if (element.sourceOffset < argSize) {
                size_t maxBytesToCopy = argSize - element.sourceOffset;
                size_t bytesToCopy = std::min(static_cast<size_t>(element.size), maxBytesToCopy);
                memcpy_s(pDst, element.size, pSrc, bytesToCopy);
            }
        }

        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgSampler(uint32_t argIndex,
                             size_t argSize,
                             const void *argVal) {
    auto retVal = CL_INVALID_SAMPLER;

    if (!argVal) {
        return retVal;
    }

    uint32_t *crossThreadData = reinterpret_cast<uint32_t *>(this->crossThreadData);
    auto clSamplerObj = *(static_cast<const cl_sampler *>(argVal));
    auto pSampler = castToObject<Sampler>(clSamplerObj);

    if (pSampler) {
        pSampler->incRefInternal();
    }

    if (kernelArguments.at(argIndex).object) {
        auto oldSampler = castToObject<Sampler>(kernelArguments.at(argIndex).object);
        UNRECOVERABLE_IF(!oldSampler);
        oldSampler->decRefInternal();
    }

    if (pSampler && argSize == sizeof(cl_sampler *)) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
        const auto &argAsSmp = arg.as<ArgDescSampler>();

        storeKernelArg(argIndex, SAMPLER_OBJ, clSamplerObj, argVal, argSize);

        auto dsh = getDynamicStateHeap();
        auto samplerState = ptrOffset(dsh, argAsSmp.bindful);

        pSampler->setArg(const_cast<void *>(samplerState), clDevice.getHardwareInfo());

        patch<uint32_t, uint32_t>(pSampler->getSnapWaValue(), crossThreadData, argAsSmp.metadataPayload.samplerSnapWa);
        patch<uint32_t, uint32_t>(getAddrModeEnum(pSampler->addressingMode), crossThreadData, argAsSmp.metadataPayload.samplerAddressingMode);
        patch<uint32_t, uint32_t>(getNormCoordsEnum(pSampler->normalizedCoordinates), crossThreadData, argAsSmp.metadataPayload.samplerNormalizedCoords);

        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgAccelerator(uint32_t argIndex,
                                 size_t argSize,
                                 const void *argVal) {
    auto retVal = CL_INVALID_ARG_VALUE;

    if (argSize != sizeof(cl_accelerator_intel)) {
        return CL_INVALID_ARG_SIZE;
    }

    if (!argVal) {
        return retVal;
    }

    auto clAcceleratorObj = *(static_cast<const cl_accelerator_intel *>(argVal));
    DBG_LOG_INPUTS("setArgAccelerator cl_mem", clAcceleratorObj);

    const auto pAccelerator = castToObject<IntelAccelerator>(clAcceleratorObj);

    if (pAccelerator) {
        storeKernelArg(argIndex, ACCELERATOR_OBJ, clAcceleratorObj, argVal, argSize);

        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
        const auto &argAsSmp = arg.as<ArgDescSampler>();

        if (argAsSmp.samplerType == iOpenCL::SAMPLER_OBJECT_VME) {

            const auto pVmeAccelerator = castToObjectOrAbort<VmeAccelerator>(pAccelerator);
            auto pDesc = static_cast<const cl_motion_estimation_desc_intel *>(pVmeAccelerator->getDescriptor());
            DEBUG_BREAK_IF(!pDesc);

            if (arg.getExtendedTypeInfo().hasVmeExtendedDescriptor) {
                const auto &explicitArgsExtendedDescriptors = kernelInfo.kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
                UNRECOVERABLE_IF(argIndex >= explicitArgsExtendedDescriptors.size());
                auto vmeDescriptor = static_cast<ArgDescVme *>(explicitArgsExtendedDescriptors[argIndex].get());

                auto pVmeMbBlockTypeDst = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, vmeDescriptor->mbBlockType));
                *pVmeMbBlockTypeDst = pDesc->mb_block_type;

                auto pVmeSubpixelMode = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, vmeDescriptor->subpixelMode));
                *pVmeSubpixelMode = pDesc->subpixel_mode;

                auto pVmeSadAdjustMode = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, vmeDescriptor->sadAdjustMode));
                *pVmeSadAdjustMode = pDesc->sad_adjust_mode;

                auto pVmeSearchPathType = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, vmeDescriptor->searchPathType));
                *pVmeSearchPathType = pDesc->search_path_type;
            }

            retVal = CL_SUCCESS;
        } else if (argAsSmp.samplerType == iOpenCL::SAMPLER_OBJECT_VE) {
            retVal = CL_SUCCESS;
        }
    }

    return retVal;
}

void Kernel::setKernelArgHandler(uint32_t argIndex, KernelArgHandler handler) {
    if (kernelArgHandlers.size() <= argIndex) {
        kernelArgHandlers.resize(argIndex + 1);
    }

    kernelArgHandlers[argIndex] = handler;
}

void Kernel::unsetArg(uint32_t argIndex) {
    if (kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum--;
        kernelArguments[argIndex].isPatched = false;
        if (kernelArguments[argIndex].isStatelessUncacheable) {
            statelessUncacheableArgsCount--;
            kernelArguments[argIndex].isStatelessUncacheable = false;
        }
    }
}

bool Kernel::hasPrintfOutput() const {
    return kernelInfo.kernelDescriptor.kernelAttributes.flags.usesPrintf;
}

void Kernel::resetSharedObjectsPatchAddresses() {
    for (size_t i = 0; i < getKernelArgsNumber(); i++) {
        auto clMem = (cl_mem)kernelArguments[i].object;
        auto memObj = castToObject<MemObj>(clMem);
        if (memObj && memObj->peekSharingHandler()) {
            setArg((uint32_t)i, sizeof(cl_mem), &clMem);
        }
    }
}

void Kernel::provideInitializationHints() {

    Context *context = program->getContextPtr();
    if (context == nullptr || !context->isProvidingPerformanceHints())
        return;

    auto pClDevice = &getDevice();
    if (privateSurfaceSize) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, PRIVATE_MEMORY_USAGE_TOO_HIGH,
                                        kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(),
                                        privateSurfaceSize);
    }
    auto scratchSize = kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] *
                       pClDevice->getSharedDeviceInfo().computeUnitsUsedForScratch * kernelInfo.getMaxSimdSize();
    if (scratchSize > 0) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, REGISTER_PRESSURE_TOO_HIGH,
                                        kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(), scratchSize);
    }
}

bool Kernel::usesSyncBuffer() const {
    return kernelInfo.kernelDescriptor.kernelAttributes.flags.usesSyncBuffer;
}

void Kernel::patchSyncBuffer(GraphicsAllocation *gfxAllocation, size_t bufferOffset) {
    const auto &syncBuffer = kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress;
    auto bufferPatchAddress = ptrOffset(crossThreadData, syncBuffer.stateless);
    patchWithRequiredSize(bufferPatchAddress, syncBuffer.pointerSize,
                          ptrOffset(gfxAllocation->getGpuAddressToPatch(), bufferOffset));

    if (isValidOffset(syncBuffer.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()), syncBuffer.bindful);
        auto addressToPatch = gfxAllocation->getUnderlyingBuffer();
        auto sizeToPatch = gfxAllocation->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&clDevice.getDevice(), surfaceState, false, false, sizeToPatch, addressToPatch, 0, gfxAllocation, 0, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }
}

bool Kernel::isPatched() const {
    return patchedArgumentsNum == kernelInfo.kernelDescriptor.kernelAttributes.numArgsToPatch;
}
cl_int Kernel::checkCorrectImageAccessQualifier(cl_uint argIndex,
                                                size_t argSize,
                                                const void *argValue) const {
    const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
    if (arg.is<ArgDescriptor::ArgTImage>()) {
        cl_mem mem = *(static_cast<const cl_mem *>(argValue));
        MemObj *pMemObj = nullptr;
        withCastToInternal(mem, &pMemObj);
        if (pMemObj) {
            auto accessQualifier = arg.getTraits().accessQualifier;
            cl_mem_flags flags = pMemObj->getFlags();
            if ((accessQualifier == KernelArgMetadata::AccessReadOnly && ((flags | CL_MEM_WRITE_ONLY) == flags)) ||
                (accessQualifier == KernelArgMetadata::AccessWriteOnly && ((flags | CL_MEM_READ_ONLY) == flags))) {
                return CL_INVALID_ARG_VALUE;
            }
        } else {
            return CL_INVALID_ARG_VALUE;
        }
    }
    return CL_SUCCESS;
}

void Kernel::resolveArgs() {
    if (!Kernel::isPatched() || !imageTransformer->hasRegisteredImages3d() || !canTransformImages())
        return;
    bool canTransformImageTo2dArray = true;
    const auto &args = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs;
    for (uint32_t i = 0; i < patchedArgumentsNum; i++) {
        if (args[i].is<ArgDescriptor::ArgTSampler>()) {
            auto sampler = castToObject<Sampler>(kernelArguments.at(i).object);
            if (sampler->isTransformable()) {
                canTransformImageTo2dArray = true;
            } else {
                canTransformImageTo2dArray = false;
                break;
            }
        }
    }

    if (canTransformImageTo2dArray) {
        imageTransformer->transformImagesTo2dArray(kernelInfo, kernelArguments, getSurfaceStateHeap());
    } else if (imageTransformer->didTransform()) {
        imageTransformer->transformImagesTo3d(kernelInfo, kernelArguments, getSurfaceStateHeap());
    }
}

bool Kernel::canTransformImages() const {
    auto renderCoreFamily = clDevice.getHardwareInfo().platform.eRenderCoreFamily;
    return renderCoreFamily >= IGFX_GEN9_CORE && renderCoreFamily <= IGFX_GEN11LP_CORE && !isBuiltIn;
}

std::unique_ptr<KernelObjsForAuxTranslation> Kernel::fillWithKernelObjsForAuxTranslation() {
    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    kernelObjsForAuxTranslation->reserve(getKernelArgsNumber());
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation()->isCompressionEnabled()) {
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer});
                auto &context = this->program->getContext();
                if (context.isProvidingPerformanceHints()) {
                    const auto &argExtMeta = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[i];
                    context.providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, KERNEL_ARGUMENT_AUX_TRANSLATION,
                                                   kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(), i, argExtMeta.argName.c_str());
                }
            }
        }
        if (SVM_ALLOC_OBJ == getKernelArguments().at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto svmAlloc = reinterpret_cast<GraphicsAllocation *>(const_cast<void *>(getKernelArg(i)));
            if (svmAlloc && svmAlloc->isCompressionEnabled()) {
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, svmAlloc});
                auto &context = this->program->getContext();
                if (context.isProvidingPerformanceHints()) {
                    const auto &argExtMeta = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[i];
                    context.providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, KERNEL_ARGUMENT_AUX_TRANSLATION,
                                                   kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(), i, argExtMeta.argName.c_str());
                }
            }
        }
    }
    const auto &hwInfoConfig = *HwInfoConfig::get(getDevice().getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.allowStatelessCompression(getDevice().getHardwareInfo())) {
        for (auto gfxAllocation : kernelUnifiedMemoryGfxAllocations) {
            if (gfxAllocation->isCompressionEnabled()) {
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, gfxAllocation});
                auto &context = this->program->getContext();
                if (context.isProvidingPerformanceHints()) {
                    context.providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, KERNEL_ALLOCATION_AUX_TRANSLATION,
                                                   kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(),
                                                   reinterpret_cast<void *>(gfxAllocation->getGpuAddress()), gfxAllocation->getUnderlyingBufferSize());
                }
            }
        }
        if (getContext().getSVMAllocsManager()) {
            for (auto &allocation : getContext().getSVMAllocsManager()->getSVMAllocs()->allocations) {
                auto gfxAllocation = allocation.second.gpuAllocations.getDefaultGraphicsAllocation();
                if (gfxAllocation->isCompressionEnabled()) {
                    kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, gfxAllocation});
                    auto &context = this->program->getContext();
                    if (context.isProvidingPerformanceHints()) {
                        context.providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, KERNEL_ALLOCATION_AUX_TRANSLATION,
                                                       kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(),
                                                       reinterpret_cast<void *>(gfxAllocation->getGpuAddress()), gfxAllocation->getUnderlyingBufferSize());
                    }
                }
            }
        }
    }
    return kernelObjsForAuxTranslation;
}

bool Kernel::hasDirectStatelessAccessToSharedBuffer() const {
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == AllocationType::SHARED_BUFFER) {
                return true;
            }
        }
    }
    return false;
}

bool Kernel::hasDirectStatelessAccessToHostMemory() const {
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == AllocationType::BUFFER_HOST_MEMORY) {
                return true;
            }
        }
        if (SVM_ALLOC_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto svmAlloc = reinterpret_cast<const GraphicsAllocation *>(getKernelArg(i));
            if (svmAlloc && svmAlloc->getAllocationType() == AllocationType::BUFFER_HOST_MEMORY) {
                return true;
            }
        }
    }
    return false;
}

bool Kernel::hasIndirectStatelessAccessToHostMemory() const {
    if (!kernelInfo.hasIndirectStatelessAccess) {
        return false;
    }

    for (auto gfxAllocation : kernelUnifiedMemoryGfxAllocations) {
        if (gfxAllocation->getAllocationType() == AllocationType::BUFFER_HOST_MEMORY) {
            return true;
        }
    }

    if (unifiedMemoryControls.indirectHostAllocationsAllowed) {
        return getContext().getSVMAllocsManager()->hasHostAllocations();
    }

    return false;
}

void Kernel::getAllocationsForCacheFlush(CacheFlushAllocationsVec &out) const {
    if (false == HwHelper::cacheFlushAfterWalkerSupported(getHardwareInfo())) {
        return;
    }
    for (GraphicsAllocation *alloc : this->kernelArgRequiresCacheFlush) {
        if (nullptr == alloc) {
            continue;
        }

        out.push_back(alloc);
    }

    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    auto global = getProgram()->getGlobalSurface(rootDeviceIndex);
    if (global != nullptr) {
        out.push_back(global);
    }

    if (svmAllocationsRequireCacheFlush) {
        for (GraphicsAllocation *alloc : kernelSvmGfxAllocations) {
            if (allocationForCacheFlush(alloc)) {
                out.push_back(alloc);
            }
        }
    }
}

bool Kernel::allocationForCacheFlush(GraphicsAllocation *argAllocation) const {
    return argAllocation->isFlushL3Required();
}

void Kernel::addAllocationToCacheFlushVector(uint32_t argIndex, GraphicsAllocation *argAllocation) {
    if (argAllocation == nullptr) {
        kernelArgRequiresCacheFlush[argIndex] = nullptr;
    } else {
        if (allocationForCacheFlush(argAllocation)) {
            kernelArgRequiresCacheFlush[argIndex] = argAllocation;
        } else {
            kernelArgRequiresCacheFlush[argIndex] = nullptr;
        }
    }
}

uint64_t Kernel::getKernelStartAddress(const bool localIdsGenerationByRuntime, const bool kernelUsesLocalIds, const bool isCssUsed, const bool returnFullAddress) const {

    uint64_t kernelStartOffset = 0;

    if (kernelInfo.getGraphicsAllocation()) {
        kernelStartOffset = returnFullAddress ? kernelInfo.getGraphicsAllocation()->getGpuAddress()
                                              : kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();
        if (localIdsGenerationByRuntime == false && kernelUsesLocalIds == true) {
            kernelStartOffset += kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        }
    }

    kernelStartOffset += getStartOffset();

    auto &hardwareInfo = getHardwareInfo();
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    if (isCssUsed && hwHelper.isOffsetToSkipSetFFIDGPWARequired(hardwareInfo)) {
        kernelStartOffset += kernelInfo.kernelDescriptor.entryPoints.skipSetFFIDGP;
    }

    return kernelStartOffset;
}
void *Kernel::patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless) {
    auto &hwHelper = HwHelper::get(getDevice().getHardwareInfo().platform.eRenderCoreFamily);
    auto surfaceStateSize = hwHelper.getRenderSurfaceStateSize();
    NEO::BindlessHeapsHelper *bindlessHeapsHelper = getDevice().getDevice().getBindlessHeapsHelper();
    auto ssInHeap = bindlessHeapsHelper->allocateSSInHeap(surfaceStateSize, alloc, NEO::BindlessHeapsHelper::GLOBAL_SSH);
    auto patchLocation = ptrOffset(getCrossThreadData(), bindless);
    auto patchValue = hwHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(ssInHeap.surfaceStateOffset));
    patchWithRequiredSize(patchLocation, sizeof(patchValue), patchValue);
    return ssInHeap.ssPtr;
}

void Kernel::setAdditionalKernelExecInfo(uint32_t additionalKernelExecInfo) {
    this->additionalKernelExecInfo = additionalKernelExecInfo;
}

uint32_t Kernel::getAdditionalKernelExecInfo() const {
    return this->additionalKernelExecInfo;
}

bool Kernel::requiresWaDisableRccRhwoOptimization() const {
    auto &hardwareInfo = getHardwareInfo();
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();

    if (hwHelper.isWaDisableRccRhwoOptimizationRequired() && isUsingSharedObjArgs()) {
        for (auto &arg : getKernelArguments()) {
            auto clMemObj = static_cast<cl_mem>(arg.object);
            auto memObj = castToObject<MemObj>(clMemObj);
            if (memObj && memObj->peekSharingHandler()) {
                auto allocation = memObj->getGraphicsAllocation(rootDeviceIndex);
                for (uint32_t handleId = 0u; handleId < allocation->getNumGmms(); handleId++) {
                    if (allocation->getGmm(handleId)->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

const HardwareInfo &Kernel::getHardwareInfo() const {
    return getDevice().getHardwareInfo();
}

void Kernel::setWorkDim(uint32_t workDim) {
    patchNonPointer<uint32_t, uint32_t>(getCrossThreadDataRef(), getDescriptor().payloadMappings.dispatchTraits.workDim, workDim);
    if (pImplicitArgs) {
        pImplicitArgs->numWorkDim = workDim;
    }
}

void Kernel::setGlobalWorkOffsetValues(uint32_t globalWorkOffsetX, uint32_t globalWorkOffsetY, uint32_t globalWorkOffsetZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.globalWorkOffset,
                       {globalWorkOffsetX, globalWorkOffsetY, globalWorkOffsetZ});
    if (pImplicitArgs) {
        pImplicitArgs->globalOffsetX = globalWorkOffsetX;
        pImplicitArgs->globalOffsetY = globalWorkOffsetY;
        pImplicitArgs->globalOffsetZ = globalWorkOffsetZ;
    }
}

void Kernel::setGlobalWorkSizeValues(uint32_t globalWorkSizeX, uint32_t globalWorkSizeY, uint32_t globalWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.globalWorkSize,
                       {globalWorkSizeX, globalWorkSizeY, globalWorkSizeZ});
    if (pImplicitArgs) {
        pImplicitArgs->globalSizeX = globalWorkSizeX;
        pImplicitArgs->globalSizeY = globalWorkSizeY;
        pImplicitArgs->globalSizeZ = globalWorkSizeZ;
    }
}

void Kernel::setLocalWorkSizeValues(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.localWorkSize,
                       {localWorkSizeX, localWorkSizeY, localWorkSizeZ});
    if (pImplicitArgs) {
        pImplicitArgs->localSizeX = localWorkSizeX;
        pImplicitArgs->localSizeY = localWorkSizeY;
        pImplicitArgs->localSizeZ = localWorkSizeZ;
    }
}

void Kernel::setLocalWorkSize2Values(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.localWorkSize2,
                       {localWorkSizeX, localWorkSizeY, localWorkSizeZ});
}

void Kernel::setEnqueuedLocalWorkSizeValues(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.enqueuedLocalWorkSize,
                       {localWorkSizeX, localWorkSizeY, localWorkSizeZ});
}

void Kernel::setNumWorkGroupsValues(uint32_t numWorkGroupsX, uint32_t numWorkGroupsY, uint32_t numWorkGroupsZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.numWorkGroups,
                       {numWorkGroupsX, numWorkGroupsY, numWorkGroupsZ});
    if (pImplicitArgs) {
        pImplicitArgs->groupCountX = numWorkGroupsX;
        pImplicitArgs->groupCountY = numWorkGroupsY;
        pImplicitArgs->groupCountZ = numWorkGroupsZ;
    }
}

bool Kernel::isLocalWorkSize2Patchable() {
    const auto &localWorkSize2 = getDescriptor().payloadMappings.dispatchTraits.localWorkSize2;
    return isValidOffset(localWorkSize2[0]) && isValidOffset(localWorkSize2[1]) && isValidOffset(localWorkSize2[2]);
}

uint32_t Kernel::getMaxKernelWorkGroupSize() const {
    return maxKernelWorkGroupSize;
}

uint32_t Kernel::getSlmTotalSize() const {
    return slmTotalSize;
}

bool Kernel::areMultipleSubDevicesInContext() const {
    auto context = program->getContextPtr();
    return context ? context->containsMultipleSubDevices(clDevice.getRootDeviceIndex()) : false;
}

void Kernel::reconfigureKernel() {
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    if (kernelDescriptor.kernelAttributes.numGrfRequired == GrfConfig::LargeGrfNumber &&
        kernelDescriptor.kernelAttributes.simdSize != 32) {
        maxKernelWorkGroupSize >>= 1;
    }
    this->containsStatelessWrites = kernelDescriptor.kernelAttributes.flags.usesStatelessWrites;
    this->systolicPipelineSelectMode = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
}

bool Kernel::requiresCacheFlushCommand(const CommandQueue &commandQueue) const {
    if (false == HwHelper::cacheFlushAfterWalkerSupported(commandQueue.getDevice().getHardwareInfo())) {
        return false;
    }

    if (DebugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.get() != -1) {
        return !!DebugManager.flags.EnableCacheFlushAfterWalkerForAllQueues.get();
    }

    bool cmdQueueRequiresCacheFlush = commandQueue.getRequiresCacheFlushAfterWalker();
    if (false == cmdQueueRequiresCacheFlush) {
        return false;
    }
    if (commandQueue.getGpgpuCommandStreamReceiver().isMultiOsContextCapable()) {
        return false;
    }
    bool isMultiDevice = commandQueue.getContext().containsMultipleSubDevices(commandQueue.getDevice().getRootDeviceIndex());
    if (false == isMultiDevice) {
        return false;
    }
    bool isDefaultContext = (commandQueue.getContext().peekContextType() == ContextType::CONTEXT_TYPE_DEFAULT);
    if (true == isDefaultContext) {
        return false;
    }

    if (getProgram()->getGlobalSurface(commandQueue.getDevice().getRootDeviceIndex()) != nullptr) {
        return true;
    }
    if (svmAllocationsRequireCacheFlush) {
        return true;
    }
    size_t args = kernelArgRequiresCacheFlush.size();
    for (size_t i = 0; i < args; i++) {
        if (kernelArgRequiresCacheFlush[i] != nullptr) {
            return true;
        }
    }
    return false;
}

void Kernel::updateAuxTranslationRequired() {
    const auto &hwInfoConfig = *HwInfoConfig::get(getDevice().getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.allowStatelessCompression(getDevice().getHardwareInfo())) {
        if (hasDirectStatelessAccessToHostMemory() ||
            hasIndirectStatelessAccessToHostMemory() ||
            hasDirectStatelessAccessToSharedBuffer()) {
            setAuxTranslationRequired(true);
        }
    }
}

int Kernel::setKernelThreadArbitrationPolicy(uint32_t policy) {
    auto &hwInfo = clDevice.getHardwareInfo();
    auto &hwHelper = NEO::ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto &threadArbitrationPolicy = const_cast<ThreadArbitrationPolicy &>(getDescriptor().kernelAttributes.threadArbitrationPolicy);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        return CL_INVALID_DEVICE;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL ||
               policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    } else {
        threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

bool Kernel::graphicsAllocationTypeUseSystemMemory(AllocationType type) {
    return (type == AllocationType::BUFFER_HOST_MEMORY) ||
           (type == AllocationType::EXTERNAL_HOST_PTR) ||
           (type == AllocationType::SVM_CPU) ||
           (type == AllocationType::SVM_ZERO_COPY);
}

} // namespace NEO
