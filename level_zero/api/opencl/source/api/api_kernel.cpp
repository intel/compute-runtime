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
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/source/sampler/leo_sampler.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <limits>
#include <map>

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

    return kernel->getSuggestedLocalWorkSize(workDim, globalWorkSize, suggestedLocalWorkSize);
}
} // namespace

cl_kernel CL_API_CALL clCreateKernel(cl_program clProgram,
                                     const char *kernelName,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateKernel, &clProgram, &kernelName, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram), std::make_tuple(const_cast<char *>(kernelName)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_kernel tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateKernel, &tracingRetVal);
        return tracingRetVal;
    }

    if (pProgram->getModuleHandles().empty()) [[unlikely]] {
        err.set(CL_INVALID_PROGRAM_EXECUTABLE);
        cl_kernel tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateKernel, &tracingRetVal);
        return tracingRetVal;
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
            cl_kernel tracingRetVal = nullptr;
            TRACING_EXIT(ClCreateKernel, &tracingRetVal);
            return tracingRetVal;
        }
        kernelHandles[rootDeviceIndex] = kernelHandle;
    }

    cl_kernel tracingRetVal = new NEO::LEO::Kernel(std::move(kernelHandles), pProgram);
    TRACING_EXIT(ClCreateKernel, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clCreateKernelsInProgram(cl_program clProgram,
                                            cl_uint numKernels,
                                            cl_kernel *kernels,
                                            cl_uint *numKernelsRet) {
    TRACING_ENTER(ClCreateKernelsInProgram, &clProgram, &numKernels, &kernels, &numKernelsRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClCreateKernelsInProgram, &retVal);
        return retVal;
    }

    auto kernelNames = pProgram->getUserKernelNames();
    auto numUserKernels = static_cast<cl_uint>(kernelNames.size());

    if (kernels) [[likely]] {
        if (numKernels < numUserKernels) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClCreateKernelsInProgram, &tracingRetVal);
            return tracingRetVal;
        }

        size_t i = 0u;
        for (auto kernelName : kernelNames) {
            cl_int ret = CL_SUCCESS;
            kernels[i++] = clCreateKernel(clProgram, kernelName, &ret);
            if (ret != CL_SUCCESS) [[unlikely]] {
                TRACING_EXIT(ClCreateKernelsInProgram, &ret);
                return ret;
            }
        }
    }

    if (numKernelsRet) {
        *numKernelsRet = numUserKernels;
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClCreateKernelsInProgram, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainKernel(cl_kernel kernel) {
    TRACING_ENTER(ClRetainKernel, &kernel);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainKernel, &retVal);
        return retVal;
    }

    pKernel->incRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainKernel, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseKernel(cl_kernel kernel) {
    TRACING_ENTER(ClReleaseKernel, &kernel);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseKernel, &retVal);
        return retVal;
    }

    pKernel->decRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseKernel, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetKernelArg(cl_kernel kernel,
                                  cl_uint argIndex,
                                  size_t argSize,
                                  const void *argValue) {
    TRACING_ENTER(ClSetKernelArg, &kernel, &argIndex, &argSize, &argValue);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetKernelArg, &retVal);
        return retVal;
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_ARG_INDEX;
        TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
        return tracingRetVal;
    }
    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);

    const auto &argDescriptor = pKernel->getL0Object()->getKernelDescriptor().payloadMappings.explicitArgs[argIndex];
    if (argDescriptor.type == NEO::ArgDescriptor::argTValue) [[likely]] {
        cl_int tracingRetVal = pKernel->setArgumentValue(argIndex, argSize, argValue);
        TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
        return tracingRetVal;
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTPointer) {
        auto clMem = reinterpret_cast<const cl_mem *>(argValue);
        if (clMem && *clMem) [[likely]] {
            if (argSize != sizeof(cl_mem)) [[unlikely]] {
                cl_int tracingRetVal = CL_INVALID_ARG_SIZE;
                TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
                return tracingRetVal;
            }

            auto pBuffer = NEO::LEO::castToObject<const NEO::LEO::Buffer>(*reinterpret_cast<const cl_mem *>(argValue));
            if (!pBuffer) {
                cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
                TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
                return tracingRetVal;
            }

            const auto ptr = pBuffer->getUsmPtr();
            cl_int tracingRetVal = pKernel->setArgumentValue(argIndex, sizeof(ptr), &ptr);
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        } else {
            cl_int tracingRetVal = pKernel->setArgumentValue(argIndex, argSize, nullptr);
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTImage) {
        if (argSize != sizeof(cl_mem)) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_ARG_SIZE;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        if (!argValue) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        auto pImage = NEO::LEO::castToObject<NEO::LEO::Image>(*reinterpret_cast<const cl_mem *>(argValue));
        if (!pImage) {
            cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        auto accessQualifier = argDescriptor.getTraits().accessQualifier;
        cl_mem_flags imageFlags = pImage->getFlags();
        if ((accessQualifier == NEO::KernelArgMetadata::AccessReadOnly && ((imageFlags | CL_MEM_WRITE_ONLY) == imageFlags)) ||
            (accessQualifier == NEO::KernelArgMetadata::AccessWriteOnly && ((imageFlags | CL_MEM_READ_ONLY) == imageFlags))) {
            cl_int tracingRetVal = CL_INVALID_ARG_VALUE;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
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
        TRACING_EXIT(ClSetKernelArg, &result);
        return result;
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTSampler) {
        if (argSize != sizeof(cl_sampler)) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_ARG_SIZE;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        if (!argValue) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_SAMPLER;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        auto pSampler = NEO::LEO::castToObject<const NEO::LEO::Sampler>(*reinterpret_cast<const cl_sampler *>(argValue));
        if (!pSampler) {
            cl_int tracingRetVal = CL_INVALID_SAMPLER;
            TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
            return tracingRetVal;
        }

        cl_int result = CL_SUCCESS;
        for (const auto &[rootDeviceIndex, kernelHandle] : pKernel->getKernelHandles()) {
            const auto samplerHandle = pSampler->getL0Handle(rootDeviceIndex);
            auto ret = L0ToClResultMapper(zeKernelSetArgumentValue(kernelHandle, argIndex, sizeof(samplerHandle), &samplerHandle));
            if (ret != CL_SUCCESS) {
                result = ret;
            }
        }
        TRACING_EXIT(ClSetKernelArg, &result);
        return result;
    } else {
        cl_int tracingRetVal = pKernel->setArgumentValue(argIndex, argSize, argValue);
        TRACING_EXIT(ClSetKernelArg, &tracingRetVal);
        return tracingRetVal;
    }
}

cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel kernel,
                                            cl_uint argIndex,
                                            const void *argValue) {
    TRACING_ENTER(ClSetKernelArgSvmPointer, &kernel, &argIndex, &argValue);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
        return retVal;
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_ARG_INDEX;
        TRACING_EXIT(ClSetKernelArgSvmPointer, &tracingRetVal);
        return tracingRetVal;
    }

    const auto &argDescriptor = pKernel->getL0Object()->getKernelDescriptor().payloadMappings.explicitArgs[argIndex];
    const auto addressQualifier = argDescriptor.getTraits().getAddressQualifier();
    if (addressQualifier != NEO::KernelArgMetadata::AddrGlobal &&
        addressQualifier != NEO::KernelArgMetadata::AddrConstant) {
        cl_int tracingRetVal = CL_INVALID_ARG_VALUE;
        TRACING_EXIT(ClSetKernelArgSvmPointer, &tracingRetVal);
        return tracingRetVal;
    }

    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);

    cl_int tracingRetVal = pKernel->setArgumentValue(argIndex, sizeof(argValue), &argValue);
    TRACING_EXIT(ClSetKernelArgSvmPointer, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clSetKernelArgMemPointerINTEL(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue) {
    TRACING_ENTER(ClSetKernelArgMemPointerINTEL, &kernel, &argIndex, &argValue);
    cl_int tracingRetVal = clSetKernelArgSVMPointer(kernel, argIndex, argValue);
    TRACING_EXIT(ClSetKernelArgMemPointerINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel kernel,
                                       cl_kernel_exec_info paramName,
                                       size_t paramValueSize,
                                       const void *paramValue) {
    TRACING_ENTER(ClSetKernelExecInfo, &kernel, &paramName, &paramValueSize, &paramValue);
    auto [validationResult, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetKernelExecInfo, &validationResult);
        return validationResult;
    }

    cl_int retVal = CL_SUCCESS;
    switch (paramName) {
    case CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL:
        if (nullptr == paramValue ||
            sizeof(cl_bool) != paramValueSize) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &tracingRetVal);
            return tracingRetVal;
        }
        retVal = pKernel->setIndirectAccess(paramName, *static_cast<const cl_bool *>(paramValue));
        break;

    case CL_KERNEL_EXEC_INFO_SVM_PTRS:
    case CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL: {
        if ((0u == paramValueSize && nullptr != paramValue) ||
            (paramValueSize % sizeof(void *)) ||
            (0u != paramValueSize && nullptr == paramValue)) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &tracingRetVal);
            return tracingRetVal;
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
                cl_int tracingRetVal = CL_INVALID_VALUE;
                TRACING_EXIT(ClSetKernelExecInfo, &tracingRetVal);
                return tracingRetVal;
            }
        }

        if (numPointers > 0u) {
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, CL_TRUE);
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, CL_TRUE);
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, CL_TRUE);
        }
        break;
    }
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL:
        if (nullptr == paramValue ||
            sizeof(uint32_t) != paramValueSize) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &tracingRetVal);
            return tracingRetVal;
        }
        retVal = pKernel->setThreadArbitrationPolicy(*reinterpret_cast<const uint32_t *>(paramValue));
        break;
    case CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM:
        retVal = CL_INVALID_OPERATION;
        break;
    case CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL:
        if (nullptr == paramValue ||
            sizeof(cl_execution_info_kernel_type_intel) != paramValueSize) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &tracingRetVal);
            return tracingRetVal;
        }
        retVal = pKernel->setKernelExecutionType(*reinterpret_cast<const cl_execution_info_kernel_type_intel *>(paramValue));
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    TRACING_EXIT(ClSetKernelExecInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelInfo(cl_kernel kernel,
                                   cl_kernel_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelInfo, &kernel, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetKernelInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetKernelInfo, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelArgInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetKernelArgInfo, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelWorkGroupInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pKernel->getWorkGroupInfo(device, paramName, 0u, nullptr, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetKernelWorkGroupInfo, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelSubGroupInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pKernel->getWorkGroupInfo(device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetKernelSubGroupInfo, &tracingRetVal);
    return tracingRetVal;
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
    cl_int tracingRetVal = clGetKernelSubGroupInfo(kernel, device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetKernelSubGroupInfoKHR, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeKHR(cl_command_queue commandQueue,
                                                        cl_kernel kernel,
                                                        cl_uint workDim,
                                                        const size_t *globalWorkOffset,
                                                        const size_t *globalWorkSize,
                                                        size_t *suggestedLocalWorkSize) {
    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSizeKHR, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    cl_int tracingRetVal = clGetKernelSuggestedLocalWorkSizeINTEL(commandQueue, kernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeKHR, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSize, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    TRACING_EXIT(ClGetKernelSuggestedLocalWorkSize, &tracingRetVal);
    return tracingRetVal;
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
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pKernel->getMaxConcurrentWorkGroupCount(workDim, localWorkSize, suggestedWorkGroupCount);
    TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_kernel CL_API_CALL clCloneKernel(cl_kernel sourceKernel,
                                    cl_int *errcodeRet) {
    TRACING_ENTER(ClCloneKernel, &sourceKernel, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(sourceKernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_kernel tracingRetVal = nullptr;
        TRACING_EXIT(ClCloneKernel, &tracingRetVal);
        return tracingRetVal;
    }

    cl_kernel tracingRetVal = new NEO::LEO::Kernel(pKernel);
    TRACING_EXIT(ClCloneKernel, &tracingRetVal);
    return tracingRetVal;
}
