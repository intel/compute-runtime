/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/kernel/kernel.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/api/opencl/source/sampler/sampler.h"
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
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram), std::make_tuple(const_cast<char *>(kernelName)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (pProgram->getModuleHandles().empty()) [[unlikely]] {
        err.set(CL_INVALID_PROGRAM_EXECUTABLE);
        return nullptr;
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
            return nullptr;
        }
        kernelHandles[rootDeviceIndex] = kernelHandle;
    }

    return new NEO::LEO::Kernel(std::move(kernelHandles), pProgram);
}

cl_int CL_API_CALL clCreateKernelsInProgram(cl_program clProgram,
                                            cl_uint numKernels,
                                            cl_kernel *kernels,
                                            cl_uint *numKernelsRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(clProgram));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto kernelNames = pProgram->getUserKernelNames();
    auto numUserKernels = static_cast<cl_uint>(kernelNames.size());

    if (kernels) [[likely]] {
        if (numKernels < numUserKernels) [[unlikely]] {
            return CL_INVALID_VALUE;
        }

        size_t i = 0u;
        for (auto kernelName : kernelNames) {
            cl_int ret = CL_SUCCESS;
            kernels[i++] = clCreateKernel(clProgram, kernelName, &ret);
            if (ret != CL_SUCCESS) [[unlikely]] {
                return ret;
            }
        }
    }

    if (numKernelsRet) {
        *numKernelsRet = numUserKernels;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clRetainKernel(cl_kernel kernel) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pKernel->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseKernel(cl_kernel kernel) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pKernel->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clSetKernelArg(cl_kernel kernel,
                                  cl_uint argIndex,
                                  size_t argSize,
                                  const void *argValue) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        return CL_INVALID_ARG_INDEX;
    }
    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);

    const auto &argDescriptor = pKernel->getL0Object()->getKernelDescriptor().payloadMappings.explicitArgs[argIndex];
    if (argDescriptor.type == NEO::ArgDescriptor::argTValue) [[likely]] {
        return pKernel->setArgumentValue(argIndex, argSize, argValue);
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTPointer) {
        auto clMem = reinterpret_cast<const cl_mem *>(argValue);
        if (clMem && *clMem) [[likely]] {
            if (argSize != sizeof(cl_mem)) [[unlikely]] {
                return CL_INVALID_ARG_SIZE;
            }

            auto pBuffer = NEO::LEO::castToObject<const NEO::LEO::Buffer>(*reinterpret_cast<const cl_mem *>(argValue));
            if (!pBuffer) {
                return CL_INVALID_MEM_OBJECT;
            }

            const auto ptr = pBuffer->getUsmPtr();
            return pKernel->setArgumentValue(argIndex, sizeof(ptr), &ptr);
        } else {
            return pKernel->setArgumentValue(argIndex, argSize, nullptr);
        }
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTImage) {
        if (argSize != sizeof(cl_mem)) [[unlikely]] {
            return CL_INVALID_ARG_SIZE;
        }

        if (!argValue) [[unlikely]] {
            return CL_INVALID_MEM_OBJECT;
        }

        auto pImage = NEO::LEO::castToObject<NEO::LEO::Image>(*reinterpret_cast<const cl_mem *>(argValue));
        if (!pImage) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto accessQualifier = argDescriptor.getTraits().accessQualifier;
        cl_mem_flags imageFlags = pImage->getFlags();
        if ((accessQualifier == NEO::KernelArgMetadata::AccessReadOnly && ((imageFlags | CL_MEM_WRITE_ONLY) == imageFlags)) ||
            (accessQualifier == NEO::KernelArgMetadata::AccessWriteOnly && ((imageFlags | CL_MEM_READ_ONLY) == imageFlags))) {
            return CL_INVALID_ARG_VALUE;
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
        return result;
    } else if (argDescriptor.type == NEO::ArgDescriptor::argTSampler) {
        if (argSize != sizeof(cl_sampler)) [[unlikely]] {
            return CL_INVALID_ARG_SIZE;
        }

        if (!argValue) [[unlikely]] {
            return CL_INVALID_SAMPLER;
        }

        auto pSampler = NEO::LEO::castToObject<const NEO::LEO::Sampler>(*reinterpret_cast<const cl_sampler *>(argValue));
        if (!pSampler) {
            return CL_INVALID_SAMPLER;
        }

        cl_int result = CL_SUCCESS;
        for (const auto &[rootDeviceIndex, kernelHandle] : pKernel->getKernelHandles()) {
            const auto samplerHandle = pSampler->getL0Handle(rootDeviceIndex);
            auto ret = L0ToClResultMapper(zeKernelSetArgumentValue(kernelHandle, argIndex, sizeof(samplerHandle), &samplerHandle));
            if (ret != CL_SUCCESS) {
                result = ret;
            }
        }
        return result;
    } else {
        return pKernel->setArgumentValue(argIndex, argSize, argValue);
    }
}

cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel kernel,
                                            cl_uint argIndex,
                                            const void *argValue) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (argIndex >= pKernel->getNumArgs()) [[unlikely]] {
        return CL_INVALID_ARG_INDEX;
    }
    pKernel->markArgAsSet(argIndex);
    pKernel->clearImageArg(argIndex);

    return pKernel->setArgumentValue(argIndex, sizeof(argValue), &argValue);
}

CL_API_ENTRY cl_int CL_API_CALL clSetKernelArgMemPointerINTEL(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue) {
    return clSetKernelArgSVMPointer(kernel, argIndex, argValue);
}

cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel kernel,
                                       cl_kernel_exec_info paramName,
                                       size_t paramValueSize,
                                       const void *paramValue) {
    auto [validationResult, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return validationResult;
    }

    cl_int retVal = CL_SUCCESS;
    switch (paramName) {
    case CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL:
        if (nullptr == paramValue ||
            sizeof(cl_bool) != paramValueSize) [[unlikely]] {
            return CL_INVALID_VALUE;
        }
        retVal = pKernel->setIndirectAccess(paramName, *static_cast<const cl_bool *>(paramValue));
        break;

    case CL_KERNEL_EXEC_INFO_SVM_PTRS:
    case CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL:
        if (0u != paramValueSize && nullptr != paramValue) [[likely]] {
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, CL_TRUE);
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, CL_TRUE);
            pKernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, CL_TRUE);
        }
        break;
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL:
        if (nullptr == paramValue ||
            sizeof(uint32_t) != paramValueSize) [[unlikely]] {
            return CL_INVALID_VALUE;
        }
        retVal = pKernel->setThreadArbitrationPolicy(*reinterpret_cast<const uint32_t *>(paramValue));
        break;
    case CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM:
        retVal = CL_INVALID_OPERATION;
        break;
    case CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL:
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    return retVal;
}

cl_int CL_API_CALL clGetKernelInfo(cl_kernel kernel,
                                   cl_kernel_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetKernelArgInfo(cl_kernel kernel,
                                      cl_uint argIndx,
                                      cl_kernel_arg_info paramName,
                                      size_t paramValueSize,
                                      void *paramValue,
                                      size_t *paramValueSizeRet) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetKernelWorkGroupInfo(cl_kernel kernel,
                                            cl_device_id device,
                                            cl_kernel_work_group_info paramName,
                                            size_t paramValueSize,
                                            void *paramValue,
                                            size_t *paramValueSizeRet) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pKernel->getWorkGroupInfo(device, paramName, 0u, nullptr, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetKernelSubGroupInfo(cl_kernel kernel,
                                           cl_device_id device,
                                           cl_kernel_sub_group_info paramName,
                                           size_t inputValueSize,
                                           const void *inputValue,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pKernel->getWorkGroupInfo(device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetKernelSubGroupInfoKHR(cl_kernel kernel,
                                              cl_device_id device,
                                              cl_kernel_sub_group_info paramName,
                                              size_t inputValueSize,
                                              const void *inputValue,
                                              size_t paramValueSize,
                                              void *paramValue,
                                              size_t *paramValueSizeRet) {
    return clGetKernelSubGroupInfo(kernel, device, paramName, inputValueSize, inputValue, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeINTEL(cl_command_queue commandQueue,
                                                          cl_kernel kernel,
                                                          cl_uint workDim,
                                                          const size_t *globalWorkOffset,
                                                          const size_t *globalWorkSize,
                                                          size_t *suggestedLocalWorkSize) {
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeKHR(cl_command_queue commandQueue,
                                                        cl_kernel kernel,
                                                        cl_uint workDim,
                                                        const size_t *globalWorkOffset,
                                                        const size_t *globalWorkSize,
                                                        size_t *suggestedLocalWorkSize) {
    return clGetKernelSuggestedLocalWorkSizeINTEL(commandQueue, kernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSize(cl_command_queue commandQueue,
                                                     cl_kernel kernel,
                                                     cl_uint workDim,
                                                     const size_t *globalWorkOffset,
                                                     const size_t *globalWorkSize,
                                                     size_t *suggestedLocalWorkSize) {
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return getKernelSuggestedLocalWorkSizeImpl(pCommandQueue, pKernel, workDim, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
}

cl_int CL_API_CALL clGetKernelMaxConcurrentWorkGroupCountINTEL(cl_command_queue commandQueue,
                                                               cl_kernel kernel,
                                                               cl_uint workDim,
                                                               const size_t *globalWorkOffset,
                                                               const size_t *localWorkSize,
                                                               size_t *suggestedWorkGroupCount) {
    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pKernel->getMaxConcurrentWorkGroupCount(workDim, localWorkSize, suggestedWorkGroupCount);
}

cl_kernel CL_API_CALL clCloneKernel(cl_kernel sourceKernel,
                                    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(sourceKernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    return new NEO::LEO::Kernel(pKernel);
}
