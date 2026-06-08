/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/kernel/kernel.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/program/kernel_info.h"

#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/kernel/kernel_info_cl.h"

namespace NEO {
namespace LEO {

Kernel::Kernel(std::map<uint32_t, ze_kernel_handle_t> kernelHandles, Program *program)
    : argsSet(static_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandles.begin()->second))->getPrivateState().kernelArgHandlers.size(), false),
      kernelHandles(std::move(kernelHandles)), program(program) {
    program->incRefInternal();
}

Kernel::Kernel(Kernel *sourceKernel) : argsSet(sourceKernel->argsSet), program(sourceKernel->program) {
    for (const auto &[rootDeviceIndex, kernelHandle] : sourceKernel->kernelHandles) {
        this->kernelHandles[rootDeviceIndex] = static_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle))->makeDependentClone().release();
    }
    this->clonedFromKernel = sourceKernel;
    this->clonedFromKernel->incRefInternal();
    program->incRefInternal();
}

Kernel::~Kernel() {
    for (auto &[rootDeviceIndex, kernelHandle] : this->kernelHandles) {
        zeKernelDestroy(kernelHandle);
    }
    if (this->clonedFromKernel) {
        this->clonedFromKernel->decRefInternal();
    }
    program->decRefInternal();
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
    auto gmmHelper = program->getContext()->getClDevice()->getL0Object()->getNEODevice()->getGmmHelper();
    const auto &kernelInfo = *getL0Object()->getImmutableData()->getKernelInfo();

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
        ctxt = program->getContext();
        srcSize = sizeof(ctxt);
        pSrc = &ctxt;
        break;

    case CL_KERNEL_PROGRAM:
        prog = program;
        srcSize = sizeof(prog);
        pSrc = &prog;
        break;

    case CL_KERNEL_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getRefApiCount());
        srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_KERNEL_ATTRIBUTES:
        pSrc = kernelInfo.kernelDescriptor.kernelMetadata.kernelLanguageAttributes.c_str();
        srcSize = kernelInfo.kernelDescriptor.kernelMetadata.kernelLanguageAttributes.length() + 1;
        break;

    case CL_KERNEL_BINARY_PROGRAM_INTEL:
        pSrc = kernelInfo.heapInfo.pKernelHeap;
        srcSize = kernelInfo.heapInfo.kernelHeapSize;
        break;

    case CL_KERNEL_BINARY_GPU_ADDRESS_INTEL:
        nonCannonizedGpuAddress = gmmHelper->decanonize(kernelInfo.getIsaGraphicsAllocation()->getGpuAddress() + kernelInfo.getIsaOffsetInParentAllocation());
        pSrc = &nonCannonizedGpuAddress;
        srcSize = sizeof(nonCannonizedGpuAddress);
        break;

    default:
        return CL_INVALID_VALUE;
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
}

cl_int Kernel::getWorkGroupInfo(cl_device_id device,
                                cl_kernel_work_group_info paramName,
                                size_t inputValueSize,
                                const void *inputValue,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) const {
    cl_int retVal = CL_INVALID_VALUE;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    const auto &kernelInfo = *getL0Object()->getImmutableData()->getKernelInfo();
    const auto maxSimdSize = static_cast<size_t>(kernelInfo.getMaxSimdSize());
    size_t subGroupCountForNdrange = 0u;
    size_t numDimensions = 0u;
    size_t wgs = 1u;
    cl_uint regCount = 0u;
    cl_ulong memorySize = 0u;
    size_t maxWorkGroupSize = 0u;
    size_t preferredWorkGroupSizeMultiple = 0u;

    auto pDevice = NEO::LEO::castToObject<NEO::LEO::ClDevice>(device);
    if ((paramName == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR) ||
        (paramName == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR)) {
        if (!inputValue) {
            return CL_INVALID_VALUE;
        }
        if (inputValueSize % sizeof(size_t) != 0) {
            return CL_INVALID_VALUE;
        }
        if (nullptr == pDevice) {
            return CL_INVALID_DEVICE;
        }
        numDimensions = inputValueSize / sizeof(size_t);
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(pDevice->getDeviceInfo().maxWorkItemDimensions)) {
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
        if (nullptr == pDevice) {
            return CL_INVALID_DEVICE;
        }
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(pDevice->getDeviceInfo().maxWorkItemDimensions)) {
            return CL_INVALID_VALUE;
        }
    }

    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    ze_kernel_max_group_size_properties_ext_t kernelMaxGroupSizeProperties{ZE_STRUCTURE_TYPE_KERNEL_MAX_GROUP_SIZE_EXT_PROPERTIES};
    kernelProperties.pNext = &kernelMaxGroupSizeProperties;
    ze_kernel_preferred_group_size_properties_t kernelPreferredGroupSizeProperties{ZE_STRUCTURE_TYPE_KERNEL_PREFERRED_GROUP_SIZE_PROPERTIES};
    kernelMaxGroupSizeProperties.pNext = &kernelPreferredGroupSizeProperties;
    const auto rootDeviceIndex = pDevice ? pDevice->getRootDeviceIndex() : this->program->getContext()->getDefaultRootDeviceIndex();
    zeKernelGetProperties(this->getL0Handle(rootDeviceIndex), &kernelProperties);

    size_t requiredWorkgroupSize[3] = {kernelProperties.requiredGroupSizeX, kernelProperties.requiredGroupSizeY, kernelProperties.requiredGroupSizeZ};
    std::array<size_t, 3> localSizeForSubGroupCount = {0u, 0u, 0u};
    switch (paramName) {
    case CL_KERNEL_WORK_GROUP_SIZE:
        maxWorkGroupSize = static_cast<size_t>(kernelMaxGroupSizeProperties.maxGroupSize);
        srcSize = sizeof(maxWorkGroupSize);
        pSrc = &maxWorkGroupSize;
        break;

    case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
        srcSize = sizeof(requiredWorkgroupSize);
        pSrc = requiredWorkgroupSize;
        break;

    case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
        preferredWorkGroupSizeMultiple = static_cast<size_t>(kernelPreferredGroupSizeProperties.preferredMultiple);
        srcSize = sizeof(preferredWorkGroupSizeMultiple);
        pSrc = &preferredWorkGroupSizeMultiple;
        break;

    case CL_KERNEL_LOCAL_MEM_SIZE:
        memorySize = kernelProperties.localMemSize;
        srcSize = sizeof(memorySize);
        pSrc = &memorySize;
        break;

    case CL_KERNEL_SPILL_MEM_SIZE_INTEL:
        memorySize = kernelProperties.spillMemSize;
        srcSize = sizeof(memorySize);
        pSrc = &memorySize;
        break;

    case CL_KERNEL_PRIVATE_MEM_SIZE:
        memorySize = kernelProperties.privateMemSize;
        srcSize = sizeof(memorySize);
        pSrc = &memorySize;
        break;

    case CL_KERNEL_MAX_NUM_SUB_GROUPS:
        srcSize = sizeof(kernelProperties.maxNumSubgroups);
        pSrc = &kernelProperties.maxNumSubgroups;
        break;

    case CL_KERNEL_COMPILE_NUM_SUB_GROUPS:
        srcSize = sizeof(kernelProperties.requiredNumSubGroups);
        pSrc = &kernelProperties.requiredNumSubGroups;
        break;

    case CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL:
        srcSize = sizeof(kernelProperties.requiredSubgroupSize);
        pSrc = &kernelProperties.requiredSubgroupSize;
        break;

    case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR:
        srcSize = sizeof(maxSimdSize);
        pSrc = &maxSimdSize;
        break;

    case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR:
        for (size_t i = 0; i < numDimensions; i++) {
            wgs *= reinterpret_cast<const size_t *>(inputValue)[i];
        }
        subGroupCountForNdrange = (wgs / maxSimdSize) + std::min(static_cast<size_t>(1), wgs % maxSimdSize); // add 1 if WGS % maxSimdSize != 0
        srcSize = sizeof(subGroupCountForNdrange);
        pSrc = &subGroupCountForNdrange;
        break;

    case CL_KERNEL_EU_THREAD_COUNT_INTEL:
        srcSize = sizeof(kernelInfo.kernelDescriptor.kernelAttributes.numThreadsRequired);
        pSrc = &kernelInfo.kernelDescriptor.kernelAttributes.numThreadsRequired;
        break;
    case CL_KERNEL_REGISTER_COUNT_INTEL:
        regCount = kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired;
        srcSize = sizeof(regCount);
        pSrc = &regCount;
        break;
    case CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT: {
        auto largestCompiledSIMDSize = static_cast<size_t>(kernelInfo.getMaxSimdSize());
        auto maxRequiredWorkGroupSize = static_cast<size_t>(kernelInfo.getMaxRequiredWorkGroupSize(kernelMaxGroupSizeProperties.maxGroupSize));
        auto subGroupsNum = *reinterpret_cast<const size_t *>(inputValue);
        auto workGroupSize = subGroupsNum * largestCompiledSIMDSize;
        // return workgroup size in first dimension, the rest shall be 1 in positive case
        if (workGroupSize > maxRequiredWorkGroupSize) {
            workGroupSize = 0;
        }
        // If no work group size can accommodate the requested number of subgroups, return 0 in each element of the returned array.
        localSizeForSubGroupCount = {workGroupSize, (workGroupSize > 0) ? 1u : 0u, (workGroupSize > 0) ? 1u : 0u};
        srcSize = sizeof(size_t) * numDimensions;
        pSrc = localSizeForSubGroupCount.data();
    } break;
    default:
        return CL_INVALID_VALUE;
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
    const auto &kernelInfo = *getL0Object()->getImmutableData()->getKernelInfo();
    const auto &args = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs;

    if (argIndex >= args.size()) {
        retVal = CL_INVALID_ARG_INDEX;
        return retVal;
    }

    getL0Object()->populateMetadata();

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

cl_int Kernel::getSuggestedLocalWorkSize(cl_uint workDim, const size_t *globalWorkSize, size_t *suggestedLocalWorkSize) {
    if (nullptr == suggestedLocalWorkSize) {
        return CL_INVALID_VALUE;
    }
    if (nullptr == globalWorkSize) {
        return CL_INVALID_GLOBAL_WORK_SIZE;
    }
    if (0u == workDim || workDim > 3u) {
        return CL_INVALID_WORK_DIMENSION;
    }
    if (this->kernelHandles.empty()) {
        return CL_INVALID_KERNEL;
    }
    auto kernelHandle = this->getL0Handle();

    Vec3<uint32_t> globalSize{1, 1, 1};
    for (auto dim = 0u; dim < workDim; ++dim) {
        if (0u == globalWorkSize[dim] ||
            globalWorkSize[dim] > std::numeric_limits<uint32_t>::max()) {
            return CL_INVALID_GLOBAL_WORK_SIZE;
        }
        globalSize[dim] = static_cast<uint32_t>(globalWorkSize[dim]);
    }

    Vec3<uint32_t> suggestedGroupSize{1, 1, 1};
    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    auto ret = zeKernelGetProperties(kernelHandle, &kernelProperties);
    if (0u != kernelProperties.requiredGroupSizeX) {
        suggestedGroupSize.x = kernelProperties.requiredGroupSizeX;
        suggestedGroupSize.y = kernelProperties.requiredGroupSizeY;
        suggestedGroupSize.z = kernelProperties.requiredGroupSizeZ;
    } else {
        ret = zeKernelSuggestGroupSize(kernelHandle,
                                       globalSize.x, globalSize.y, globalSize.z,
                                       &suggestedGroupSize.x, &suggestedGroupSize.y, &suggestedGroupSize.z);
    }
    if (ZE_RESULT_SUCCESS == ret) {
        for (auto dim = 0u; dim < workDim; ++dim) {
            suggestedLocalWorkSize[dim] = suggestedGroupSize[dim];
        }
    }
    return L0ToClResultMapper(ret);
}

cl_int Kernel::setArgumentValue(uint32_t argIndex, size_t argSize, const void *argValue) {
    cl_int result = CL_SUCCESS;
    for (const auto &[rootDeviceIndex, kernelHandle] : this->kernelHandles) {
        auto ret = L0ToClResultMapper(zeKernelSetArgumentValue(kernelHandle, argIndex, argSize, argValue));
        if (ret != CL_SUCCESS) {
            result = ret;
        }
    }
    return result;
}

cl_int Kernel::setIndirectAccess(cl_kernel_exec_info flag, cl_bool val) {
    cl_int result = CL_SUCCESS;
    if (CL_TRUE == val) {
        for (const auto &[rootDeviceIndex, kernelHandle] : this->kernelHandles) {
            auto ret = L0ToClResultMapper(zeKernelSetIndirectAccess(kernelHandle, NEO::LEO::Kernel::indirectAccessFlagToL0(flag)));
            if (ret != CL_SUCCESS) {
                result = ret;
            }
        }
    }
    return result;
}

cl_int Kernel::setThreadArbitrationPolicy(uint32_t flag) {
    ze_scheduling_hint_exp_desc_t schedulingDesc;
    schedulingDesc.flags = NEO::LEO::Kernel::schedulingHintToL0(flag);
    cl_int result = CL_SUCCESS;
    for (const auto &[rootDeviceIndex, kernelHandle] : this->kernelHandles) {
        auto ret = L0ToClResultMapper(zeKernelSchedulingHintExp(kernelHandle, &schedulingDesc));
        if (ret != CL_SUCCESS) {
            result = ret;
        }
    }
    return result;
}

ze_kernel_indirect_access_flags_t Kernel::indirectAccessFlagToL0(cl_kernel_exec_info clFlag) {
    switch (clFlag) {
    case CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL:
        return ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    case CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL:
        return ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST;
    case CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL:
        return ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

ze_scheduling_hint_exp_flags_t Kernel::schedulingHintToL0(uint32_t arbitrationPolicy) {
    switch (arbitrationPolicy) {
    default:
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL:
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL:
        return ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN;
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL:
        return ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL:
        return ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST;
    }
}

cl_int Kernel::getMaxConcurrentWorkGroupCount(cl_uint workDim, const size_t *localWorkSize, size_t *suggestedWorkGroupCount) {
    if (nullptr == suggestedWorkGroupCount) {
        return CL_INVALID_VALUE;
    }
    if (0u == workDim || workDim > 3u) {
        return CL_INVALID_WORK_DIMENSION;
    }
    if (nullptr == localWorkSize) {
        return CL_INVALID_WORK_GROUP_SIZE;
    }
    if (this->kernelHandles.empty()) {
        return CL_INVALID_KERNEL;
    }
    for (cl_uint i = 0; i < workDim; i++) {
        if (localWorkSize[i] == 0) {
            return CL_INVALID_WORK_GROUP_SIZE;
        }
    }

    auto *l0Kernel = getL0Object();
    auto &neoDevice = *l0Kernel->getParentModule().getDevice()->getNEODevice();
    auto &helper = neoDevice.getGfxCoreHelper();
    auto &descriptor = l0Kernel->getImmutableData()->getDescriptor();

    auto usedSlmSize = helper.alignSlmSize(l0Kernel->getPrivateState().slmArgsTotalSize + descriptor.kernelAttributes.slmInlineSize,
                                           neoDevice.getRootDeviceEnvironment().getReleaseHelper());

    bool platformImplicitScaling = helper.platformSupportsImplicitScaling(neoDevice.getRootDeviceEnvironment());
    bool isImplicitScalingEnabled = NEO::ImplicitScalingHelper::isImplicitScalingEnabled(neoDevice.getDeviceBitfield(), platformImplicitScaling);

    *suggestedWorkGroupCount = NEO::KernelHelper::getMaxWorkGroupCount(neoDevice,
                                                                       descriptor.kernelAttributes.numGrfRequired,
                                                                       descriptor.kernelAttributes.simdSize,
                                                                       descriptor.kernelAttributes.barrierCount,
                                                                       usedSlmSize,
                                                                       workDim,
                                                                       localWorkSize,
                                                                       NEO::EngineGroupType::compute,
                                                                       isImplicitScalingEnabled,
                                                                       false);

    return CL_SUCCESS;
}
} // namespace LEO
} // namespace NEO
