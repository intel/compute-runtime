/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/program/kernel_info.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/module/internal_core_program_ext.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_program CL_API_CALL clCreateProgramWithSource(cl_context context,
                                                 cl_uint count,
                                                 const char **strings,
                                                 const size_t *lengths,
                                                 cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context), std::make_tuple(const_cast<char **>(strings)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (count == 0) [[unlikely]] {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    return new NEO::LEO::Program(pContext, count, strings, lengths);
}

cl_program CL_API_CALL clCreateProgramWithBinary(cl_context context,
                                                 cl_uint numDevices,
                                                 const cl_device_id *deviceList,
                                                 const size_t *lengths,
                                                 const unsigned char **binaries,
                                                 cl_int *binaryStatus,
                                                 cl_int *errcodeRet) {
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        if (binaryStatus) {
            binaryStatus[0] = status;
        }
        return new NEO::LEO::Program(nullptr);
    }

    if (lengths == nullptr ||
        lengths[0] == 0u ||
        binaries == nullptr ||
        binaries[0] == nullptr ||
        numDevices == 0u ||
        deviceList == nullptr) {
        status = CL_INVALID_VALUE;
    }

    auto pProgram = new NEO::LEO::Program(pContext);
    if (CL_SUCCESS == status) {
        for (cl_uint i = 0; i < numDevices; ++i) {
            auto deviceStatus = pProgram->createFromBinary(deviceList[i], lengths[i], binaries[i]);
            if (binaryStatus) {
                binaryStatus[i] = deviceStatus;
            }
            if (CL_SUCCESS != deviceStatus) {
                status = deviceStatus;
                break;
            }
        }
    } else if (binaryStatus) {
        binaryStatus[0] = status;
    }
    ErrorCodeHelper(errcodeRet, status);
    return pProgram;
}

cl_program CL_API_CALL clCreateProgramWithIL(cl_context context,
                                             const void *il,
                                             size_t length,
                                             cl_int *errcodeRet) {
    auto [status, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (status != CL_SUCCESS) {
        ErrorCodeHelper(errcodeRet, status);
        return new NEO::LEO::Program(nullptr);
    }

    if (il == nullptr) {
        status = CL_INVALID_VALUE;
    } else if (0u == length) {
        status = CL_INVALID_VALUE;
    }

    auto pProgram = new NEO::LEO::Program(pContext);
    if (CL_SUCCESS == status) {
        status = pProgram->createFromBinary(pContext->getClDevice(), length, reinterpret_cast<const uint8_t *>(il));
    }

    ErrorCodeHelper(errcodeRet, status);
    return pProgram;
}

cl_program CL_API_CALL clCreateProgramWithILKHR(cl_context context,
                                                const void *il,
                                                size_t length,
                                                cl_int *errcodeRet) {
    return clCreateProgramWithIL(context, il, length, errcodeRet);
}

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context context,
                                                         cl_uint numDevices,
                                                         const cl_device_id *deviceList,
                                                         const char *kernelNames,
                                                         cl_int *errcodeRet) {
    return nullptr;
}

cl_int CL_API_CALL clRetainProgram(cl_program program) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pProgram->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseProgram(cl_program program) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pProgram->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clBuildProgram(cl_program program,
                                  cl_uint numDevices,
                                  const cl_device_id *deviceList,
                                  const char *options,
                                  void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                  void *userData) {
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        return CL_INVALID_VALUE;
    }
    if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::binary) [[unlikely]] {
        pProgram->invokeCallback(funcNotify, userData);
        return CL_SUCCESS;
    }

    cl_int retVal = CL_INVALID_OPERATION;
    pProgram->setBuildStatus(CL_BUILD_ERROR);
    if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::source) {
        retVal = pProgram->buildFromSource(options);
    } else if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::il) {
        retVal = pProgram->buildFromIL(options);
    }
    if (retVal != CL_SUCCESS) [[unlikely]] {
        pProgram->invokeCallback(funcNotify, userData);
        return retVal;
    }
    pProgram->setBuildStatus(CL_BUILD_SUCCESS);
    pProgram->invokeCallback(funcNotify, userData);
    return retVal;
}

cl_int CL_API_CALL clCompileProgram(cl_program program,
                                    cl_uint numDevices,
                                    const cl_device_id *deviceList,
                                    const char *options,
                                    cl_uint numInputHeaders,
                                    const cl_program *inputHeaders,
                                    const char **headerIncludeNames,
                                    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                    void *userData) {
    auto [validationResult, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (validationResult != CL_SUCCESS) [[unlikely]] {
        return validationResult;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::binary) [[unlikely]] {
        pProgram->invokeCallback(funcNotify, userData);
        return CL_SUCCESS;
    }

    cl_int retVal = CL_INVALID_OPERATION;
    pProgram->setBuildStatus(CL_BUILD_ERROR);
    if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::source) {
        retVal = pProgram->compileFromSourceWithHeaders(options, numInputHeaders, inputHeaders, headerIncludeNames);
    } else if (pProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::il) {
        retVal = pProgram->buildFromIL(options);
    }
    if (CL_SUCCESS == retVal) {
        pProgram->setProgramBinaryType(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT);
        pProgram->setBuildStatus(CL_BUILD_SUCCESS);
    }
    pProgram->invokeCallback(funcNotify, userData);
    return retVal;
}

cl_program CL_API_CALL clLinkProgram(cl_context context,
                                     cl_uint numDevices,
                                     const cl_device_id *deviceList,
                                     const char *options,
                                     cl_uint numInputPrograms,
                                     const cl_program *inputPrograms,
                                     void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                     void *userData,
                                     cl_int *errcodeRet) {
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        return nullptr;
    }

    if (!NEO::LEO::Program::isValidCallback(funcNotify, userData)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        return nullptr;
    }

    auto retProgram = std::make_unique<NEO::LEO::Program>(pContext);
    retProgram->setBuildStatus(CL_BUILD_ERROR);

    if (numInputPrograms == 0 || inputPrograms == nullptr) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_VALUE);
        retProgram->invokeCallback(funcNotify, userData);
        return retProgram.release();
    }

    // Extracting IR binaries from Program
    std::vector<ze_module_handle_t> modules;
    std::vector<const uint8_t *> spirvBinaries;
    std::vector<size_t> spirvBinariesSizes;
    std::vector<const uint8_t *> llvmbcBinaries;
    std::vector<size_t> llvmbcBinariesSizes;
    for (uint32_t i = 0; i < numInputPrograms; ++i) {
        auto inProgram = NEO::LEO::castToObject<NEO::LEO::Program>(inputPrograms[i]);
        if (nullptr == inProgram) {
            errcodeHelper.set(CL_INVALID_PROGRAM);
            retProgram->invokeCallback(funcNotify, userData);
            return retProgram.release();
        }
        if (inProgram->getCreatedFrom() == NEO::LEO::Program::CreatedFrom::il) {
            if (inProgram->getIsSpirv()) {
                spirvBinaries.push_back(reinterpret_cast<const uint8_t *>(inProgram->getIrBinary()));
                spirvBinariesSizes.push_back(inProgram->getIrBinarySize());
            } else {
                llvmbcBinaries.push_back(reinterpret_cast<const uint8_t *>(inProgram->getIrBinary()));
                llvmbcBinariesSizes.push_back(inProgram->getIrBinarySize());
            }
            continue;
        }
        if (nullptr == inProgram->getModuleHandle()) [[unlikely]] {
            errcodeHelper.set(CL_INVALID_OPERATION);
            retProgram->invokeCallback(funcNotify, userData);
            return retProgram.release();
        }
        modules.push_back(inProgram->getModuleHandle());
    }

    // Extracting IR binaries from L0 module (if did not get from Program directly)
    for (auto moduleHandle : modules) {
        auto l0Module = L0::Module::fromHandle(moduleHandle);
        if (!l0Module->isModulesPackage()) [[likely]] {
            auto moduleImp = static_cast<L0::ModuleImp *>(l0Module);
            auto binarySize = moduleImp->getTranslationUnit()->irBinarySize;
            auto binaryPtr = reinterpret_cast<uint8_t *>(moduleImp->getTranslationUnit()->irBinary.get());
            if (0u == binarySize || nullptr == binaryPtr) [[unlikely]] {
                errcodeHelper.set(CL_INVALID_OPERATION);
                retProgram->invokeCallback(funcNotify, userData);
                return retProgram.release();
            }
            if (moduleImp->isLlvmBc()) {
                llvmbcBinariesSizes.push_back(binarySize);
                llvmbcBinaries.push_back(binaryPtr);
            } else {
                spirvBinariesSizes.push_back(binarySize);
                spirvBinaries.push_back(binaryPtr);
            }
        } else {
            errcodeHelper.set(CL_INVALID_OPERATION);
            retProgram->invokeCallback(funcNotify, userData);
            return retProgram.release();
        }
    }

    // Preparing L0 descriptors
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    ze_module_program_exp_desc_t moduleProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    L0::ze_module_program_llvmbc_exp_desc_t llvmBcDesc;
    if (options) {
        moduleDesc.pBuildFlags = options;
    }
    moduleProgDesc.inputSizes = spirvBinariesSizes.data();
    moduleProgDesc.pInputModules = spirvBinaries.data();
    moduleProgDesc.count = static_cast<uint32_t>(spirvBinaries.size());
    moduleDesc.pNext = &moduleProgDesc;
    if (llvmbcBinaries.size() > 0) {
        llvmBcDesc.inputSizes = llvmbcBinariesSizes.data();
        llvmBcDesc.pInputModules = llvmbcBinaries.data();
        llvmBcDesc.count = static_cast<uint32_t>(llvmbcBinaries.size());
        moduleProgDesc.pNext = &llvmBcDesc;
    }

    auto l0Context = pContext->getL0ContextHandle();
    ze_result_t ret = ZE_RESULT_SUCCESS;
    for (auto clDevice : pContext->getClDevices()) {
        const auto rootDeviceIndex = clDevice->getRootDeviceIndex();
        if (retProgram->getModuleHandle(rootDeviceIndex) != nullptr) {
            continue;
        }
        ze_module_handle_t moduleHandle = {};
        ze_module_build_log_handle_t buildLog{};
        ret = zeModuleCreate(l0Context, clDevice->getL0Handle(), &moduleDesc, &moduleHandle, &buildLog);
        retProgram->setBuildLogHandle(rootDeviceIndex, buildLog);
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            errcodeHelper.set(L0ToClResultMapper(ret));
            retProgram->invokeCallback(funcNotify, userData);
            return retProgram.release();
        }
        retProgram->setModuleHandle(rootDeviceIndex, moduleHandle);
    }

    errcodeHelper.set(L0ToClResultMapper(ret));
    const bool requestedLibrary = options && NEO::CompilerOptions::contains(options, "-create-library");
    retProgram->setProgramBinaryType(requestedLibrary ? CL_PROGRAM_BINARY_TYPE_LIBRARY : CL_PROGRAM_BINARY_TYPE_EXECUTABLE);
    retProgram->setBuildStatus(CL_BUILD_SUCCESS);
    retProgram->invokeCallback(funcNotify, userData);
    return retProgram.release();
}

cl_int CL_API_CALL clGetProgramInfo(cl_program program,
                                    cl_program_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clGetProgramBuildInfo(cl_program program,
                                         cl_device_id device,
                                         cl_program_build_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->getBuildInfo(device, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clSetProgramReleaseCallback(cl_program program,
                                               void(CL_CALLBACK *pfnNotify)(cl_program /* program */, void * /* user_data */),
                                               void *userData) {
    return CL_INVALID_OPERATION;
}

cl_int CL_API_CALL clSetProgramSpecializationConstant(cl_program program, cl_uint specId, size_t specSize, const void *specValue) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program), std::make_tuple(const_cast<void *>(specValue)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pProgram->setProgramSpecializationConstant(specId, specSize, specValue);
}
