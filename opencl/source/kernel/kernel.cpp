/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_vme.h"
#include "shared/source/kernel/local_ids_cache.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/sampler_helpers.h"
#include "opencl/source/kernel/kernel_info_cl.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/source/program/program.h"
#include "opencl/source/sampler/sampler.h"

#include "patch_list.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <vector>

using namespace iOpenCL;

namespace NEO {
class Surface;

uint32_t Kernel::dummyPatchLocation = 0xbaddf00d;

Kernel::Kernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg)
    : executionEnvironment(programArg->getExecutionEnvironment()),
      program(programArg),
      clDevice(clDeviceArg),
      kernelInfo(kernelInfoArg),
      isBuiltIn(programArg->getIsBuiltIn()) {
    program->retain();
    program->retainForKernel();
    auto &deviceInfo = getDevice().getDevice().getDeviceInfo();
    if (isSimd1(kernelInfoArg.kernelDescriptor.kernelAttributes.simdSize)) {
        auto &productHelper = getDevice().getProductHelper();
        maxKernelWorkGroupSize = productHelper.getMaxThreadsForWorkgroupInDSSOrSS(getHardwareInfo(), static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice));
    } else {
        maxKernelWorkGroupSize = static_cast<uint32_t>(deviceInfo.maxWorkGroupSize);
    }
    slmTotalSize = kernelInfoArg.kernelDescriptor.kernelAttributes.slmInlineSize;
    this->implicitArgsVersion = getDevice().getGfxCoreHelper().getImplicitArgsVersion();
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

void Kernel::patchWithImplicitSurface(uint64_t ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const ArgDescPointer &arg) {
    if ((nullptr != crossThreadData) && isValidOffset(arg.stateless)) {
        auto pp = ptrOffset(crossThreadData, arg.stateless);
        patchWithRequiredSize(pp, arg.pointerSize, ptrToPatchInCrossThreadData);
        if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            PatchInfoData patchInfoData(ptrToPatchInCrossThreadData, 0u, PatchInfoAllocationType::kernelArg, reinterpret_cast<uint64_t>(crossThreadData), arg.stateless, PatchInfoAllocationType::indirectObjectHeap, arg.pointerSize);
            this->patchInfoDataList.push_back(patchInfoData);
        }
    }

    void *ssh = getSurfaceStateHeap();
    if (nullptr != ssh) {
        void *addressToPatch = reinterpret_cast<void *>(allocation.getGpuAddressToPatch());
        size_t sizeToPatch = allocation.getUnderlyingBufferSize();

        if (isValidOffset(arg.bindful)) {
            auto surfaceState = ptrOffset(ssh, arg.bindful);
            Buffer::setSurfaceState(&clDevice.getDevice(), surfaceState, false, false, sizeToPatch, addressToPatch, 0, &allocation, 0, 0,
                                    areMultipleSubDevicesInContext());
        } else if (isValidOffset(arg.bindless)) {
            auto &gfxCoreHelper = clDevice.getDevice().getGfxCoreHelper();
            void *surfaceState = nullptr;
            auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

            if (clDevice.getDevice().getBindlessHeapsHelper()) {
                auto &ssInHeap = allocation.getBindlessInfo();
                surfaceState = ssInHeap.ssPtr;
                auto patchLocation = ptrOffset(crossThreadData, arg.bindless);
                auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(ssInHeap.surfaceStateOffset));
                patchWithRequiredSize(reinterpret_cast<uint8_t *>(patchLocation), sizeof(patchValue), patchValue);
            } else {
                auto index = std::numeric_limits<uint32_t>::max();
                const auto &iter = kernelInfo.kernelDescriptor.getBindlessOffsetToSurfaceState().find(arg.bindless);
                if (iter != kernelInfo.kernelDescriptor.getBindlessOffsetToSurfaceState().end()) {
                    index = iter->second;
                }
                if (index < std::numeric_limits<uint32_t>::max()) {
                    surfaceState = ptrOffset(ssh, index * surfaceStateSize);
                }
            }

            if (surfaceState) {
                Buffer::setSurfaceState(&clDevice.getDevice(), surfaceState, false, false, sizeToPatch, addressToPatch, 0, &allocation, 0, 0,
                                        areMultipleSubDevicesInContext());
            }
        }
    }
}

cl_int Kernel::initialize() {
    auto pClDevice = &getDevice();
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    reconfigureKernel();
    auto &hwInfo = pClDevice->getHardwareInfo();
    auto &rootDeviceEnvironment = pClDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    const auto &implicitArgs = kernelDescriptor.payloadMappings.implicitArgs;
    const auto &explicitArgs = kernelDescriptor.payloadMappings.explicitArgs;
    auto maxSimdSize = kernelInfo.getMaxSimdSize();
    const auto &heapInfo = kernelInfo.heapInfo;

    auto localMemSize = static_cast<uint32_t>(clDevice.getDevice().getDeviceInfo().localMemSize);
    auto slmTotalSize = this->getSlmTotalSize();
    if (slmTotalSize > 0 && localMemSize < slmTotalSize) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        return CL_OUT_OF_RESOURCES;
    }

    if (maxSimdSize != 1 && maxSimdSize < gfxCoreHelper.getMinimalSIMDSize()) {
        return CL_INVALID_KERNEL;
    }

    if (kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs) {
        pImplicitArgs = std::make_unique<ImplicitArgs>();
        *pImplicitArgs = {};
        pImplicitArgs->initializeHeader(this->implicitArgsVersion);
        pImplicitArgs->setSimdWidth(maxSimdSize);
    }
    auto ret = KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(kernelDescriptor.kernelAttributes, &pClDevice->getDevice());
    if (ret == NEO::KernelHelper::ErrorCode::invalidKernel) {
        return CL_INVALID_KERNEL;
    }
    if (ret == NEO::KernelHelper::ErrorCode::outOfDeviceMemory) {
        return CL_OUT_OF_RESOURCES;
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
    sshLocalSize = heapInfo.surfaceStateHeapSize;
    if (sshLocalSize) {
        pSshLocal = std::make_unique_for_overwrite<char[]>(sshLocalSize);

        // copy the ssh into our local copy
        memcpy_s(pSshLocal.get(), sshLocalSize,
                 heapInfo.pSsh, heapInfo.surfaceStateHeapSize);
    } else if (NEO::KernelDescriptor::isBindlessAddressingKernel(kernelDescriptor)) {
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());
        sshLocalSize = kernelDescriptor.kernelAttributes.numArgsStateful * surfaceStateSize;
        DEBUG_BREAK_IF(kernelDescriptor.kernelAttributes.numArgsStateful != kernelDescriptor.getBindlessOffsetToSurfaceState().size());
        pSshLocal = std::make_unique<char[]>(sshLocalSize);
    }

    numberOfBindingTableStates = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    localBindingTableOffset = kernelDescriptor.payloadMappings.bindingTable.tableOffset;

    // patch crossthread data and ssh with inline surfaces, if necessary
    auto status = patchPrivateSurface();
    if (CL_SUCCESS != status) {
        return status;
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless) ||
        isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless)) {
        DEBUG_BREAK_IF(program->getConstantSurface(rootDeviceIndex) == nullptr);
        uint64_t constMemory = isBuiltIn ? castToUint64(program->getConstantSurface(rootDeviceIndex)->getUnderlyingBuffer()) : program->getConstantSurface(rootDeviceIndex)->getGpuAddressToPatch();

        const auto &arg = kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress;
        patchWithImplicitSurface(constMemory, *program->getConstantSurface(rootDeviceIndex), arg);
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless) ||
        isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless)) {
        DEBUG_BREAK_IF(program->getGlobalSurface(rootDeviceIndex) == nullptr);
        uint64_t globalMemory = isBuiltIn ? castToUint64(program->getGlobalSurface(rootDeviceIndex)->getUnderlyingBuffer()) : program->getGlobalSurface(rootDeviceIndex)->getGpuAddressToPatch();

        const auto &arg = kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress;
        patchWithImplicitSurface(globalMemory, *program->getGlobalSurface(rootDeviceIndex), arg);
    }

    if (isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                      kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress.bindful);
        Buffer::setSurfaceState(&pClDevice->getDevice(), surfaceState, false, false, 0, nullptr, 0, nullptr, 0, 0, areMultipleSubDevicesInContext());
    }

    auto &threadArbitrationPolicy = const_cast<ThreadArbitrationPolicy &>(kernelInfo.kernelDescriptor.kernelAttributes.threadArbitrationPolicy);
    if (threadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) {
        threadArbitrationPolicy = static_cast<ThreadArbitrationPolicy>(gfxCoreHelper.getDefaultThreadArbitrationPolicy());
    }
    if (kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress == true) {
        threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    }

    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();

    auxTranslationRequired = !program->getIsBuiltIn() && GfxCoreHelper::compressedBuffersSupported(hwInfo) && clGfxCoreHelper.requiresAuxResolves(kernelInfo);

    if (debugManager.flags.ForceAuxTranslationEnabled.get() != -1) {
        auxTranslationRequired &= !!debugManager.flags.ForceAuxTranslationEnabled.get();
    }
    if (auxTranslationRequired) {
        program->getContextPtr()->setResolvesRequiredInKernels(true);
    }

    auto numArgs = explicitArgs.size();
    slmSizes.resize(numArgs);

    this->setInlineSamplers();

    bool detectIndirectAccessInKernel = productHelper.isDetectIndirectAccessInKernelSupported(kernelDescriptor, program->getCreatedFromBinary(), program->getIndirectDetectionVersion());
    if (debugManager.flags.DetectIndirectAccessInKernel.get() != -1) {
        detectIndirectAccessInKernel = debugManager.flags.DetectIndirectAccessInKernel.get() == 1;
    }
    if (detectIndirectAccessInKernel) {
        this->kernelHasIndirectAccess = kernelDescriptor.kernelAttributes.hasNonKernelArgLoad ||
                                        kernelDescriptor.kernelAttributes.hasNonKernelArgStore ||
                                        kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic ||
                                        kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess ||
                                        kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg ||
                                        kernelDescriptor.kernelAttributes.flags.useStackCalls ||
                                        NEO::KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor);
    } else {
        this->kernelHasIndirectAccess = true;
    }
    provideInitializationHints();
    // resolve the new kernel info to account for kernel handlers
    // I think by this time we have decoded the binary and know the number of args etc.
    // double check this assumption
    bool usingBuffers = false;
    kernelArguments.resize(numArgs);
    kernelArgHandlers.resize(numArgs);

    for (uint32_t i = 0; i < numArgs; ++i) {
        storeKernelArg(i, NONE_OBJ, nullptr, nullptr, 0);

        // set the argument handler
        const auto &arg = explicitArgs[i];
        if (arg.is<ArgDescriptor::argTPointer>()) {
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
        } else if (arg.is<ArgDescriptor::argTImage>()) {
            kernelArgHandlers[i] = &Kernel::setArgImage;
            kernelArguments[i].type = IMAGE_OBJ;
            usingImages = true;
        } else if (arg.is<ArgDescriptor::argTSampler>()) {
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

    if (kernelDescriptor.kernelAttributes.numLocalIdChannels > 0) {
        initializeLocalIdsCache();
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

            privateSurface = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(
                {rootDeviceIndex,
                 static_cast<size_t>(privateSurfaceSize),
                 AllocationType::privateSurface,
                 pClDevice->getDeviceBitfield()});
            if (privateSurface == nullptr) {
                return CL_OUT_OF_RESOURCES;
            }
        }

        const auto &privateMemoryAddress = kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress;
        patchWithImplicitSurface(privateSurface->getGpuAddressToPatch(), *privateSurface, privateMemoryAddress);
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
                           reinterpret_cast<GraphicsAllocation *>(pSourceKernel->getKernelArgInfo(i).object),
                           pSourceKernel->getKernelArgInfo(i).allocId);
            break;
        case BUFFER_OBJ:
            setArg(i, pSourceKernel->getKernelArgInfo(i).size, &pSourceKernel->getKernelArgInfo(i).object);
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
        memcpy_s(pImplicitArgs.get(), pImplicitArgs->getSize(), pSourceKernel->getImplicitArgs(), pImplicitArgs->getSize());
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

    program->callPopulateZebinExtendedArgsMetadataOnce(clDevice.getRootDeviceIndex());
    program->callGenerateDefaultExtendedArgsMetadataOnce(clDevice.getRootDeviceIndex());

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
    struct SizeT3 {
        size_t val[3];
    } requiredWorkGroupSize;
    cl_ulong localMemorySize;
    const auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    size_t preferredWorkGroupSizeMultiple = 0;
    cl_ulong scratchSize;
    cl_ulong privateMemSize;
    size_t maxWorkgroupSize;
    cl_uint regCount;
    const auto &hwInfo = clDevice.getHardwareInfo();
    auto &gfxCoreHelper = this->getGfxCoreHelper();
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    switch (paramName) {
    case CL_KERNEL_WORK_GROUP_SIZE:
        maxWorkgroupSize = maxKernelWorkGroupSize;
        if (debugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()) {
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
        localMemorySize = this->getSlmTotalSize();
        srcSize = sizeof(localMemorySize);
        pSrc = &localMemorySize;
        break;

    case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
        preferredWorkGroupSizeMultiple = kernelInfo.getMaxSimdSize();
        if (gfxCoreHelper.isFusedEuDispatchEnabled(hwInfo, kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion)) {
            preferredWorkGroupSizeMultiple *= 2;
        }
        srcSize = sizeof(preferredWorkGroupSizeMultiple);
        pSrc = &preferredWorkGroupSizeMultiple;
        break;

    case CL_KERNEL_SPILL_MEM_SIZE_INTEL:
        scratchSize = kernelDescriptor.kernelAttributes.spillFillScratchMemorySize;
        srcSize = sizeof(scratchSize);
        pSrc = &scratchSize;
        break;
    case CL_KERNEL_PRIVATE_MEM_SIZE:
        privateMemSize = gfxCoreHelper.getKernelPrivateMemSize(kernelDescriptor);
        srcSize = sizeof(privateMemSize);
        pSrc = &privateMemSize;
        break;
    case CL_KERNEL_EU_THREAD_COUNT_INTEL:
        srcSize = sizeof(cl_uint);
        pSrc = &this->getKernelInfo().kernelDescriptor.kernelAttributes.numThreadsRequired;
        break;
    case CL_KERNEL_REGISTER_COUNT_INTEL:
        regCount = kernelDescriptor.kernelAttributes.numGrfRequired;
        srcSize = sizeof(cl_uint);
        pSrc = &regCount;
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
            wgs *= reinterpret_cast<const size_t *>(inputValue)[i];
        }
        return changeGetInfoStatusToCLResultType(
            info.set<size_t>((wgs / maxSimdSize) + std::min(static_cast<size_t>(1), wgs % maxSimdSize))); // add 1 if WGS % maxSimdSize != 0
    }
    case CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT: {
        auto subGroupsNum = *reinterpret_cast<const size_t *>(inputValue);
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
            struct SizeT2 {
                size_t val[2];
            } workGroupSize2;
            workGroupSize2.val[0] = workGroupSize;
            workGroupSize2.val[1] = (workGroupSize > 0) ? 1 : 0;
            return changeGetInfoStatusToCLResultType(info.set<SizeT2>(workGroupSize2));
        default:
            struct SizeT3 {
                size_t val[3];
            } workGroupSize3;
            workGroupSize3.val[0] = workGroupSize;
            workGroupSize3.val[1] = (workGroupSize > 0) ? 1 : 0;
            workGroupSize3.val[2] = (workGroupSize > 0) ? 1 : 0;
            return changeGetInfoStatusToCLResultType(info.set<SizeT3>(workGroupSize3));
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
    return kernelInfo.heapInfo.kernelHeapSize;
}

void Kernel::substituteKernelHeap(void *newKernelHeap, size_t newKernelHeapSize) {
    KernelInfo *pKernelInfo = const_cast<KernelInfo *>(&kernelInfo);
    void **pKernelHeap = const_cast<void **>(&pKernelInfo->heapInfo.pKernelHeap);
    *pKernelHeap = newKernelHeap;
    auto &heapInfo = pKernelInfo->heapInfo;
    heapInfo.kernelHeapSize = static_cast<uint32_t>(newKernelHeapSize);
    pKernelInfo->isKernelHeapSubstituted = true;
    auto memoryManager = executionEnvironment.memoryManager.get();

    auto currentAllocationSize = pKernelInfo->kernelAllocation->getUnderlyingBufferSize();
    bool status = false;
    auto &rootDeviceEnvironment = clDevice.getRootDeviceEnvironment();
    auto &helper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();

    if (currentAllocationSize >= newKernelHeapSize + isaPadding) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        auto useBlitter = productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *pKernelInfo->getGraphicsAllocation());
        status = MemoryTransferHelper::transferMemoryToAllocation(useBlitter,
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

Context &Kernel::getContext() const {
    return program->getContext();
}

void Kernel::setStartOffset(uint32_t offset) {
    this->startOffset = offset;
}

void *Kernel::getSurfaceStateHeap() const {
    return pSshLocal.get();
}

size_t Kernel::getDynamicStateHeapSize() const {
    return kernelInfo.heapInfo.dynamicStateHeapSize;
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
    UNRECOVERABLE_IF(ptrDiff(svmPtr, ptrToPatch) != static_cast<uint32_t>(ptrDiff(svmPtr, ptrToPatch)));
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
                                areMultipleSubDevicesInContext());
    } else if (isValidOffset(argAsPtr.bindless)) {
        auto &gfxCoreHelper = this->getGfxCoreHelper();
        auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

        auto ssIndex = getSurfaceStateIndexForBindlessOffset(argAsPtr.bindless);
        if (ssIndex < std::numeric_limits<uint32_t>::max()) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), ssIndex * surfaceStateSize);
            Buffer::setSurfaceState(&getDevice().getDevice(), surfaceState, false, false, svmAllocSize + ptrDiff(svmPtr, ptrToPatch), ptrToPatch, 0, svmAlloc, svmFlags, 0,
                                    areMultipleSubDevicesInContext());
        }
    }

    storeKernelArg(argIndex, SVM_OBJ, nullptr, svmPtr, sizeof(void *), svmAlloc, svmFlags);
    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
    }
    if (svmPtr != nullptr && isBuiltIn == false) {
        this->anyKernelArgumentUsingSystemMemory |= true;
    }
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
    const bool isAuxTranslationKernel = (AuxTranslationDirection::none != auxTranslationDirection);
    auto &rootDeviceEnvironment = getDevice().getRootDeviceEnvironment();
    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();

    if (isAuxTranslationKernel) {
        if (((AuxTranslationDirection::auxToNonAux == auxTranslationDirection) && argIndex == 1) ||
            ((AuxTranslationDirection::nonAuxToAux == auxTranslationDirection) && argIndex == 0)) {
            forceNonAuxMode = true;
        }
        disableL3 = (argIndex == 0);
    } else if (svmAlloc && svmAlloc->isCompressionEnabled() && clGfxCoreHelper.requiresNonAuxMode(argAsPtr)) {
        forceNonAuxMode = true;
    }

    const bool argWasUncacheable = kernelArgInfo.isStatelessUncacheable;
    const bool argIsUncacheable = svmAlloc ? svmAlloc->isUncacheable() : false;
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
                                areMultipleSubDevicesInContext());
    } else if (isValidOffset(argAsPtr.bindless)) {
        size_t allocSize = 0;
        size_t offset = 0;
        if (svmAlloc != nullptr) {
            allocSize = svmAlloc->getUnderlyingBufferSize();
            offset = ptrDiff(ptrToPatch, svmAlloc->getGpuAddressToPatch());
            allocSize -= offset;
        }

        auto &gfxCoreHelper = this->getGfxCoreHelper();
        auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

        auto ssIndex = getSurfaceStateIndexForBindlessOffset(argAsPtr.bindless);
        if (ssIndex < std::numeric_limits<uint32_t>::max()) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), ssIndex * surfaceStateSize);
            Buffer::setSurfaceState(&getDevice().getDevice(), surfaceState, forceNonAuxMode, disableL3, allocSize, ptrToPatch, offset, svmAlloc, 0, 0,
                                    areMultipleSubDevicesInContext());
        }
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
    return CL_SUCCESS;
}

void Kernel::storeKernelArg(uint32_t argIndex, KernelArgType argType, void *argObject,
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

bool Kernel::getAllowNonUniform() const {
    return program->getAllowNonUniform();
}

void Kernel::setSvmKernelExecInfo(GraphicsAllocation *argValue) {
    kernelSvmGfxAllocations.push_back(argValue);
}

void Kernel::clearSvmKernelExecInfo() {
    kernelSvmGfxAllocations.clear();
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
        this->executionType = KernelExecutionType::defaultType;
        break;
    case CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL:
        this->executionType = KernelExecutionType::concurrent;
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

uint32_t Kernel::getMaxWorkGroupCount(const cl_uint workDim, const size_t *localWorkSize, const CommandQueue *commandQueue, bool forceSingleTileQuery) const {
    auto &hardwareInfo = getHardwareInfo();
    auto &device = this->getDevice();
    auto &helper = device.getGfxCoreHelper();

    auto engineGroupType = helper.getEngineGroupType(commandQueue->getGpgpuEngine().getEngineType(),
                                                     commandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);

    auto usedSlmSize = helper.alignSlmSize(slmTotalSize);

    bool platformImplicitScaling = helper.platformSupportsImplicitScaling(device.getRootDeviceEnvironment());
    bool isImplicitScalingEnabled = ImplicitScalingHelper::isImplicitScalingEnabled(device.getDeviceBitfield(), platformImplicitScaling);

    auto maxWorkGroupCount = KernelHelper::getMaxWorkGroupCount(device.getDevice(),
                                                                kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired,
                                                                kernelInfo.kernelDescriptor.kernelAttributes.simdSize,
                                                                kernelInfo.kernelDescriptor.kernelAttributes.barrierCount,
                                                                usedSlmSize,
                                                                workDim,
                                                                localWorkSize,
                                                                engineGroupType,
                                                                isImplicitScalingEnabled,
                                                                forceSingleTileQuery);

    return maxWorkGroupCount;
}

inline void Kernel::makeArgsResident(CommandStreamReceiver &commandStreamReceiver) {
    auto numArgs = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = reinterpret_cast<GraphicsAllocation *>(kernelArguments[argIndex].object);
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

bool Kernel::isSingleSubdevicePreferred() const {
    auto &gfxCoreHelper = this->getGfxCoreHelper();

    return gfxCoreHelper.singleTileExecImplicitScalingRequired(this->usesSyncBuffer());
}

void Kernel::setInlineSamplers() {
    for (auto &inlineSampler : getDescriptor().inlineSamplers) {
        using AddrMode = NEO::KernelDescriptor::InlineSampler::AddrMode;
        constexpr LookupArray<AddrMode, cl_addressing_mode, 5> addressingModes({{{AddrMode::none, CL_ADDRESS_NONE},
                                                                                 {AddrMode::repeat, CL_ADDRESS_REPEAT},
                                                                                 {AddrMode::clampEdge, CL_ADDRESS_CLAMP_TO_EDGE},
                                                                                 {AddrMode::clampBorder, CL_ADDRESS_CLAMP},
                                                                                 {AddrMode::mirror, CL_ADDRESS_MIRRORED_REPEAT}}});

        using FilterMode = NEO::KernelDescriptor::InlineSampler::FilterMode;
        constexpr LookupArray<FilterMode, cl_filter_mode, 2> filterModes({{{FilterMode::linear, CL_FILTER_LINEAR},
                                                                           {FilterMode::nearest, CL_FILTER_NEAREST}}});

        cl_int errCode = CL_SUCCESS;
        auto sampler = std::unique_ptr<Sampler>(Sampler::create(&getContext(),
                                                                static_cast<cl_bool>(inlineSampler.isNormalized),
                                                                addressingModes.lookUp(inlineSampler.addrMode),
                                                                filterModes.lookUp(inlineSampler.filterMode),
                                                                errCode));
        UNRECOVERABLE_IF(errCode != CL_SUCCESS);

        void *samplerState = nullptr;
        auto dsh = const_cast<void *>(getDynamicStateHeap());

        if (isValidOffset(inlineSampler.bindless)) {
            auto samplerStateIndex = inlineSampler.samplerIndex;
            auto &gfxCoreHelper = this->getGfxCoreHelper();
            auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();
            auto offset = inlineSampler.borderColorStateSize;
            samplerState = ptrOffset(dsh, (samplerStateIndex * samplerStateSize) + offset);
        } else {
            samplerState = ptrOffset(dsh, static_cast<size_t>(inlineSampler.getSamplerBindfulOffset()));
        }
        sampler->setArg(const_cast<void *>(samplerState), clDevice.getRootDeviceEnvironment());
    }
}

void Kernel::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    auto rootDeviceIndex = commandStreamReceiver.getRootDeviceIndex();
    if (privateSurface) {
        commandStreamReceiver.makeResident(*privateSurface);
    }

    if (program->getConstantSurface(rootDeviceIndex)) {
        commandStreamReceiver.makeResident(*(program->getConstantSurface(rootDeviceIndex)));

        auto bindlessHeapAllocation = program->getConstantSurface(rootDeviceIndex)->getBindlessInfo().heapAllocation;
        if (bindlessHeapAllocation) {
            commandStreamReceiver.makeResident(*bindlessHeapAllocation);
        }
    }

    if (program->getGlobalSurface(rootDeviceIndex)) {
        commandStreamReceiver.makeResident(*(program->getGlobalSurface(rootDeviceIndex)));

        auto bindlessHeapAllocation = program->getGlobalSurface(rootDeviceIndex)->getBindlessInfo().heapAllocation;
        if (bindlessHeapAllocation) {
            commandStreamReceiver.makeResident(*bindlessHeapAllocation);
        }
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

    if (getHasIndirectAccess() && unifiedMemoryControls.indirectSharedAllocationsAllowed && pageFaultManager) {
        pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(this->getContext().getSVMAllocsManager());
    }
    makeArgsResident(commandStreamReceiver);

    auto kernelIsaAllocation = this->kernelInfo.kernelAllocation;
    if (kernelIsaAllocation) {
        commandStreamReceiver.makeResident(*kernelIsaAllocation);
    }

    gtpinNotifyMakeResident(this, &commandStreamReceiver);

    if (getHasIndirectAccess() && (unifiedMemoryControls.indirectDeviceAllocationsAllowed ||
                                   unifiedMemoryControls.indirectHostAllocationsAllowed ||
                                   unifiedMemoryControls.indirectSharedAllocationsAllowed)) {
        auto svmAllocsManager = this->getContext().getSVMAllocsManager();
        auto submittedAsPack = svmAllocsManager->submitIndirectAllocationsAsPack(commandStreamReceiver);
        if (!submittedAsPack) {
            svmAllocsManager->makeInternalAllocationsResident(commandStreamReceiver, unifiedMemoryControls.generateMask());
        }
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
                auto pSVMAlloc = reinterpret_cast<GraphicsAllocation *>(kernelArguments[argIndex].object);
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

    slmTotalSize = kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize + alignUp(slmOffset, MemoryConstants::kiloByte);

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
    patch<int64_t, int64_t>(0, crossThreadData, argAsPtr.bufferSize);

    if (clMem && *clMem) {
        auto clMemObj = *clMem;
        DBG_LOG_INPUTS("setArgBuffer cl_mem", clMemObj);

        storeKernelArg(argIndex, BUFFER_OBJ, clMemObj, argVal, argSize);

        auto buffer = castToObject<Buffer>(clMemObj);
        if (!buffer) {
            return CL_INVALID_MEM_OBJECT;
        }

        patch<int64_t, int64_t>(static_cast<int64_t>(buffer->getSize()), getCrossThreadData(), argAsPtr.bufferSize);

        auto gfxAllocationType = buffer->getGraphicsAllocation(rootDeviceIndex)->getAllocationType();
        if (!isBuiltIn) {
            this->anyKernelArgumentUsingSystemMemory |= Kernel::graphicsAllocationTypeUseSystemMemory(gfxAllocationType);
        }

        this->anyKernelArgumentUsingZeroCopyMemory |= buffer->isMemObjZeroCopy();

        if (buffer->peekSharingHandler()) {
            usingSharedObjArgs = true;
        }
        patchBufferOffset(argAsPtr, nullptr, nullptr);

        if (isValidOffset(argAsPtr.stateless)) {
            auto patchLocation = ptrOffset(crossThreadData, argAsPtr.stateless);
            uint64_t addressToPatch = buffer->setArgStateless(patchLocation, argAsPtr.pointerSize, rootDeviceIndex, !this->isBuiltIn);

            if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
                PatchInfoData patchInfoData(addressToPatch - buffer->getOffset(), static_cast<uint64_t>(buffer->getOffset()),
                                            PatchInfoAllocationType::kernelArg, reinterpret_cast<uint64_t>(crossThreadData),
                                            static_cast<uint64_t>(argAsPtr.stateless),
                                            PatchInfoAllocationType::indirectObjectHeap, argAsPtr.pointerSize);
                this->patchInfoDataList.push_back(patchInfoData);
            }
        }

        bool disableL3 = false;
        bool forceNonAuxMode = false;
        bool isAuxTranslationKernel = (AuxTranslationDirection::none != auxTranslationDirection);
        auto graphicsAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
        auto &rootDeviceEnvironment = getDevice().getRootDeviceEnvironment();
        auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();

        if (isAuxTranslationKernel) {
            if (((AuxTranslationDirection::auxToNonAux == auxTranslationDirection) && argIndex == 1) ||
                ((AuxTranslationDirection::nonAuxToAux == auxTranslationDirection) && argIndex == 0)) {
                forceNonAuxMode = true;
            }
            disableL3 = (argIndex == 0);
        } else if (graphicsAllocation->isCompressionEnabled() && clGfxCoreHelper.requiresNonAuxMode(argAsPtr)) {
            forceNonAuxMode = true;
        }

        if (isValidOffset(argAsPtr.bindful)) {
            buffer->setArgStateful(ptrOffset(getSurfaceStateHeap(), argAsPtr.bindful), forceNonAuxMode,
                                   disableL3, isAuxTranslationKernel, arg.isReadOnly(), pClDevice->getDevice(),
                                   areMultipleSubDevicesInContext());
        } else if (isValidOffset(argAsPtr.bindless)) {
            auto &gfxCoreHelper = this->getGfxCoreHelper();
            auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

            auto ssIndex = getSurfaceStateIndexForBindlessOffset(argAsPtr.bindless);
            if (ssIndex < std::numeric_limits<uint32_t>::max()) {
                auto surfaceState = ptrOffset(getSurfaceStateHeap(), ssIndex * surfaceStateSize);
                buffer->setArgStateful(surfaceState, forceNonAuxMode,
                                       disableL3, isAuxTranslationKernel, arg.isReadOnly(), pClDevice->getDevice(),
                                       areMultipleSubDevicesInContext());
            }
        }

        kernelArguments[argIndex].isStatelessUncacheable = argAsPtr.isPureStateful() ? false : buffer->isMemObjUncacheable();

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
                                    areMultipleSubDevicesInContext());
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

    return CL_INVALID_MEM_OBJECT;
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
            auto ssIndex = getSurfaceStateIndexForBindlessOffset(argAsImg.bindless);
            if (ssIndex < std::numeric_limits<uint32_t>::max()) {
                auto &gfxCoreHelper = this->getGfxCoreHelper();
                auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
                surfaceState = ptrOffset(getSurfaceStateHeap(), ssIndex * surfaceStateSize);
            }
        } else {
            DEBUG_BREAK_IF(isUndefinedOffset(argAsImg.bindful));
            surfaceState = ptrOffset(getSurfaceStateHeap(), argAsImg.bindful);
        }

        // Sets SS structure
        UNRECOVERABLE_IF(surfaceState == nullptr);
        if (arg.getExtendedTypeInfo().isMediaImage) {
            DEBUG_BREAK_IF(!kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme);
            pImage->setMediaImageArg(surfaceState, rootDeviceIndex);
        } else {
            pImage->setImageArg(surfaceState, arg.getExtendedTypeInfo().isMediaBlockImage, mipLevel, rootDeviceIndex);
        }

        auto &imageDesc = pImage->getImageDesc();
        auto &imageFormat = pImage->getImageFormat();
        auto graphicsAllocation = pImage->getGraphicsAllocation(rootDeviceIndex);

        patch<uint32_t, cl_uint>(imageDesc.num_samples, crossThreadData, argAsImg.metadataPayload.numSamples);
        patch<uint32_t, cl_uint>(imageDesc.num_mip_levels, crossThreadData, argAsImg.metadataPayload.numMipLevels);
        patch<uint32_t, uint64_t>(imageDesc.image_width, crossThreadData, argAsImg.metadataPayload.imgWidth);
        patch<uint32_t, uint64_t>(imageDesc.image_height, crossThreadData, argAsImg.metadataPayload.imgHeight);
        patch<uint32_t, uint64_t>(imageDesc.image_depth, crossThreadData, argAsImg.metadataPayload.imgDepth);
        patch<uint32_t, uint64_t>(imageDesc.image_array_size, crossThreadData, argAsImg.metadataPayload.arraySize);
        patch<uint32_t, cl_channel_type>(imageFormat.image_channel_data_type, crossThreadData, argAsImg.metadataPayload.channelDataType);
        patch<uint32_t, cl_channel_order>(imageFormat.image_channel_order, crossThreadData, argAsImg.metadataPayload.channelOrder);

        auto pixelSize = pImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
        patch<uint64_t, uint64_t>(graphicsAllocation->getGpuAddress(), crossThreadData, argAsImg.metadataPayload.flatBaseOffset);
        patch<uint32_t, uint64_t>((imageDesc.image_width * pixelSize) - 1, crossThreadData, argAsImg.metadataPayload.flatWidth);
        patch<uint32_t, uint64_t>((imageDesc.image_height * pixelSize) - 1, crossThreadData, argAsImg.metadataPayload.flatHeight);
        patch<uint32_t, uint64_t>(imageDesc.image_row_pitch - 1, crossThreadData, argAsImg.metadataPayload.flatPitch);

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

        void *samplerState = nullptr;
        auto dsh = getDynamicStateHeap();

        if (isValidOffset(argAsSmp.bindless)) {
            auto samplerStateIndex = argAsSmp.index;
            auto &gfxCoreHelper = this->getGfxCoreHelper();
            auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();
            const auto offset = kernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset;
            samplerState = ptrOffset(const_cast<void *>(dsh), (samplerStateIndex * samplerStateSize) + offset);
        } else {
            DEBUG_BREAK_IF(isUndefinedOffset(argAsSmp.bindful));
            samplerState = ptrOffset(const_cast<void *>(dsh), argAsSmp.bindful);
        }

        pSampler->setArg(const_cast<void *>(samplerState), clDevice.getRootDeviceEnvironment());

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
        auto clMem = kernelArguments[i].object;
        auto memObj = castToObject<MemObj>(clMem);
        if (memObj && memObj->peekSharingHandler()) {
            setArg(static_cast<uint32_t>(i), sizeof(cl_mem), &clMem);
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
    auto scratchSize = kernelInfo.kernelDescriptor.kernelAttributes.spillFillScratchMemorySize *
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
                                areMultipleSubDevicesInContext());
    }
}

bool Kernel::isPatched() const {
    return patchedArgumentsNum == kernelInfo.kernelDescriptor.kernelAttributes.numArgsToPatch;
}
cl_int Kernel::checkCorrectImageAccessQualifier(cl_uint argIndex,
                                                size_t argSize,
                                                const void *argValue) const {
    const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
    if (arg.is<ArgDescriptor::argTImage>()) {
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

std::unique_ptr<KernelObjsForAuxTranslation> Kernel::fillWithKernelObjsForAuxTranslation() {
    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    kernelObjsForAuxTranslation->reserve(getKernelArgsNumber());
    for (uint32_t i = 0; i < getKernelArgsNumber(); i++) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[i];
        if (BUFFER_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto buffer = castToObject<Buffer>(getKernelArg(i));
            if (buffer && buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation()->isCompressionEnabled()) {
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::memObj, buffer});
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
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, svmAlloc});
                auto &context = this->program->getContext();
                if (context.isProvidingPerformanceHints()) {
                    const auto &argExtMeta = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[i];
                    context.providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, KERNEL_ARGUMENT_AUX_TRANSLATION,
                                                   kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(), i, argExtMeta.argName.c_str());
                }
            }
        }
    }

    if (CompressionSelector::allowStatelessCompression()) {
        for (auto gfxAllocation : kernelUnifiedMemoryGfxAllocations) {
            if (gfxAllocation->isCompressionEnabled()) {
                kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation});
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
                auto gfxAllocation = allocation.second->gpuAllocations.getDefaultGraphicsAllocation();
                if (gfxAllocation->isCompressionEnabled()) {
                    kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation});
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
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == AllocationType::sharedBuffer) {
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
            if (buffer && buffer->getMultiGraphicsAllocation().getAllocationType() == AllocationType::bufferHostMemory) {
                return true;
            }
        }
        if (SVM_ALLOC_OBJ == kernelArguments.at(i).type && !arg.as<ArgDescPointer>().isPureStateful()) {
            auto svmAlloc = reinterpret_cast<const GraphicsAllocation *>(getKernelArg(i));
            if (svmAlloc && svmAlloc->getAllocationType() == AllocationType::bufferHostMemory) {
                return true;
            }
        }
    }
    return false;
}

bool Kernel::hasIndirectStatelessAccessToHostMemory() const {
    if (!kernelInfo.kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess) {
        return false;
    }

    for (auto gfxAllocation : kernelUnifiedMemoryGfxAllocations) {
        if (gfxAllocation->getAllocationType() == AllocationType::bufferHostMemory) {
            return true;
        }
    }

    if (unifiedMemoryControls.indirectHostAllocationsAllowed) {
        return getContext().getSVMAllocsManager()->hasHostAllocations();
    }

    return false;
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
    const auto &gfxCoreHelper = this->getGfxCoreHelper();
    const auto &productHelper = getDevice().getProductHelper();

    if (isCssUsed && gfxCoreHelper.isOffsetToSkipSetFFIDGPWARequired(hardwareInfo, productHelper)) {
        kernelStartOffset += kernelInfo.kernelDescriptor.entryPoints.skipSetFFIDGP;
    }

    return kernelStartOffset;
}
void *Kernel::patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless) {
    auto &gfxCoreHelper = this->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    NEO::BindlessHeapsHelper *bindlessHeapsHelper = getDevice().getDevice().getBindlessHeapsHelper();
    auto ssInHeap = bindlessHeapsHelper->allocateSSInHeap(surfaceStateSize, alloc, NEO::BindlessHeapsHelper::globalSsh);
    auto patchLocation = ptrOffset(getCrossThreadData(), bindless);
    auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(ssInHeap.surfaceStateOffset));
    patchWithRequiredSize(patchLocation, sizeof(patchValue), patchValue);
    return ssInHeap.ssPtr;
}

uint32_t Kernel::getSurfaceStateIndexForBindlessOffset(NEO::CrossThreadDataOffset bindlessOffset) const {
    const auto &iter = kernelInfo.kernelDescriptor.getBindlessOffsetToSurfaceState().find(bindlessOffset);
    if (iter != kernelInfo.kernelDescriptor.getBindlessOffsetToSurfaceState().end()) {
        return iter->second;
    }
    DEBUG_BREAK_IF(true);
    return std::numeric_limits<uint32_t>::max();
}

template <bool heaplessEnabled>
void Kernel::patchBindlessSurfaceStatesForImplicitArgs(uint64_t bindlessSurfaceStatesBaseAddress) const {
    auto implicitArgsVec = kernelInfo.kernelDescriptor.getImplicitArgBindlessCandidatesVec();

    auto &gfxCoreHelper = this->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    auto *crossThreadDataPtr = reinterpret_cast<uint8_t *>(getCrossThreadData());

    for (size_t i = 0; i < implicitArgsVec.size(); i++) {
        if (NEO::isValidOffset(implicitArgsVec[i]->bindless)) {
            auto patchLocation = ptrOffset(crossThreadDataPtr, implicitArgsVec[i]->bindless);
            auto index = getSurfaceStateIndexForBindlessOffset(implicitArgsVec[i]->bindless);

            if (index < std::numeric_limits<uint32_t>::max()) {

                auto surfaceStateAddress = bindlessSurfaceStatesBaseAddress + (index * surfaceStateSize);

                if constexpr (heaplessEnabled) {
                    uint64_t patchValue = surfaceStateAddress;
                    patchWithRequiredSize(patchLocation, sizeof(uint64_t), patchValue);
                } else {
                    uint32_t patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(surfaceStateAddress));
                    patchWithRequiredSize(patchLocation, sizeof(uint32_t), patchValue);
                }
            }
        }
    }
}

template <bool heaplessEnabled>
void Kernel::patchBindlessSurfaceStatesInCrossThreadData(uint64_t bindlessSurfaceStatesBaseAddress) const {
    auto &gfxCoreHelper = this->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    auto *crossThreadDataPtr = reinterpret_cast<uint8_t *>(getCrossThreadData());

    for (auto &arg : kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) {

        auto offset = NEO::undefined<NEO::CrossThreadDataOffset>;
        if (arg.type == NEO::ArgDescriptor::argTPointer) {
            offset = arg.as<NEO::ArgDescPointer>().bindless;
        } else if (arg.type == NEO::ArgDescriptor::argTImage) {
            offset = arg.as<NEO::ArgDescImage>().bindless;
        } else {
            continue;
        }

        if (NEO::isValidOffset(offset)) {
            auto index = getSurfaceStateIndexForBindlessOffset(offset);

            if (index < std::numeric_limits<uint32_t>::max()) {
                auto patchLocation = ptrOffset(crossThreadDataPtr, offset);
                auto surfaceStateAddress = bindlessSurfaceStatesBaseAddress + (index * surfaceStateSize);

                if constexpr (heaplessEnabled) {
                    uint64_t patchValue = surfaceStateAddress;
                    patchWithRequiredSize(patchLocation, sizeof(uint64_t), patchValue);
                } else {
                    uint32_t patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(surfaceStateAddress));
                    patchWithRequiredSize(patchLocation, sizeof(uint32_t), patchValue);
                }
            }
        }
    }

    const auto bindlessHeapsHelper = getDevice().getDevice().getBindlessHeapsHelper();

    if (!bindlessHeapsHelper) {
        patchBindlessSurfaceStatesForImplicitArgs<heaplessEnabled>(bindlessSurfaceStatesBaseAddress);
    }
}

void Kernel::patchBindlessSamplerStatesInCrossThreadData(uint64_t bindlessSamplerStatesBaseAddress) const {
    auto &gfxCoreHelper = this->getGfxCoreHelper();
    const auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();
    auto *crossThreadDataPtr = reinterpret_cast<uint8_t *>(getCrossThreadData());

    auto samplerArgs = std::ranges::subrange(kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) | std::views::filter([](const auto &arg) {
                           return (arg.type == NEO::ArgDescriptor::argTSampler) && NEO::isValidOffset(arg.template as<NEO::ArgDescSampler>().bindless);
                       });

    for (auto &arg : samplerArgs) {
        auto &sampler = arg.template as<NEO::ArgDescSampler>();
        auto patchLocation = ptrOffset(crossThreadDataPtr, sampler.bindless);
        auto samplerStateAddress = static_cast<uint64_t>(bindlessSamplerStatesBaseAddress + sampler.index * samplerStateSize);
        auto patchValue = samplerStateAddress;
        patchWithRequiredSize(patchLocation, sampler.size, patchValue);
    }

    auto inlineSamplers = kernelInfo.kernelDescriptor.inlineSamplers | std::views::filter([](const auto &sampler) {
                              return (NEO::isValidOffset(sampler.bindless));
                          });

    for (auto &sampler : inlineSamplers) {
        auto patchLocation = ptrOffset(crossThreadDataPtr, sampler.bindless);
        auto samplerStateAddress = static_cast<uint64_t>(bindlessSamplerStatesBaseAddress + sampler.samplerIndex * samplerStateSize);
        auto patchValue = samplerStateAddress;
        patchWithRequiredSize(patchLocation, sampler.size, patchValue);
    }
}

void Kernel::setAdditionalKernelExecInfo(uint32_t additionalKernelExecInfo) {
    this->additionalKernelExecInfo = additionalKernelExecInfo;
}

uint32_t Kernel::getAdditionalKernelExecInfo() const {
    return this->additionalKernelExecInfo;
}

bool Kernel::requiresWaDisableRccRhwoOptimization() const {
    auto &gfxCoreHelper = this->getGfxCoreHelper();
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();

    if (gfxCoreHelper.isWaDisableRccRhwoOptimizationRequired() && isUsingSharedObjArgs()) {
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
        pImplicitArgs->setNumWorkDim(workDim);
    }
}

void Kernel::setGlobalWorkOffsetValues(uint32_t globalWorkOffsetX, uint32_t globalWorkOffsetY, uint32_t globalWorkOffsetZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.globalWorkOffset,
                       {globalWorkOffsetX, globalWorkOffsetY, globalWorkOffsetZ});
    if (pImplicitArgs) {
        pImplicitArgs->setGlobalOffset(globalWorkOffsetX, globalWorkOffsetY, globalWorkOffsetZ);
    }
}

void Kernel::setGlobalWorkSizeValues(uint32_t globalWorkSizeX, uint32_t globalWorkSizeY, uint32_t globalWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.globalWorkSize,
                       {globalWorkSizeX, globalWorkSizeY, globalWorkSizeZ});
    if (pImplicitArgs) {
        pImplicitArgs->setGlobalSize(globalWorkSizeX, globalWorkSizeY, globalWorkSizeZ);
    }
}

void Kernel::setLocalWorkSizeValues(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ) {
    patchVecNonPointer(getCrossThreadDataRef(),
                       getDescriptor().payloadMappings.dispatchTraits.localWorkSize,
                       {localWorkSizeX, localWorkSizeY, localWorkSizeZ});
    if (pImplicitArgs) {
        pImplicitArgs->setLocalSize(localWorkSizeX, localWorkSizeY, localWorkSizeZ);
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
        pImplicitArgs->setGroupCount(numWorkGroupsX, numWorkGroupsY, numWorkGroupsZ);
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
    const auto &kernelDescriptor = kernelInfo.kernelDescriptor;
    const auto &gfxCoreHelper = this->getGfxCoreHelper();
    auto &rootDeviceEnvironment = getDevice().getRootDeviceEnvironment();

    this->maxKernelWorkGroupSize = gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, this->maxKernelWorkGroupSize, rootDeviceEnvironment);

    this->containsStatelessWrites = kernelDescriptor.kernelAttributes.flags.usesStatelessWrites;
    this->systolicPipelineSelectMode = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
}

void Kernel::updateAuxTranslationRequired() {
    if (CompressionSelector::allowStatelessCompression()) {
        if (hasDirectStatelessAccessToHostMemory() ||
            hasIndirectStatelessAccessToHostMemory() ||
            hasDirectStatelessAccessToSharedBuffer()) {
            setAuxTranslationRequired(true);
        }
    }
}

int Kernel::setKernelThreadArbitrationPolicy(uint32_t policy) {
    auto &clGfxCoreHelper = clDevice.getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    auto &threadArbitrationPolicy = const_cast<ThreadArbitrationPolicy &>(getDescriptor().kernelAttributes.threadArbitrationPolicy);
    if (!clGfxCoreHelper.isSupportedKernelThreadArbitrationPolicy()) {
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
    return (type == AllocationType::bufferHostMemory) ||
           (type == AllocationType::externalHostPtr) ||
           (type == AllocationType::svmCpu) ||
           (type == AllocationType::svmZeroCopy);
}

void Kernel::initializeLocalIdsCache() {
    auto workgroupDimensionsOrder = getDescriptor().kernelAttributes.workgroupDimensionsOrder;
    std::array<uint8_t, 3> wgDimOrder = {workgroupDimensionsOrder[0],
                                         workgroupDimensionsOrder[1],
                                         workgroupDimensionsOrder[2]};
    auto simdSize = getDescriptor().kernelAttributes.simdSize;
    auto grfCount = getDescriptor().kernelAttributes.numGrfRequired;
    auto grfSize = static_cast<uint8_t>(getDevice().getHardwareInfo().capabilityTable.grfSize);
    localIdsCache = std::make_unique<LocalIdsCache>(4, wgDimOrder, grfCount, simdSize, grfSize, usingImagesOnly);
}

void Kernel::setLocalIdsForGroup(const Vec3<uint16_t> &groupSize, void *destination) const {
    UNRECOVERABLE_IF(localIdsCache.get() == nullptr);
    localIdsCache->setLocalIdsForGroup(groupSize, destination, clDevice.getRootDeviceEnvironment());
}

size_t Kernel::getLocalIdsSizeForGroup(const Vec3<uint16_t> &groupSize) const {
    UNRECOVERABLE_IF(localIdsCache.get() == nullptr);
    return localIdsCache->getLocalIdsSizeForGroup(groupSize, clDevice.getRootDeviceEnvironment());
}

size_t Kernel::getLocalIdsSizePerThread() const {
    UNRECOVERABLE_IF(localIdsCache.get() == nullptr);
    return localIdsCache->getLocalIdsSizePerThread();
}

template void Kernel::patchBindlessSurfaceStatesForImplicitArgs<false>(uint64_t bindlessSurfaceStatesBaseAddress) const;
template void Kernel::patchBindlessSurfaceStatesForImplicitArgs<true>(uint64_t bindlessSurfaceStatesBaseAddress) const;

template void Kernel::patchBindlessSurfaceStatesInCrossThreadData<false>(uint64_t bindlessSurfaceStatesBaseAddress) const;
template void Kernel::patchBindlessSurfaceStatesInCrossThreadData<true>(uint64_t bindlessSurfaceStatesBaseAddress) const;

} // namespace NEO
