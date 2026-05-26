/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/program/program.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/core/source/module/defines_ext.h"
#include "level_zero/core/source/module/internal_core_program_ext.h"
#include "level_zero/core/source/module/module_build_log.h"

#include <cstring>
#include <string_view>

namespace NEO {
namespace LEO {

Program::Program(Context *context, cl_uint count, const char **strings, const size_t *lengths) : Program(context) {
    for (cl_uint i = 0; i < count; ++i) {
        size_t size = (lengths && lengths[i] != 0) ? lengths[i] : strlen(strings[i]);
        this->source += std::string_view(strings[i], size);
    }
    source += '\0';
    this->createdFrom = CreatedFrom::source;
}

Program::Program(Context *context) : context(context) {
    this->context->incRefInternal();
}

Program::~Program() {
    if (this->moduleHandle) {
        zeModuleDestroy(this->moduleHandle);
    }
    if (this->buildLogHandle) {
        zeModuleBuildLogDestroy(this->buildLogHandle);
    }

    context->decRefInternal();
}

Program::CreatedFrom Program::getCreatedFrom() const {
    return this->createdFrom;
}

Context *Program::getContext() {
    return this->context;
}

std::string_view Program::getSource() const {
    return this->source;
}

const char *Program::getIrBinary() const {
    return this->irBinary.get();
}

size_t Program::getIrBinarySize() const {
    return this->irBinarySize;
}

bool Program::getIsSpirv() const {
    return this->isSpirv;
}

ze_module_handle_t Program::getModuleHandle() const {
    return this->moduleHandle;
}

bool Program::isValidCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData) {
    return funcNotify != nullptr || userData == nullptr;
}

void Program::invokeCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData) {
    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }
}

cl_int Program::createFromBinary(cl_device_id device, size_t length, const unsigned char *binary) {
    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(device);
    if (!deviceHandle) {
        return CL_INVALID_DEVICE;
    }

    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(binary), length);
    const bool isSpirV = NEO::isSpirVBitcode(archive);
    const bool isLlvmBc = NEO::isLlvmBitcode(archive);
    const bool isDeviceBinary = isAnyDeviceBinaryFormat(archive);
    if (isSpirV || isLlvmBc) {
        this->irBinary = makeCopy(binary, length);
        this->irBinarySize = length;
        this->setProgramBinaryType(isSpirV ? CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT : CL_PROGRAM_BINARY_TYPE_LIBRARY);
        this->createdFrom = CreatedFrom::il;
        this->isSpirv = isSpirV;
        return CL_SUCCESS;
    } else if (!isDeviceBinary) {
        return CL_INVALID_BINARY;
    }

    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_NATIVE, length, binary, nullptr, nullptr};
    ze_result_t ret = zeModuleCreate(this->context->getL0ContextHandle(), deviceHandle, &moduleDescription, &this->moduleHandle, &buildLogHandle);
    if (ZE_RESULT_SUCCESS == ret) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    } else {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    }
    this->createdFrom = CreatedFrom::binary;
    return L0ToClResultMapper(ret);
}

/**
 * @brief Use l0 api to compile to spirv. Resulting l0 module is expected to have irBinary.
 * This is used in linking to gen binary or llvmbc library.
 */
cl_int Program::compileFromSourceWithHeaders(cl_device_id device, const char *options, cl_uint numInputHeaders,
                                             const cl_program *inputHeaders, const char **headerIncludeNames) {
    L0::ze_module_program_headers_exp_desc_t headersDesc{};
    std::vector<const char *> headersSources(numInputHeaders);
    std::vector<size_t> headerSourceSizes(numInputHeaders);
    if (numInputHeaders > 0) {
        for (uint32_t i = 0; i < numInputHeaders; ++i) {
            auto program = NEO::LEO::castToObject<NEO::LEO::Program>(inputHeaders[i]);
            if (program == nullptr) {
                return CL_INVALID_PROGRAM;
            }
            auto headerSource = program->getSource();
            if (headerSource.data() == nullptr) {
                return CL_INVALID_OPERATION;
            }
            headersSources[i] = headerSource.data();
            headerSourceSizes[i] = headerSource.size();
        }

        headersDesc.count = numInputHeaders;
        headersDesc.headerSources = headersSources.data();
        headersDesc.headerSourceSizes = headerSourceSizes.data();
        headersDesc.headerNames = headerIncludeNames;
    }

    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(device);
    if (!deviceHandle) {
        return CL_INVALID_DEVICE;
    }
    if (options) {
        this->options = options;
    }
    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_OCLC, this->source.length(), reinterpret_cast<const uint8_t *>(this->source.data()), options, nullptr};
    moduleDescription.pNext = &headersDesc;

    ze_result_t ret = zeModuleCreate(this->context->getL0ContextHandle(), deviceHandle, &moduleDescription, &this->moduleHandle, &buildLogHandle);
    if (ZE_RESULT_SUCCESS == ret) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    } else {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    }
    return L0ToClResultMapper(ret);
}

/**
 * @brief Directly build from opencl c source to gen binary. This is used in clBuildProgram.
 * The source is not wrapped in elf but passed directly (otherwise fcl is skipped and -cmc flag not handled).
 */
cl_int Program::buildFromSource(cl_device_id device, const char *options) {
    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(device);
    if (!deviceHandle) {
        return CL_INVALID_DEVICE;
    }
    if (options) {
        this->options = options;
    }
    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_OCLC, this->source.length(), reinterpret_cast<const uint8_t *>(this->source.data()), options, nullptr};

    ze_result_t ret = zeModuleCreate(this->context->getL0ContextHandle(), deviceHandle, &moduleDescription, &this->moduleHandle, &buildLogHandle);
    if (ZE_RESULT_SUCCESS == ret) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    } else {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    }
    return L0ToClResultMapper(ret);
}

/**
 * @brief Use l0 api to link spirv to gen binary.
 */
cl_int Program::buildFromIL(cl_device_id device, const char *options) {
    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(device);
    if (!deviceHandle) {
        return CL_INVALID_DEVICE;
    }
    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_IL_SPIRV, this->irBinarySize, reinterpret_cast<uint8_t *>(this->irBinary.get()), options, nullptr};

    ze_module_constants_t moduleConstants;
    std::vector<uint32_t> constantsIds;
    std::vector<const void *> constantsValuesPtrs;
    auto lock = std::lock_guard(this->specializationConstantsMutex);
    DEBUG_BREAK_IF(this->specConstantsValues.size() > std::numeric_limits<uint32_t>::max());
    if (this->specConstantsValues.size() > 0) {
        moduleConstants.numConstants = static_cast<uint32_t>(this->specConstantsValues.size());
        constantsIds.reserve(moduleConstants.numConstants);
        constantsValuesPtrs.reserve(moduleConstants.numConstants);
        for (const auto &[id, value] : this->specConstantsValues) {
            constantsIds.push_back(id);
            constantsValuesPtrs.push_back(&value);
        }
        moduleConstants.pConstantIds = constantsIds.data();
        moduleConstants.pConstantValues = constantsValuesPtrs.data();
        moduleDescription.pConstants = &moduleConstants;
    }
    ze_result_t ret = zeModuleCreate(this->context->getL0ContextHandle(), deviceHandle, &moduleDescription, &this->moduleHandle, &buildLogHandle);
    if (ZE_RESULT_SUCCESS == ret) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    } else {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    }
    return L0ToClResultMapper(ret);
}

cl_int Program::setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    if (CreatedFrom::il != this->createdFrom) {
        return CL_INVALID_PROGRAM;
    }

    if (nullptr == specValue ||
        0u == specSize ||
        specSize > sizeof(uint64_t)) {
        return CL_INVALID_VALUE;
    }

    uint64_t value = 0u;
    memcpy_s(&value, sizeof(value), specValue, specSize);
    auto lock = std::lock_guard(this->specializationConstantsMutex);
    specConstantsValues[specId] = value;
    return CL_SUCCESS;
}

cl_int Program::getInfo(cl_program_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    StackVec<size_t, 1> binarySizes;
    auto clDevicesSize = 1u;
    auto requiredSize = clDevicesSize * sizeof(unsigned char *);
    size_t retSize = 0;
    cl_uint refCount = 0;
    cl_context clContext = context;
    cl_uint clFalse = CL_FALSE;
    std::vector<cl_device_id> devicesToExpose{};
    for (const auto clDevice : context->getClDevices()) {
        devicesToExpose.push_back(clDevice);
    }
    uint32_t numDevices = static_cast<uint32_t>(devicesToExpose.size());
    uint32_t numKernels = 0u;
    std::string kernelNames;

    switch (paramName) {
    case CL_PROGRAM_CONTEXT:
        pSrc = &clContext;
        retSize = srcSize = sizeof(clContext);
        break;

    case CL_PROGRAM_NUM_DEVICES:
        pSrc = &numDevices;
        retSize = srcSize = sizeof(cl_uint);
        break;

    case CL_PROGRAM_DEVICES:
        pSrc = devicesToExpose.data();
        retSize = srcSize = devicesToExpose.size() * sizeof(cl_device_id);
        break;

    case CL_PROGRAM_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        retSize = srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_PROGRAM_SOURCE:
        pSrc = source.c_str();
        retSize = srcSize = strlen(source.c_str()) + 1;
        break;
    case CL_PROGRAM_IL:
        if (CreatedFrom::il != createdFrom) {
            if (paramValueSizeRet) {
                *paramValueSizeRet = 0;
            }
            return CL_SUCCESS;
        }
        pSrc = irBinary.get();
        retSize = srcSize = irBinarySize;
        break;
    case CL_PROGRAM_DEBUG_INFO_SIZES_INTEL:
        break;

    case CL_PROGRAM_BINARIES: {
        if (!paramValue) {
            retSize = requiredSize;
            srcSize = 0u;
            break;
        }
        if (paramValueSize < requiredSize) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        auto outputBinaries = reinterpret_cast<unsigned char **>(paramValue);
        for (auto i = 0u; i < clDevicesSize; i++) {
            if (outputBinaries[i] == nullptr) {
                continue;
            }
            switch (this->programBinaryType) {
            case CL_PROGRAM_BINARY_TYPE_EXECUTABLE: {
                auto zeStatus = zeModuleGetNativeBinary(this->moduleHandle, binarySizes.begin(), outputBinaries[i]);
                // binary size is unused but needs a valid ptr
                if (ZE_RESULT_SUCCESS != zeStatus) {
                    return L0ToClResultMapper(zeStatus);
                }
                continue;
            }
            case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT:
            case CL_PROGRAM_BINARY_TYPE_LIBRARY:
            case CL_PROGRAM_BINARY_TYPE_INTERMEDIATE:
                if (this->irBinarySize > 0) {
                    memcpy_s(outputBinaries[i], this->irBinarySize, this->irBinary.get(), this->irBinarySize);
                } else {
                    auto l0Module = L0::Module::fromHandle(this->moduleHandle);
                    l0Module->getIrBinary(binarySizes.begin(), outputBinaries[i]);
                }
                continue;
            case CL_PROGRAM_BINARY_TYPE_NONE:
            default:
                continue;
            }
        }
        GetInfo::setParamValueReturnSize(paramValueSizeRet, requiredSize, GetInfoStatus::success);
        return CL_SUCCESS;
        break;
    }
    case CL_PROGRAM_BINARY_SIZES:
        binarySizes.resize(clDevicesSize, 0u);
        for (auto i = 0u; i < clDevicesSize; i++) {
            switch (this->programBinaryType) {
            case CL_PROGRAM_BINARY_TYPE_EXECUTABLE:
                zeModuleGetNativeBinary(this->moduleHandle, &binarySizes[i], nullptr);
                continue;
            case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT:
            case CL_PROGRAM_BINARY_TYPE_LIBRARY:
            case CL_PROGRAM_BINARY_TYPE_INTERMEDIATE:
                if (this->irBinarySize > 0) {
                    binarySizes[i] = this->irBinarySize;
                } else {
                    auto l0Module = L0::Module::fromHandle(this->moduleHandle);
                    l0Module->getIrBinary(&binarySizes[i], nullptr);
                }
                continue;
            case CL_PROGRAM_BINARY_TYPE_NONE:
            default:
                binarySizes[i] = 0;
                continue;
            }
        }

        pSrc = binarySizes.data();
        retSize = srcSize = binarySizes.size() * sizeof(size_t);
        break;

    case CL_PROGRAM_DEBUG_INFO_INTEL:
        retVal = CL_INVALID_VALUE;
        break;
    case CL_PROGRAM_NUM_KERNELS:
        if (nullptr == moduleHandle) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        } else {
            zeModuleGetKernelNames(moduleHandle, &numKernels, nullptr);
            pSrc = &numKernels;
            retSize = srcSize = sizeof(numKernels);
        }
        break;
    case CL_PROGRAM_KERNEL_NAMES:
        if (nullptr == moduleHandle) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
        } else {
            zeModuleGetKernelNames(moduleHandle, &numKernels, nullptr);
            std::vector<char *> kernelNamePtrs(numKernels, nullptr);
            zeModuleGetKernelNames(moduleHandle, &numKernels, const_cast<const char **>(kernelNamePtrs.data()));
            for (auto kernelName : kernelNamePtrs) {
                if (!kernelNames.empty()) {
                    kernelNames += ";";
                }
                kernelNames += kernelName;
            }
            retSize = srcSize = kernelNames.length() + 1;
            pSrc = kernelNames.c_str();
        }
        break;

    case CL_PROGRAM_SCOPE_GLOBAL_CTORS_PRESENT:
    case CL_PROGRAM_SCOPE_GLOBAL_DTORS_PRESENT:
        retSize = srcSize = sizeof(clFalse);
        pSrc = &clFalse;
        break;

    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    auto getInfoStatus = GetInfoStatus::invalidValue;
    if (retVal == CL_SUCCESS) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    }
    GetInfo::setParamValueReturnSize(paramValueSizeRet, retSize, getInfoStatus);
    return retVal;
}

cl_int Program::getBuildInfo(cl_device_id device, cl_program_build_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    const void *pSrc = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    size_t retSize = 0;
    uint64_t globalVariablesSize = 0;

    auto l0Module = L0::Module::fromHandle(this->moduleHandle);
    auto l0BuildLog = L0::ModuleBuildLog::fromHandle(this->buildLogHandle);
    const char *buildLog = l0BuildLog ? l0BuildLog->getBuildLog() : "";

    switch (paramName) {
    case CL_PROGRAM_BUILD_STATUS:
        srcSize = retSize = sizeof(cl_build_status);
        pSrc = &buildStatus;
        break;

    case CL_PROGRAM_BUILD_OPTIONS:
        srcSize = retSize = strlen(options.c_str()) + 1;
        pSrc = options.c_str();
        break;
    case CL_PROGRAM_BUILD_LOG: {
        pSrc = buildLog;
        srcSize = retSize = strlen(buildLog) + 1;
    } break;
    case CL_PROGRAM_BINARY_TYPE:
        srcSize = retSize = sizeof(cl_program_binary_type);
        pSrc = &programBinaryType;
        break;
    case CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE: {
        if (CL_BUILD_SUCCESS != buildStatus) {
            return CL_INVALID_PROGRAM;
        }
        if (!l0Module->isModulesPackage()) {
            auto moduleImp = static_cast<L0::ModuleImp *>(l0Module);
            globalVariablesSize = moduleImp->getTranslationUnit()->programInfo.globalVariables.size + moduleImp->getTranslationUnit()->programInfo.globalVariables.zeroInitSize;
        } else {
            auto modulesPackage = static_cast<L0::ModulesPackage *>(l0Module);
            std::vector<ze_module_handle_t> underlyingModuleHandles;
            modulesPackage->getAllUnderlyingModuleHandles(underlyingModuleHandles);
            for (auto underlyingModuleHandle : underlyingModuleHandles) {
                auto moduleImp = static_cast<L0::ModuleImp *>(L0::Module::fromHandle(underlyingModuleHandle));
                globalVariablesSize += moduleImp->getTranslationUnit()->programInfo.globalVariables.size + moduleImp->getTranslationUnit()->programInfo.globalVariables.zeroInitSize;
            }
        }
        srcSize = sizeof(globalVariablesSize);
        pSrc = &globalVariablesSize;
        break;
    }
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    auto getInfoStatus = GetInfoStatus::invalidValue;
    if (retVal == CL_SUCCESS) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pSrc, srcSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    }
    GetInfo::setParamValueReturnSize(paramValueSizeRet, retSize, getInfoStatus);

    return retVal;
}

} // namespace LEO
} // namespace NEO
