/*
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_device_side_enqueue.h"
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
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/source/execution_model/device_enqueue.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/image_transformer.h"
#include "opencl/source/kernel/kernel.inl"
#include "opencl/source/kernel/kernel_info_cl.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/sampler/sampler.h"

#include "patch_list.h"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace iOpenCL;

namespace NEO {
class Surface;

uint32_t Kernel::dummyPatchLocation = 0xbaddf00d;

Kernel::Kernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg, bool schedulerKernel)
    : isParentKernel(kernelInfoArg.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue),
      isSchedulerKernel(schedulerKernel),
      executionEnvironment(programArg->getExecutionEnvironment()),
      program(programArg),
      clDevice(clDeviceArg),
      kernelInfo(kernelInfoArg) {
    program->retain();
    program->retainForKernel();
    imageTransformer.reset(new ImageTransformer);
    if (kernelInfoArg.kernelDescriptor.kernelAttributes.simdSize == 1u) {
        auto deviceInfo = getDevice().getDevice().getDeviceInfo();
        auto &hwInfoConfig = *HwInfoConfig::get(getHardwareInfo().platform.eProductFamily);
        maxKernelWorkGroupSize = hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(getHardwareInfo(), static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice));
    } else {
        maxKernelWorkGroupSize = static_cast<uint32_t>(clDevice.getSharedDeviceInfo().maxWorkGroupSize);
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

    if (kernelReflectionSurface) {
        program->peekExecutionEnvironment().memoryManager->freeGraphicsMemory(kernelReflectionSurface);
        kernelReflectionSurface = nullptr;
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

    setThreadArbitrationPolicy(hwHelper.getDefaultThreadArbitrationPolicy());
    if (false == kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress) {
        setThreadArbitrationPolicy(ThreadArbitrationPolicy::AgeBased);
    }
    patchBlocksSimdSize();

    auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);

    auxTranslationRequired = !program->getIsBuiltIn() && HwHelper::renderCompressedBuffersSupported(hwInfo) && clHwHelper.requiresAuxResolves(kernelInfo, hwInfo);

    if (DebugManager.flags.ForceAuxTranslationEnabled.get() != -1) {
        auxTranslationRequired &= !!DebugManager.flags.ForceAuxTranslationEnabled.get();
    }
    if (auxTranslationRequired) {
        program->getContextPtr()->setResolvesRequiredInKernels(true);
    }

    if (isParentKernel) {
        program->allocateBlockPrivateSurfaces(*pClDevice);
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
            } else if (arg.getExtendedTypeInfo().isDeviceQueue) {
                kernelArgHandlers[i] = &Kernel::setArgDevQueue;
                kernelArguments[i].type = DEVICE_QUEUE_OBJ;
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
                 GraphicsAllocation::AllocationType::PRIVATE_SURFACE,
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
                      pSourceKernel->getKernelArgInfo(i).pSvmAlloc, pSourceKernel->getKernelArgInfo(i).svmFlags);
            break;
        case SVM_ALLOC_OBJ:
            setArgSvmAlloc(i, const_cast<void *>(pSourceKernel->getKernelArgInfo(i).value),
                           (GraphicsAllocation *)pSourceKernel->getKernelArgInfo(i).object);
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
        nonCannonizedGpuAddress = GmmHelper::decanonize(kernelInfo.kernelAllocation->getGpuAddress());
        pSrc = &nonCannonizedGpuAddress;
        srcSize = sizeof(nonCannonizedGpuAddress);
        break;
    default:
        getAdditionalInfo(paramName, pSrc, srcSize);
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
        if (hwHelper.isFusedEuDispatchEnabled(hwInfo)) {
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
    default:
        getAdditionalWorkGroupInfo(paramName, pSrc, srcSize);
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
    size_t WGS = 1;
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
            WGS *= ((size_t *)inputValue)[i];
        }
        return changeGetInfoStatusToCLResultType(
            info.set<size_t>((WGS / maxSimdSize) + std::min(static_cast<size_t>(1), WGS % maxSimdSize))); // add 1 if WGS % maxSimdSize != 0
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
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        status = MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *pKernelInfo->getGraphicsAllocation()),
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
    addAllocationToCacheFlushVector(argIndex, svmAlloc);

    return CL_SUCCESS;
}

cl_int Kernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) {
    DBG_LOG_INPUTS("setArgBuffer svm_alloc", svmAlloc);

    const auto &argAsPtr = getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescPointer>();

    auto patchLocation = ptrOffset(getCrossThreadData(), argAsPtr.stateless);
    patchWithRequiredSize(patchLocation, argAsPtr.pointerSize, reinterpret_cast<uintptr_t>(svmPtr));

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
    } else if (svmAlloc && svmAlloc->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED &&
               clHwHelper.requiresNonAuxMode(argAsPtr, hwInfo)) {
        forceNonAuxMode = true;
    }

    bool argWasUncacheable = kernelArguments[argIndex].isStatelessUncacheable;
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
    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
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
    kernelArguments[argIndex].pSvmAlloc = argSvmAlloc;
    kernelArguments[argIndex].svmFlags = argSvmFlags;
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
    auto availableThreadCount = hwHelper.calculateAvailableThreadCount(
        hardwareInfo.platform.eProductFamily,
        kernelDescriptor.kernelAttributes.numGrfRequired,
        hardwareInfo.gtSystemInfo.EUCount, hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount);

    auto barrierCount = kernelDescriptor.kernelAttributes.barrierCount;
    auto maxWorkGroupCount = KernelHelper::getMaxWorkGroupCount(kernelInfo.getMaxSimdSize(),
                                                                availableThreadCount,
                                                                dssCount,
                                                                dssCount * KB * hardwareInfo.capabilityTable.slmSize,
                                                                hwHelper.alignSlmSize(slmTotalSize),
                                                                static_cast<uint32_t>(hwHelper.getMaxBarrierRegisterPerSlice()),
                                                                hwHelper.getBarriersCountFromHasBarriers(barrierCount),
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
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                dst.push_back(new GeneralSurface(pSVMAlloc));
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
        if (!buffer)
            return CL_INVALID_MEM_OBJECT;

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
        } else if (graphicsAllocation->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED &&
                   clHwHelper.requiresNonAuxMode(argAsPtr, hwInfo)) {
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

        //if we make object uncacheable for surface state and there are not stateless accessess , then ther is no need to flush caches
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
        if (arg.getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor) {
            const auto &explicitArgsExtendedDescriptors = kernelInfo.kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
            UNRECOVERABLE_IF(argIndex >= explicitArgsExtendedDescriptors.size());
            auto deviceSideEnqueueDescriptor = static_cast<ArgDescriptorDeviceSideEnqueue *>(explicitArgsExtendedDescriptors[argIndex].get());
            patch<uint32_t, uint32_t>(argAsImg.bindful, crossThreadData, deviceSideEnqueueDescriptor->objectId);
        }

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
        patch<uint32_t, uint32_t>(GetAddrModeEnum(pSampler->addressingMode), crossThreadData, argAsSmp.metadataPayload.samplerAddressingMode);
        patch<uint32_t, uint32_t>(GetNormCoordsEnum(pSampler->normalizedCoordinates), crossThreadData, argAsSmp.metadataPayload.samplerNormalizedCoords);
        if (arg.getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor) {
            const auto &explicitArgsExtendedDescriptors = kernelInfo.kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
            UNRECOVERABLE_IF(argIndex >= explicitArgsExtendedDescriptors.size());
            auto deviceSideEnqueueDescriptor = static_cast<ArgDescriptorDeviceSideEnqueue *>(explicitArgsExtendedDescriptors[argIndex].get());
            patch<uint32_t, uint32_t>(SAMPLER_OBJECT_ID_SHIFT + argAsSmp.bindful, crossThreadData, deviceSideEnqueueDescriptor->objectId);
        }

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

cl_int Kernel::setArgDevQueue(uint32_t argIndex,
                              size_t argSize,
                              const void *argVal) {
    if (argVal == nullptr) {
        return CL_INVALID_ARG_VALUE;
    }

    if (argSize != sizeof(cl_command_queue)) {
        return CL_INVALID_ARG_SIZE;
    }

    auto clDeviceQueue = *(static_cast<const device_queue *>(argVal));
    auto pDeviceQueue = castToObject<DeviceQueue>(clDeviceQueue);

    if (pDeviceQueue == nullptr) {
        return CL_INVALID_DEVICE_QUEUE;
    }

    storeKernelArg(argIndex, DEVICE_QUEUE_OBJ, clDeviceQueue, argVal, argSize);

    const auto &argAsPtr = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex].as<ArgDescPointer>();
    auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), argAsPtr.stateless);
    patchWithRequiredSize(patchLocation, argAsPtr.pointerSize,
                          static_cast<uintptr_t>(pDeviceQueue->getQueueBuffer()->getGpuAddressToPatch()));

    return CL_SUCCESS;
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

void Kernel::createReflectionSurface() {
    auto pClDevice = &clDevice;
    if (this->isParentKernel && kernelReflectionSurface == nullptr) {
        auto &hwInfo = pClDevice->getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        BlockKernelManager *blockManager = program->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        ObjectCounts objectCount;
        getParentObjectCounts(objectCount);
        uint32_t parentImageCount = objectCount.imageCount;
        uint32_t parentSamplerCount = objectCount.samplerCount;
        size_t maxConstantBufferSize = 0;

        std::vector<IGIL_KernelCurbeParams> *curbeParamsForBlocks = new std::vector<IGIL_KernelCurbeParams>[blockCount];

        uint64_t *tokenMask = new uint64_t[blockCount];
        uint32_t *sshTokenOffsetsFromKernelData = new uint32_t[blockCount];

        size_t kernelReflectionSize = alignUp(sizeof(IGIL_KernelDataHeader) + blockCount * sizeof(IGIL_KernelAddressData), sizeof(void *));
        uint32_t kernelDataOffset = static_cast<uint32_t>(kernelReflectionSize);
        uint32_t parentSSHAlignedSize = alignUp(this->kernelInfo.heapInfo.SurfaceStateHeapSize, hwHelper.getBindingTableStateAlignement());
        uint32_t btOffset = parentSSHAlignedSize;

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            size_t samplerStateAndBorderColorSize = 0;

            uint32_t firstSSHTokenIndex = 0;

            ReflectionSurfaceHelper::getCurbeParams(curbeParamsForBlocks[i], tokenMask[i], firstSSHTokenIndex, *pBlockInfo, hwInfo);

            maxConstantBufferSize = std::max(maxConstantBufferSize, static_cast<size_t>(pBlockInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize));

            samplerStateAndBorderColorSize = pBlockInfo->getSamplerStateArraySize(hwInfo);
            samplerStateAndBorderColorSize = alignUp(samplerStateAndBorderColorSize, Sampler::samplerStateArrayAlignment);
            samplerStateAndBorderColorSize += pBlockInfo->getBorderColorStateSize();
            samplerStateAndBorderColorSize = alignUp(samplerStateAndBorderColorSize, sizeof(void *));

            sshTokenOffsetsFromKernelData[i] = offsetof(IGIL_KernelData, m_data) + sizeof(IGIL_KernelCurbeParams) * firstSSHTokenIndex;

            kernelReflectionSize += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParamsForBlocks[i].size(), sizeof(void *));
            kernelReflectionSize += parentSamplerCount * sizeof(IGIL_SamplerParams) + samplerStateAndBorderColorSize;
        }

        maxConstantBufferSize = alignUp(maxConstantBufferSize, sizeof(void *));
        kernelReflectionSize += blockCount * alignUp(maxConstantBufferSize, sizeof(void *));
        kernelReflectionSize += parentImageCount * sizeof(IGIL_ImageParamters);
        kernelReflectionSize += parentSamplerCount * sizeof(IGIL_ParentSamplerParams);
        kernelReflectionSurface = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(
            {pClDevice->getRootDeviceIndex(), kernelReflectionSize,
             GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER,
             pClDevice->getDeviceBitfield()});

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            uint32_t newKernelDataOffset = ReflectionSurfaceHelper::setKernelData(kernelReflectionSurface->getUnderlyingBuffer(),
                                                                                  kernelDataOffset,
                                                                                  curbeParamsForBlocks[i],
                                                                                  tokenMask[i],
                                                                                  maxConstantBufferSize,
                                                                                  parentSamplerCount,
                                                                                  *pBlockInfo,
                                                                                  hwInfo);

            uint32_t offset = static_cast<uint32_t>(offsetof(IGIL_KernelDataHeader, m_data) + sizeof(IGIL_KernelAddressData) * i);

            uint32_t samplerHeapOffset = static_cast<uint32_t>(alignUp(kernelDataOffset + sizeof(IGIL_KernelData) + curbeParamsForBlocks[i].size() * sizeof(IGIL_KernelCurbeParams), sizeof(void *)));
            uint32_t samplerHeapSize = static_cast<uint32_t>(alignUp(pBlockInfo->getSamplerStateArraySize(hwInfo), Sampler::samplerStateArrayAlignment) + pBlockInfo->getBorderColorStateSize());
            uint32_t constantBufferOffset = alignUp(samplerHeapOffset + samplerHeapSize, sizeof(void *));

            uint32_t samplerParamsOffset = 0;
            if (parentSamplerCount) {
                samplerParamsOffset = newKernelDataOffset - sizeof(IGIL_SamplerParams) * parentSamplerCount;
                IGIL_SamplerParams *pSamplerParams = (IGIL_SamplerParams *)ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), samplerParamsOffset);
                uint32_t sampler = 0;
                const auto &args = pBlockInfo->kernelDescriptor.payloadMappings.explicitArgs;
                for (uint32_t argID = 0; argID < args.size(); argID++) {
                    if (args[argID].is<ArgDescriptor::ArgTSampler>()) {

                        pSamplerParams[sampler].m_ArgID = argID;
                        pSamplerParams[sampler].m_SamplerStateOffset = args[argID].as<ArgDescSampler>().bindful;
                        sampler++;
                    }
                }
            }

            ReflectionSurfaceHelper::setKernelAddressData(kernelReflectionSurface->getUnderlyingBuffer(),
                                                          offset,
                                                          kernelDataOffset,
                                                          samplerHeapOffset,
                                                          constantBufferOffset,
                                                          samplerParamsOffset,
                                                          sshTokenOffsetsFromKernelData[i] + kernelDataOffset,
                                                          btOffset,
                                                          *pBlockInfo,
                                                          hwInfo);

            if (samplerHeapSize > 0) {
                void *pDst = ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), samplerHeapOffset);
                const void *pSrc = ptrOffset(pBlockInfo->heapInfo.pDsh, pBlockInfo->getBorderColorOffset());
                memcpy_s(pDst, samplerHeapSize, pSrc, samplerHeapSize);
            }

            void *pDst = ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), constantBufferOffset);
            const char *pSrc = pBlockInfo->crossThreadData;
            memcpy_s(pDst, pBlockInfo->getConstantBufferSize(), pSrc, pBlockInfo->getConstantBufferSize());

            btOffset += pBlockInfo->kernelDescriptor.payloadMappings.bindingTable.tableOffset;
            kernelDataOffset = newKernelDataOffset;
        }

        uint32_t samplerOffset = 0;
        if (parentSamplerCount) {
            samplerOffset = kernelDataOffset + parentImageCount * sizeof(IGIL_ImageParamters);
        }
        ReflectionSurfaceHelper::setKernelDataHeader(kernelReflectionSurface->getUnderlyingBuffer(), blockCount, parentImageCount, parentSamplerCount, kernelDataOffset, samplerOffset);
        delete[] curbeParamsForBlocks;
        delete[] tokenMask;
        delete[] sshTokenOffsetsFromKernelData;

        // Patch constant values once after reflection surface creation
        patchBlocksCurbeWithConstantValues();
    }

    if (DebugManager.flags.ForceDispatchScheduler.get()) {
        if (this->isSchedulerKernel && kernelReflectionSurface == nullptr) {
            kernelReflectionSurface = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(
                {pClDevice->getRootDeviceIndex(), MemoryConstants::pageSize,
                 GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER,
                 pClDevice->getDeviceBitfield()});
        }
    }
}

void Kernel::getParentObjectCounts(ObjectCounts &objectCount) {
    objectCount.imageCount = 0;
    objectCount.samplerCount = 0;
    DEBUG_BREAK_IF(!isParentKernel);

    for (const auto &arg : this->kernelArguments) {
        if (arg.type == SAMPLER_OBJ) {
            objectCount.samplerCount++;
        } else if (arg.type == IMAGE_OBJ) {
            objectCount.imageCount++;
        }
    }
}

bool Kernel::hasPrintfOutput() const {
    return kernelInfo.kernelDescriptor.kernelAttributes.flags.usesPrintf;
}

size_t Kernel::getInstructionHeapSizeForExecutionModel() const {
    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    size_t totalSize = 0;
    if (isParentKernel) {
        totalSize = kernelBinaryAlignment - 1; // for initial alignment
        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            totalSize += pBlockInfo->heapInfo.KernelHeapSize;
            totalSize = alignUp(totalSize, kernelBinaryAlignment);
        }
    }
    return totalSize;
}

void Kernel::patchBlocksCurbeWithConstantValues() {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    uint64_t globalMemoryGpuAddress = program->getGlobalSurface(rootDeviceIndex) != nullptr ? program->getGlobalSurface(rootDeviceIndex)->getGpuAddressToPatch() : 0;
    uint64_t constantMemoryGpuAddress = program->getConstantSurface(rootDeviceIndex) != nullptr ? program->getConstantSurface(rootDeviceIndex)->getGpuAddressToPatch() : 0;

    for (uint32_t blockID = 0; blockID < blockCount; blockID++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(blockID);

        uint64_t globalMemoryCurbeOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t globalMemoryPatchSize = 0;
        uint64_t constantMemoryCurbeOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t constantMemoryPatchSize = 0;

        if (isValidOffset(pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless)) {
            globalMemoryCurbeOffset = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless;
            globalMemoryPatchSize = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize;
        }

        if (isValidOffset(pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless)) {
            constantMemoryCurbeOffset = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless;
            constantMemoryPatchSize = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize;
        }

        ReflectionSurfaceHelper::patchBlocksCurbeWithConstantValues(kernelReflectionSurface->getUnderlyingBuffer(), blockID,
                                                                    globalMemoryCurbeOffset, globalMemoryPatchSize, globalMemoryGpuAddress,
                                                                    constantMemoryCurbeOffset, constantMemoryPatchSize, constantMemoryGpuAddress,
                                                                    ReflectionSurfaceHelper::undefinedOffset, 0, 0);
    }
}

void Kernel::ReflectionSurfaceHelper::getCurbeParams(std::vector<IGIL_KernelCurbeParams> &curbeParamsOut, uint64_t &tokenMaskOut, uint32_t &firstSSHTokenIndex, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {
    const auto &args = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs;
    const auto gpuPointerSize = kernelInfo.kernelDescriptor.kernelAttributes.gpuPointerSize;
    uint32_t bindingTableIndex = 253;
    uint64_t tokenMask = 0;

    for (size_t argNum = 0; argNum < args.size(); argNum++) {
        const auto &arg = args[argNum];

        auto sizeOfKernelArgForSSH = gpuPointerSize;
        bindingTableIndex = 253;

        if (arg.is<ArgDescriptor::ArgTPointer>()) {
            const auto &argAsPtr = arg.as<ArgDescPointer>();

            if (argAsPtr.requiredSlmAlignment) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES, 0, argAsPtr.slmOffset, argAsPtr.requiredSlmAlignment});
                tokenMask |= shiftLeftBy(DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES);
            } else {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{COMPILER_DATA_PARAMETER_GLOBAL_SURFACE, gpuPointerSize, argAsPtr.stateless, static_cast<uint>(argNum)});
                tokenMask |= shiftLeftBy(63);
            }
        } else if (arg.is<ArgDescriptor::ArgTImage>()) {
            const auto &argAsImg = arg.as<ArgDescImage>();

            auto emplaceIfValidOffset = [&](uint parameterType, NEO::CrossThreadDataOffset offset) {
                if (isValidOffset(offset)) {
                    curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{parameterType + 50, sizeof(uint32_t), offset, static_cast<uint>(argNum)});
                }
            };
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_WIDTH, argAsImg.metadataPayload.imgWidth);
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_HEIGHT, argAsImg.metadataPayload.imgHeight);
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_DEPTH, argAsImg.metadataPayload.imgDepth);
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE, argAsImg.metadataPayload.channelDataType);
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_CHANNEL_ORDER, argAsImg.metadataPayload.channelOrder);
            emplaceIfValidOffset(DATA_PARAMETER_IMAGE_ARRAY_SIZE, argAsImg.metadataPayload.arraySize);
            if (arg.getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor) {
                const auto &argsExtDescriptors = kernelInfo.kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
                UNRECOVERABLE_IF(argNum >= argsExtDescriptors.size());
                const auto &deviceSideEnqueueDescriptor = static_cast<ArgDescriptorDeviceSideEnqueue *>(argsExtDescriptors[argNum].get());
                emplaceIfValidOffset(DATA_PARAMETER_OBJECT_ID, deviceSideEnqueueDescriptor->objectId);
            }

            const auto &bindingTable = kernelInfo.kernelDescriptor.payloadMappings.bindingTable;
            if (isValidOffset(bindingTable.tableOffset)) {
                auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
                const auto ssh = static_cast<const char *>(kernelInfo.heapInfo.pSsh) + bindingTable.tableOffset;

                for (uint8_t i = 0; i < bindingTable.numEntries; i++) {
                    const auto pointer = static_cast<NEO::SurfaceStateHeapOffset>(hwHelper.getBindingTableStateSurfaceStatePointer(ssh, i));
                    if (pointer == argAsImg.bindful) {
                        bindingTableIndex = i;
                        break;
                    }
                }
                DEBUG_BREAK_IF(bindingTableIndex == 253);
            }

            tokenMask |= shiftLeftBy(50);
        } else if (arg.is<ArgDescriptor::ArgTSampler>()) {
            const auto &argAsSmp = arg.as<ArgDescSampler>();

            auto emplaceIfValidOffset = [&](uint parameterType, NEO::CrossThreadDataOffset offset) {
                if (isValidOffset(offset)) {
                    curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{parameterType + 100, sizeof(uint32_t), offset, static_cast<uint>(argNum)});
                }
            };
            emplaceIfValidOffset(DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED, argAsSmp.metadataPayload.samplerSnapWa);
            emplaceIfValidOffset(DATA_PARAMETER_SAMPLER_ADDRESS_MODE, argAsSmp.metadataPayload.samplerAddressingMode);
            emplaceIfValidOffset(DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS, argAsSmp.metadataPayload.samplerNormalizedCoords);
            if (arg.getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor) {
                const auto &argsExtDescriptors = kernelInfo.kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
                UNRECOVERABLE_IF(argNum >= argsExtDescriptors.size());
                const auto &deviceSideEnqueueDescriptor = static_cast<ArgDescriptorDeviceSideEnqueue *>(argsExtDescriptors[argNum].get());
                emplaceIfValidOffset(DATA_PARAMETER_OBJECT_ID, deviceSideEnqueueDescriptor->objectId);
            }

            tokenMask |= shiftLeftBy(51);
        } else {
            bindingTableIndex = 0;
            sizeOfKernelArgForSSH = 0;
        }

        curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{1024, sizeOfKernelArgForSSH, bindingTableIndex, static_cast<uint>(argNum)});
    }

    for (const auto &param : kernelInfo.kernelDescriptor.kernelMetadata.allByValueKernelArguments) {
        curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_KERNEL_ARGUMENT, param.byValueElement.size, param.byValueElement.offset, param.argNum});
        tokenMask |= shiftLeftBy(DATA_PARAMETER_KERNEL_ARGUMENT);
    }

    const auto &dispatchTraits = kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits;
    for (uint32_t i = 0; i < 3U; i++) {
        auto emplaceIfValidOffsetAndSetTokenMask = [&](uint parameterType, NEO::CrossThreadDataOffset offset) {
            constexpr uint paramSize = sizeof(uint32_t);
            if (isValidOffset(offset)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{parameterType, paramSize, offset, static_cast<uint>(i * paramSize)});
                tokenMask |= shiftLeftBy(parameterType);
            }
        };
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_LOCAL_WORK_SIZE, dispatchTraits.localWorkSize[i]);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_LOCAL_WORK_SIZE, dispatchTraits.localWorkSize2[i]);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_GLOBAL_WORK_OFFSET, dispatchTraits.globalWorkOffset[i]);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, dispatchTraits.enqueuedLocalWorkSize[i]);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_GLOBAL_WORK_SIZE, dispatchTraits.globalWorkSize[i]);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_NUM_WORK_GROUPS, dispatchTraits.numWorkGroups[i]);
    }
    {
        const auto &payloadMappings = kernelInfo.kernelDescriptor.payloadMappings;
        auto emplaceIfValidOffsetAndSetTokenMask = [&](uint parameterType, NEO::CrossThreadDataOffset offset) {
            if (isValidOffset(offset)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{parameterType, sizeof(uint32_t), offset, 0});
                tokenMask |= shiftLeftBy(parameterType);
            }
        };
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_PARENT_EVENT, payloadMappings.implicitArgs.deviceSideEnqueueParentEvent);
        emplaceIfValidOffsetAndSetTokenMask(DATA_PARAMETER_WORK_DIMENSIONS, payloadMappings.dispatchTraits.workDim);
    }

    std::sort(curbeParamsOut.begin(), curbeParamsOut.end(), compareFunction);
    tokenMaskOut = tokenMask;
    firstSSHTokenIndex = static_cast<uint32_t>(curbeParamsOut.size() - args.size());
}

uint32_t Kernel::ReflectionSurfaceHelper::setKernelData(void *reflectionSurface, uint32_t offset,
                                                        std::vector<IGIL_KernelCurbeParams> &curbeParamsIn, uint64_t tokenMaskIn,
                                                        size_t maxConstantBufferSize, size_t samplerCount, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {
    uint32_t offsetToEnd = 0;
    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(ptrOffset(reflectionSurface, offset));
    size_t samplerHeapSize = alignUp(kernelInfo.getSamplerStateArraySize(hwInfo), Sampler::samplerStateArrayAlignment) + kernelInfo.getBorderColorStateSize();

    kernelData->m_numberOfCurbeParams = static_cast<uint32_t>(curbeParamsIn.size()); // number of paramters to patch
    kernelData->m_numberOfCurbeTokens = static_cast<uint32_t>(curbeParamsIn.size() - kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size());
    kernelData->m_numberOfSamplerStates = static_cast<uint32_t>(kernelInfo.getSamplerStateArrayCount());
    kernelData->m_SizeOfSamplerHeap = static_cast<uint32_t>(samplerHeapSize);
    kernelData->m_SamplerBorderColorStateOffsetOnDSH = isValidOffset(kernelInfo.kernelDescriptor.payloadMappings.samplerTable.borderColor) ? kernelInfo.kernelDescriptor.payloadMappings.samplerTable.borderColor : 0;
    kernelData->m_SamplerStateArrayOffsetOnDSH = isValidOffset(kernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset) ? kernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset : -1;
    kernelData->m_sizeOfConstantBuffer = kernelInfo.getConstantBufferSize();
    kernelData->m_PatchTokensMask = tokenMaskIn;
    kernelData->m_ScratchSpacePatchValue = 0;
    kernelData->m_SIMDSize = kernelInfo.getMaxSimdSize();
    kernelData->m_HasBarriers = kernelInfo.kernelDescriptor.kernelAttributes.barrierCount;
    kernelData->m_RequiredWkgSizes[0] = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
    kernelData->m_RequiredWkgSizes[1] = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
    kernelData->m_RequiredWkgSizes[2] = kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];
    kernelData->m_InilineSLMSize = kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize;

    bool localIdRequired = false;
    if (kernelInfo.kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds || (kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels > 0)) {
        localIdRequired = true;
    }
    kernelData->m_PayloadSize = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, hwInfo.capabilityTable.grfSize);

    kernelData->m_NeedLocalIDS = localIdRequired ? 1 : 0;
    kernelData->m_DisablePreemption = 0u;

    bool concurrentExecAllowed = true;

    if (kernelInfo.kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize > 0) {
        concurrentExecAllowed = false;
    }
    kernelData->m_CanRunConcurently = concurrentExecAllowed ? 1 : 0;

    if (DebugManager.flags.DisableConcurrentBlockExecution.get()) {
        kernelData->m_CanRunConcurently = false;
    }

    IGIL_KernelCurbeParams *kernelCurbeParams = kernelData->m_data;

    for (uint32_t i = 0; i < curbeParamsIn.size(); i++) {
        kernelCurbeParams[i] = curbeParamsIn[i];
    }

    offsetToEnd = static_cast<uint32_t>(offset +
                                        alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParamsIn.size(), sizeof(void *)) +
                                        alignUp(samplerHeapSize, sizeof(void *)) +
                                        alignUp(maxConstantBufferSize, sizeof(void *)) +
                                        sizeof(IGIL_SamplerParams) * samplerCount);

    return offsetToEnd;
}

void Kernel::ReflectionSurfaceHelper::setKernelAddressDataBtOffset(void *reflectionSurface, uint32_t blockID, uint32_t btOffset) {

    uint32_t offset = static_cast<uint32_t>(offsetof(IGIL_KernelDataHeader, m_data) + sizeof(IGIL_KernelAddressData) * blockID);
    IGIL_KernelAddressData *kernelAddressData = reinterpret_cast<IGIL_KernelAddressData *>(ptrOffset(reflectionSurface, offset));

    kernelAddressData->m_BTSoffset = btOffset;
}

void Kernel::ReflectionSurfaceHelper::setKernelAddressData(void *reflectionSurface, uint32_t offset, uint32_t kernelDataOffset, uint32_t samplerHeapOffset,
                                                           uint32_t constantBufferOffset, uint32_t samplerParamsOffset,
                                                           uint32_t sshTokensOffset, uint32_t btOffset, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {
    IGIL_KernelAddressData *kernelAddressData = reinterpret_cast<IGIL_KernelAddressData *>(ptrOffset(reflectionSurface, offset));

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    kernelAddressData->m_KernelDataOffset = kernelDataOffset;
    kernelAddressData->m_SamplerHeapOffset = samplerHeapOffset;
    kernelAddressData->m_SamplerParamsOffset = samplerParamsOffset;
    kernelAddressData->m_ConstantBufferOffset = constantBufferOffset;
    kernelAddressData->m_SSHTokensOffset = sshTokensOffset;
    kernelAddressData->m_BTSoffset = btOffset;
    kernelAddressData->m_BTSize = static_cast<uint32_t>(kernelInfo.kernelDescriptor.payloadMappings.bindingTable.numEntries * hwHelper.getBindingTableStateSize());
}

template <>
void Kernel::ReflectionSurfaceHelper::patchBlocksCurbe<false>(void *reflectionSurface, uint32_t blockID,
                                                              uint64_t defaultDeviceQueueCurbeOffset, uint32_t patchSizeDefaultQueue, uint64_t defaultDeviceQueueGpuAddress,
                                                              uint64_t eventPoolCurbeOffset, uint32_t patchSizeEventPool, uint64_t eventPoolGpuAddress,
                                                              uint64_t deviceQueueCurbeOffset, uint32_t patchSizeDeviceQueue, uint64_t deviceQueueGpuAddress,
                                                              uint64_t printfBufferOffset, uint32_t patchSizePrintfBuffer, uint64_t printfBufferGpuAddress,
                                                              uint64_t privateSurfaceOffset, uint32_t privateSurfaceSize, uint64_t privateSurfaceGpuAddress) {

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);

    // Reflection surface must be initialized prior to patching blocks curbe on KRS
    DEBUG_BREAK_IF(blockID >= pKernelHeader->m_numberOfKernels);

    IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
    // const buffer offsets must be set
    DEBUG_BREAK_IF(addressData[blockID].m_ConstantBufferOffset == 0);

    void *pCurbe = ptrOffset(reflectionSurface, addressData[blockID].m_ConstantBufferOffset);

    if (defaultDeviceQueueCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)defaultDeviceQueueCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeDefaultQueue, (uintptr_t)defaultDeviceQueueGpuAddress);
    }
    if (eventPoolCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)eventPoolCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeEventPool, (uintptr_t)eventPoolGpuAddress);
    }
    if (deviceQueueCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)deviceQueueCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeDeviceQueue, (uintptr_t)deviceQueueGpuAddress);
    }
    if (printfBufferOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)printfBufferOffset);
        patchWithRequiredSize(patchedPointer, patchSizePrintfBuffer, (uintptr_t)printfBufferGpuAddress);
    }

    if (privateSurfaceOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)privateSurfaceOffset);
        patchWithRequiredSize(patchedPointer, privateSurfaceSize, (uintptr_t)privateSurfaceGpuAddress);
    }
}

void Kernel::ReflectionSurfaceHelper::patchBlocksCurbeWithConstantValues(void *reflectionSurface, uint32_t blockID,
                                                                         uint64_t globalMemoryCurbeOffset, uint32_t globalMemoryPatchSize, uint64_t globalMemoryGpuAddress,
                                                                         uint64_t constantMemoryCurbeOffset, uint32_t constantMemoryPatchSize, uint64_t constantMemoryGpuAddress,
                                                                         uint64_t privateMemoryCurbeOffset, uint32_t privateMemoryPatchSize, uint64_t privateMemoryGpuAddress) {

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);

    // Reflection surface must be initialized prior to patching blocks curbe on KRS
    DEBUG_BREAK_IF(blockID >= pKernelHeader->m_numberOfKernels);

    IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
    // const buffer offsets must be set
    DEBUG_BREAK_IF(addressData[blockID].m_ConstantBufferOffset == 0);

    void *pCurbe = ptrOffset(reflectionSurface, addressData[blockID].m_ConstantBufferOffset);

    if (globalMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)globalMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, globalMemoryPatchSize, (uintptr_t)globalMemoryGpuAddress);
    }
    if (constantMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)constantMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, constantMemoryPatchSize, (uintptr_t)constantMemoryGpuAddress);
    }
    if (privateMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)privateMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, privateMemoryPatchSize, (uintptr_t)privateMemoryGpuAddress);
    }
}

void Kernel::ReflectionSurfaceHelper::setParentImageParams(void *reflectionSurface, std::vector<Kernel::SimpleKernelArgInfo> &parentArguments, const KernelInfo &parentKernelInfo) {
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
    IGIL_ImageParamters *pImageParameters = reinterpret_cast<IGIL_ImageParamters *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentImageDataOffset));

    uint32_t numArgs = (uint32_t)parentArguments.size();
    for (uint32_t i = 0; i < numArgs; i++) {
        if (parentArguments[i].type == Kernel::kernelArgType::IMAGE_OBJ) {
            const Image *image = castToObject<Image>((cl_mem)parentArguments[i].object);
            if (image) {
                pImageParameters->m_ArraySize = (uint32_t)image->getImageDesc().image_array_size;
                pImageParameters->m_Depth = (uint32_t)image->getImageDesc().image_depth;
                pImageParameters->m_Height = (uint32_t)image->getImageDesc().image_height;
                pImageParameters->m_Width = (uint32_t)image->getImageDesc().image_width;
                pImageParameters->m_NumMipLevels = (uint32_t)image->getImageDesc().num_mip_levels;
                pImageParameters->m_NumSamples = (uint32_t)image->getImageDesc().num_samples;

                pImageParameters->m_ChannelDataType = (uint32_t)image->getImageFormat().image_channel_data_type;
                pImageParameters->m_ChannelOrder = (uint32_t)image->getImageFormat().image_channel_data_type;
                pImageParameters->m_ObjectID = (uint32_t)parentKernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i].as<ArgDescImage>().bindful;
                pImageParameters++;
            }
        }
    }
}

void Kernel::ReflectionSurfaceHelper::setParentSamplerParams(void *reflectionSurface, std::vector<Kernel::SimpleKernelArgInfo> &parentArguments, const KernelInfo &parentKernelInfo) {
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
    IGIL_ParentSamplerParams *pParentSamplerParams = reinterpret_cast<IGIL_ParentSamplerParams *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentSamplerParamsOffset));

    uint32_t numArgs = (uint32_t)parentArguments.size();
    for (uint32_t i = 0; i < numArgs; i++) {
        if (parentArguments[i].type == Kernel::kernelArgType::SAMPLER_OBJ) {
            const Sampler *sampler = castToObject<Sampler>((cl_sampler)parentArguments[i].object);
            if (sampler) {
                pParentSamplerParams->CoordinateSnapRequired = (uint32_t)sampler->getSnapWaValue();
                pParentSamplerParams->m_AddressingMode = (uint32_t)sampler->addressingMode;
                pParentSamplerParams->NormalizedCoords = (uint32_t)sampler->normalizedCoordinates;

                pParentSamplerParams->m_ObjectID = OCLRT_ARG_OFFSET_TO_SAMPLER_OBJECT_ID((uint32_t)parentKernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i].as<ArgDescSampler>().bindful);
                pParentSamplerParams++;
            }
        }
    }
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

void Kernel::patchDefaultDeviceQueue(DeviceQueue *devQueue) {
    const auto &defaultQueueSurfaceAddress = kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress;
    if (isValidOffset(defaultQueueSurfaceAddress.stateless) && crossThreadData) {
        auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), defaultQueueSurfaceAddress.stateless);
        patchWithRequiredSize(patchLocation, defaultQueueSurfaceAddress.pointerSize,
                              static_cast<uintptr_t>(devQueue->getQueueBuffer()->getGpuAddressToPatch()));
    }
    if (isValidOffset(defaultQueueSurfaceAddress.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()), defaultQueueSurfaceAddress.bindful);
        Buffer::setSurfaceState(&devQueue->getDevice(), surfaceState, false, false, devQueue->getQueueBuffer()->getUnderlyingBufferSize(),
                                (void *)devQueue->getQueueBuffer()->getGpuAddress(), 0, devQueue->getQueueBuffer(), 0, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }
}

void Kernel::patchEventPool(DeviceQueue *devQueue) {
    const auto &eventPoolSurfaceAddress = kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress;

    if (isValidOffset(eventPoolSurfaceAddress.stateless) && crossThreadData) {
        auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(crossThreadData), eventPoolSurfaceAddress.stateless);
        patchWithRequiredSize(patchLocation, eventPoolSurfaceAddress.pointerSize,
                              static_cast<uintptr_t>(devQueue->getEventPoolBuffer()->getGpuAddressToPatch()));
    }

    if (isValidOffset(eventPoolSurfaceAddress.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()), eventPoolSurfaceAddress.bindful);
        auto eventPoolBuffer = devQueue->getEventPoolBuffer();
        Buffer::setSurfaceState(&devQueue->getDevice(), surfaceState, false, false, eventPoolBuffer->getUnderlyingBufferSize(),
                                (void *)eventPoolBuffer->getGpuAddress(), 0, eventPoolBuffer, 0, 0,
                                kernelInfo.kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, areMultipleSubDevicesInContext());
    }
}

void Kernel::patchBlocksSimdSize() {
    BlockKernelManager *blockManager = program->getBlockKernelManager();

    for (auto &idOffset : kernelInfo.childrenKernelsIdOffset) {

        DEBUG_BREAK_IF(!(idOffset.first < static_cast<uint32_t>(blockManager->getCount())));

        const KernelInfo *blockInfo = blockManager->getBlockKernelInfo(idOffset.first);
        uint32_t *simdSize = reinterpret_cast<uint32_t *>(&crossThreadData[idOffset.second]);
        *simdSize = blockInfo->getMaxSimdSize();
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

template void Kernel::patchReflectionSurface<false>(DeviceQueue *, PrintfHandler *);

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
        WithCastToInternal(mem, &pMemObj);
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

void Kernel::fillWithKernelObjsForAuxTranslation(KernelObjsForAuxTranslation &kernelObjsForAuxTranslation) {
    kernelObjsForAuxTranslation.reserve(getKernelArgsNumber());
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) {
                kernelObjsForAuxTranslation.insert({KernelObjForAuxTranslation::Type::MEM_OBJ, buffer});
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
            if (svmAlloc && svmAlloc->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) {
                kernelObjsForAuxTranslation.insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, svmAlloc});
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
            if ((gfxAllocation->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) ||
                (gfxAllocation->getAllocationType() == GraphicsAllocation::AllocationType::SVM_GPU)) {
                kernelObjsForAuxTranslation.insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, gfxAllocation});
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
                if ((gfxAllocation->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) ||
                    (gfxAllocation->getAllocationType() == GraphicsAllocation::AllocationType::SVM_GPU)) {
                    kernelObjsForAuxTranslation.insert({KernelObjForAuxTranslation::Type::GFX_ALLOC, gfxAllocation});
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
}

bool Kernel::hasDirectStatelessAccessToSharedBuffer() const {
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == GraphicsAllocation::AllocationType::SHARED_BUFFER) {
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
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) {
                return true;
            }
        }
        if (SVM_ALLOC_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto svmAlloc = reinterpret_cast<const GraphicsAllocation *>(getKernelArg(i));
            if (svmAlloc && svmAlloc->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) {
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
        if (gfxAllocation->getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) {
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

void Kernel::setReflectionSurfaceBlockBtOffset(uint32_t blockID, uint32_t offset) {
    DEBUG_BREAK_IF(blockID >= program->getBlockKernelManager()->getCount());
    ReflectionSurfaceHelper::setKernelAddressDataBtOffset(getKernelReflectionSurface()->getUnderlyingBuffer(), blockID, offset);
}

bool Kernel::checkIfIsParentKernelAndBlocksUsesPrintf() {
    return isParentKernel && getProgram()->getBlockKernelManager()->getIfBlockUsesPrintf();
}

uint64_t Kernel::getKernelStartOffset(
    const bool localIdsGenerationByRuntime,
    const bool kernelUsesLocalIds,
    const bool isCssUsed) const {

    uint64_t kernelStartOffset = 0;

    if (kernelInfo.getGraphicsAllocation()) {
        kernelStartOffset = kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();
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
    patchNonPointer(getCrossThreadDataRef(), getDescriptor().payloadMappings.dispatchTraits.workDim, workDim);
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
    if (kernelDescriptor.kernelAttributes.numGrfRequired == GrfConfig::LargeGrfNumber) {
        maxKernelWorkGroupSize >>= 1;
    }
    this->containsStatelessWrites = kernelDescriptor.kernelAttributes.flags.usesStatelessWrites;
    this->specialPipelineSelectMode = kernelDescriptor.extendedInfo.get() ? kernelDescriptor.extendedInfo->specialPipelineSelectModeRequired() : false;
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

bool Kernel::requiresLimitedWorkgroupSize() const {
    if (!this->isBuiltIn) {
        return false;
    }
    if (this->auxTranslationDirection != AuxTranslationDirection::None) {
        return false;
    }

    //if source is buffer in local memory, no need for limited workgroup
    if (this->kernelInfo.getArgDescriptorAt(0).is<ArgDescriptor::ArgTPointer>()) {
        if (this->getKernelArgInfo(0).object) {
            auto rootDeviceIndex = getDevice().getRootDeviceIndex();
            auto buffer = castToObject<Buffer>(this->getKernelArgInfo(0u).object);
            if (buffer && buffer->getGraphicsAllocation(rootDeviceIndex)->getMemoryPool() == MemoryPool::LocalMemory) {
                return false;
            }
        }
    }

    //if we are reading from image no need for limited workgroup
    if (this->kernelInfo.getArgDescriptorAt(0).is<ArgDescriptor::ArgTImage>()) {
        return false;
    }

    return true;
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

} // namespace NEO
