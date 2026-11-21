/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/source/sampler/leo_sampler.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <limits>
#include <map>

namespace NEO {
namespace LEO {

namespace {
cl_int getKernelSuggestedLocalWorkSizeImpl(NEO::LEO::CommandQueue *commandQueue,
                                           NEO::LEO::Kernel *kernel,
                                           cl_uint workDim,
                                           const size_t *globalWorkOffset,
                                           const size_t *globalWorkSize,
                                           size_t *suggestedLocalWorkSize) {
    if (commandQueue->getContext() != kernel->getContext()) {
        return CL_INVALID_CONTEXT;
    }

    if (nullptr != globalWorkOffset) {
        if (0u == workDim || workDim > 3u) {
            return CL_INVALID_WORK_DIMENSION;
        }

        if (nullptr == globalWorkSize) {
            return CL_INVALID_GLOBAL_WORK_SIZE;
        }

        for (auto dim = 0u; dim < workDim; ++dim) {
            if (globalWorkOffset[dim] > std::numeric_limits<size_t>::max() - globalWorkSize[dim]) {
                return CL_INVALID_GLOBAL_OFFSET;
            }
        }
    }

    return kernel->getSuggestedLocalWorkSize(*commandQueue->getDevice(), workDim, globalWorkSize,
                                             suggestedLocalWorkSize);
}
} // namespace

extern "C" {

cl_kernel CL_API_CALL clCreateKernel(cl_program clProgram,
                                     const char *kernelName,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateKernel, &clProgram, &kernelName, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram), std::make_tuple(const_cast<char *>(kernelName)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return tracingScope.exit(nullptr);
    }

    if (pProgram->getModuleHandles().empty()) [[unlikely]] {
        err.set(CL_INVALID_PROGRAM_EXECUTABLE);
        return tracingScope.exit(nullptr);
    }

    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, kernelName};
    std::map<uint32_t, ze_kernel_handle_t> kernelHandles{};

    for (const auto &[rootDeviceIndex, moduleHandle] : pProgram->getModuleHandles()) {
        ze_kernel_handle_t kernelHandle{};
        auto ret = zeKernelCreate(moduleHandle, &kernelDesc, &kernelHandle);
        if (ZE_RESULT_SUCCESS != ret) [[unlikely]] {
            for (auto &[idx, handle] : kernelHandles) {
                zeKernelDestroy(handle);
            }
            err.set(L0ToClResultMapper(ret));
            return tracingScope.exit(nullptr);
        }
        kernelHandles[rootDeviceIndex] = kernelHandle;
    }

    return tracingScope.exit(new NEO::LEO::Kernel(std::move(kernelHandles), pProgram));
}

cl_int CL_API_CALL clCreateKernelsInProgram(cl_program clProgram,
                                            cl_uint numKernels,
                                            cl_kernel *kernels,
                                            cl_uint *numKernelsRet) {
    TRACING_ENTER(ClCreateKernelsInProgram, &clProgram, &numKernels, &kernels, &numKernelsRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    auto kernelNames = pProgram->getUserKernelNames();
    auto numUserKernels = static_cast<cl_uint>(kernelNames.size());

    if (kernels) [[likely]] {
        if (numKernels < numUserKernels) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }

        size_t i = 0u;
        for (auto kernelName : kernelNames) {
            cl_int ret = CL_SUCCESS;
            kernels[i++] = clCreateKernel(clProgram, kernelName, &ret);
            if (ret != CL_SUCCESS) [[unlikely]] {
                return tracingScope.exit(ret);
            }
        }
    }

    if (numKernelsRet) {
        *numKernelsRet = numUserKernels;
    }

    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clRetainKernel(cl_kernel kernel) {
    TRACING_ENTER(ClRetainKernel, &kernel);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    pKernel->incRefApi();
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clReleaseKernel(cl_kernel kernel) {
    TRACING_ENTER(ClReleaseKernel, &kernel);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    pKernel->decRefApi();
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clSetKernelArg(cl_kernel kernel,
                                  cl_uint argIndex,
                                  size_t argSize,
                                  const void *argValue) {
    TRACING_ENTER(ClSetKernelArg, &kernel, &argIndex, &argSize, &argValue);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_ARG_INDEX);
    }
    const auto &argDescriptor = pKernel->getL0Object()->getKernelDescriptor().payloadMappings.explicitArgs[argIndex];
    const bool isImmediateArg = (argDescriptor.type == NEO::ArgDescriptor::argTValue);

    if (isImmediateArg && argValue) {
        cl_int validationRetVal = pKernel->validateImmediateArgSize(argIndex, argSize);
        if (validationRetVal != CL_SUCCESS) [[unlikely]] {
            return tracingScope.exit(validationRetVal);
        }
    }

    if (argDescriptor.type == NEO::ArgDescriptor::argTImage && argDescriptor.as<NEO::ArgDescImage>().isMsaa() && argValue && argSize == sizeof(cl_mem)) {
        auto pImage = NEO::LEO::castToObject<NEO::LEO::Image>(*reinterpret_cast<const cl_mem *>(argValue));
        if (pImage && pImage->getL0Object()->getImageInfo().imgDesc.numSamples <= 1) {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }
    }

    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);
    pKernel->clearSharedObjArg(argIndex);

    if (isImmediateArg) {
        return tracingScope.exit(pKernel->setArgumentValue(argIndex, argSize, argValue));
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTPointer) {
        auto clMem = reinterpret_cast<const cl_mem *>(argValue);
        if (clMem && *clMem) [[likely]] {
            if (argSize != sizeof(cl_mem)) [[unlikely]] {
                return tracingScope.exit(CL_INVALID_ARG_SIZE);
            }

            auto pBuffer = NEO::LEO::castToObject<NEO::LEO::Buffer>(*reinterpret_cast<const cl_mem *>(argValue));
            if (!pBuffer) {
                return tracingScope.exit(CL_INVALID_MEM_OBJECT);
            }

            pKernel->setSharedObjArg(argIndex, pBuffer);

            const auto ptr = pBuffer->getUsmPtr();
            return tracingScope.exit(pKernel->setArgumentValue(argIndex, sizeof(ptr), &ptr));
        } else {
            return tracingScope.exit(pKernel->setArgumentValue(argIndex, argSize, nullptr));
        }
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTImage) {
        if (argSize != sizeof(cl_mem)) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_ARG_SIZE);
        }

        if (!argValue) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }

        auto pImage = NEO::LEO::castToObject<NEO::LEO::Image>(*reinterpret_cast<const cl_mem *>(argValue));
        if (!pImage) {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }

        auto accessQualifier = argDescriptor.getTraits().accessQualifier;
        cl_mem_flags imageFlags = pImage->getFlags();
        if ((accessQualifier == NEO::KernelArgMetadata::AccessReadOnly && ((imageFlags | CL_MEM_WRITE_ONLY) == imageFlags)) ||
            (accessQualifier == NEO::KernelArgMetadata::AccessWriteOnly && ((imageFlags | CL_MEM_READ_ONLY) == imageFlags))) {
            return tracingScope.exit(CL_INVALID_ARG_VALUE);
        }

        cl_int result = CL_SUCCESS;
        for (const auto &[rootDeviceIndex, kernelHandle] : pKernel->getKernelHandles()) {
            const auto imageHandle = pImage->getL0Handle(rootDeviceIndex);
            auto ret = L0ToClResultMapper(zeKernelSetArgumentValue(kernelHandle, argIndex, sizeof(imageHandle), &imageHandle));
            if (ret != CL_SUCCESS) {
                result = ret;
            }
        }
        pKernel->setImageArg(argIndex, pImage);
        return tracingScope.exit(result);
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTSampler) {
        if (argSize != sizeof(cl_sampler)) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_ARG_SIZE);
        }

        if (!argValue) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_SAMPLER);
        }

        auto pSampler = NEO::LEO::castToObject<const NEO::LEO::Sampler>(*reinterpret_cast<const cl_sampler *>(argValue));
        if (!pSampler) {
            return tracingScope.exit(CL_INVALID_SAMPLER);
        }

        cl_int result = CL_SUCCESS;
        for (const auto &[rootDeviceIndex, kernelHandle] : pKernel->getKernelHandles()) {
            const auto samplerHandle = pSampler->getL0Handle(rootDeviceIndex);
            auto ret = L0ToClResultMapper(zeKernelSetArgumentValue(kernelHandle, argIndex, sizeof(samplerHandle), &samplerHandle));
            if (ret != CL_SUCCESS) {
                result = ret;
            }
        }
        return tracingScope.exit(result);
    } else {
        return tracingScope.exit(pKernel->setArgumentValue(argIndex, argSize, argValue));
    }
}

cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel kernel,
                                            cl_uint argIndex,
                                            const void *argValue) {
    TRACING_ENTER(ClSetKernelArgSvmPointer, &kernel, &argIndex, &argValue);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_ARG_INDEX);
    }

    const auto &argDescriptor = pKernel->getL0Object()->getKernelDescriptor().payloadMappings.explicitArgs[argIndex];
    const auto addressQualifier = argDescriptor.getTraits().getAddressQualifier();
    if (addressQualifier != NEO::KernelArgMetadata::AddrGlobal &&
        addressQualifier != NEO::KernelArgMetadata::AddrConstant) {
        return tracingScope.exit(CL_INVALID_ARG_VALUE);
    }

    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);
    pKernel->clearSharedObjArg(argIndex);

    return tracingScope.exit(pKernel->setArgumentValue(argIndex, sizeof(argValue), &argValue));
}

CL_API_ENTRY cl_int CL_API_CALL clSetKernelArgMemPointerINTEL(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue) {
    TRACING_ENTER(ClSetKernelArgMemPointerINTEL, &kernel, &argIndex, &argValue);
    return tracingScope.exit(clSetKernelArgSVMPointer(kernel, argIndex, argValue));
}

cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel kernel,
                                       cl_kernel_exec_info paramName,
                                       size_t paramValueSize,
                                       const void *paramValue) {
    TRACING_ENTER(ClSetKernelExecInfo, &kernel, &paramName, &paramValueSize, &paramValue);
    auto [validationResult, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(validationResult);
    }

    cl_int retVal = CL_SUCCESS;
    switch (paramName) {
    case CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL:
        if (nullptr == paramValue ||
            sizeof(cl_bool) != paramValueSize) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }
        retVal = pKernel->setIndirectAccess(paramName, *static_cast<const cl_bool *>(paramValue));
        break;

    case CL_KERNEL_EXEC_INFO_SVM_PTRS:
    case CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL: {
        if ((0u == paramValueSize && nullptr != paramValue) ||
            (paramValueSize % sizeof(void *)) ||
            (0u != paramValueSize && nullptr == paramValue)) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }

        auto pContext = pKernel->getContext();
        auto svmAllocsManager = pContext->getL0Object()->getDriverHandle()->getSvmAllocsManager();
        bool sharedSystemSupported = true;
        for (auto clDevice : pContext->getClDevices()) {
            if (!clDevice->getDevice().areSharedSystemAllocationsAllowed()) {
                sharedSystemSupported = false;
                break;
            }
        }

        const size_t numPointers = paramValueSize / sizeof(void *);
        auto svmPointers = reinterpret_cast<void *const *>(paramValue);
        for (size_t i = 0; i < numPointers; i++) {
            auto svmData = svmAllocsManager ? svmAllocsManager->getSVMAlloc(svmPointers[i]) : nullptr;
            if (svmPointers[i] != nullptr && svmData == nullptr && sharedSystemSupported) {
                continue;
            }
            if (svmData == nullptr) {
                return tracingScope.exit(CL_INVALID_VALUE);
            }
        }

        if (numPointers > 0u) {
            retVal = pKernel->setIndirectAccessForExecInfoPointers();
        }
        break;
    }
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL:
        if (nullptr == paramValue ||
            sizeof(uint32_t) != paramValueSize) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }
        retVal = pKernel->setThreadArbitrationPolicy(*reinterpret_cast<const uint32_t *>(paramValue));
        break;
    case CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM:
        retVal = CL_INVALID_OPERATION;
        break;
    case CL_KERNEL_EXEC_INFO_DISPATCH_TYPE_INTEL:
    case CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL:
        if (nullptr == paramValue ||
            sizeof(cl_execution_info_kernel_type_intel) != paramValueSize) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }
        retVal = pKernel->setKernelExecutionType(*reinterpret_cast<const cl_execution_info_kernel_type_intel *>(paramValue));
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    return tracingScope.exit(retVal);
}

cl_int CL_API_CALL clGetKernelInfo(cl_kernel kernel,
                                   cl_kernel_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelInfo, &kernel, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet));
}

cl_int CL_API_CALL clGetKernelArgInfo(cl_kernel kernel,
                                      cl_uint argIndx,
                                      cl_kernel_arg_info paramName,
                                      size_t paramValueSize,
                                      void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelArgInfo, &kernel, &argIndx, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet));
}

cl_int CL_API_CALL clGetKernelWorkGroupInfo(cl_kernel kernel,
                                            cl_device_id device,
                                            cl_kernel_work_group_info paramName,
                                            size_t paramValueSize,
                                            void *paramValue,
                                            size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelWorkGroupInfo, &kernel, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pKernel->getWorkGroupInfo(device, paramName, 0u, nullptr, paramValueSize, paramValue, paramValueSizeRet));
}

cl_int CL_API_CALL clGetKernelSubGroupInfo(cl_kernel kernel,
                                           cl_device_id device,
                                           cl_kernel_sub_group_info paramName,
                                           size_t inputValueSize,
                                           const void *inputValue,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelSubGroupInfo, &kernel, &device, &paramName, &inputValueSize, &inputValue, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pKernel->getWorkGroupInfo(device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet));
}

cl_int CL_API_CALL clGetKernelSubGroupInfoKHR(cl_kernel kernel,
                                              cl_device_id device,
                                              cl_kernel_sub_group_info paramName,
                                              size_t inputValueSize,
                                              const void *inputValue,
                                              size_t paramValueSize,
                                              void *paramValue,
                                              size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelSubGroupInfoKHR, &kernel, &device, &paramName, &inputValueSize, &inputValue, &paramValueSize, &paramValue, &paramValueSizeRet);
    return tracingScope.exit(clGetKernelSubGroupInfo(kernel, device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet));
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeINTEL(cl_command_queue commandQueue,
                                                          cl_kernel kernel,
                                                          cl_uint workDim,
                                                          const size_t *globalWorkOffset,
                                                          const size_t *globalWorkSize,
                                                          size_t *suggestedLocalWorkSize) {
    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSizeINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize));
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeKHR(cl_command_queue commandQueue,
                                                        cl_kernel kernel,
                                                        cl_uint workDim,
                                                        const size_t *globalWorkOffset,
                                                        const size_t *globalWorkSize,
                                                        size_t *suggestedLocalWorkSize) {
    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSizeKHR, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    return tracingScope.exit(clGetKernelSuggestedLocalWorkSizeINTEL(commandQueue, kernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize));
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSize(cl_command_queue commandQueue,
                                                     cl_kernel kernel,
                                                     cl_uint workDim,
                                                     const size_t *globalWorkOffset,
                                                     const size_t *globalWorkSize,
                                                     size_t *suggestedLocalWorkSize) {
    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSize, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize));
}

cl_int CL_API_CALL clGetKernelMaxConcurrentWorkGroupCountINTEL(cl_command_queue commandQueue,
                                                               cl_kernel kernel,
                                                               cl_uint workDim,
                                                               const size_t *globalWorkOffset,
                                                               const size_t *localWorkSize,
                                                               size_t *suggestedWorkGroupCount) {
    TRACING_ENTER(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &localWorkSize, &suggestedWorkGroupCount);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pKernel->getMaxConcurrentWorkGroupCount(workDim, localWorkSize, suggestedWorkGroupCount));
}

cl_kernel CL_API_CALL clCloneKernel(cl_kernel sourceKernel,
                                    cl_int *errcodeRet) {
    TRACING_ENTER(ClCloneKernel, &sourceKernel, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(sourceKernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return tracingScope.exit(nullptr);
    }

    return tracingScope.exit(new NEO::LEO::Kernel(pKernel));
}

} // extern "C"

} // namespace LEO
} // namespace NEO
