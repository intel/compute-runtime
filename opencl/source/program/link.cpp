/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"

#include <cstring>

namespace NEO {

cl_int Program::link(
    const ClDeviceVector &deviceVector,
    const char *buildOptions,
    cl_uint numInputPrograms,
    const cl_program *inputPrograms) {
    cl_int retVal = CL_SUCCESS;
    bool isCreateLibrary;

    auto defaultClDevice = deviceVector[0];
    UNRECOVERABLE_IF(defaultClDevice == nullptr);
    auto &defaultDevice = defaultClDevice->getDevice();
    std::unordered_map<uint32_t, bool> kernelDebugDataNotified;
    std::unordered_map<uint32_t, bool> debugOptionsAppended;
    auto internalOptions = getInternalOptions();
    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    do {
        if ((numInputPrograms == 0) || (inputPrograms == nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (std::any_of(deviceVector.begin(), deviceVector.end(), [&](auto device) { return CL_BUILD_IN_PROGRESS == deviceBuildInfos[device].buildStatus; })) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        for (const auto &device : deviceVector) {
            kernelDebugDataNotified[device->getRootDeviceIndex()] = false;
            debugOptionsAppended[device->getRootDeviceIndex()] = false;
            deviceBuildInfos[device].buildStatus = CL_BUILD_IN_PROGRESS;
        }

        options = (buildOptions != nullptr) ? buildOptions : "";

        for (const auto &optionString : {CompilerOptions::gtpinRera, CompilerOptions::greaterThan4gbBuffersRequired}) {
            size_t pos = options.find(optionString.data());
            if (pos != std::string::npos) {
                options.erase(pos, optionString.length());
                CompilerOptions::concatenateAppend(internalOptions, optionString);
            }
        }

        if (isKernelDebugEnabled()) {
            for (auto &device : deviceVector) {
                if (debugOptionsAppended[device->getRootDeviceIndex()]) {
                    continue;
                }
                appendKernelDebugOptions(*device, internalOptions);

                debugOptionsAppended[device->getRootDeviceIndex()] = true;
            }
        }

        isCreateLibrary = CompilerOptions::contains(options, CompilerOptions::createLibrary);

        NEO::Elf::ElfEncoder<> elfEncoder(true, false, 1U);
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

        StackVec<const Program *, 16> inputProgramsInternal;
        StackVec<uint32_t, 64> specConstIds;
        StackVec<uint64_t, 64> specConstValues;
        for (cl_uint i = 0; i < numInputPrograms; i++) {
            auto program = inputPrograms[i];
            if (program == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            auto pInputProgObj = castToObject<Program>(program);
            if (pInputProgObj == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            inputProgramsInternal.push_back(pInputProgObj);
            if ((pInputProgObj->irBinary == nullptr) || (pInputProgObj->irBinarySize == 0)) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            if (pInputProgObj->areSpecializationConstantsInitialized) {
                specConstIds.clear();
                specConstValues.clear();
                specConstIds.reserve(pInputProgObj->specConstantsValues.size());
                specConstValues.reserve(pInputProgObj->specConstantsValues.size());
                for (const auto &specConst : pInputProgObj->specConstantsValues) {
                    specConstIds.push_back(specConst.first);
                    specConstValues.push_back(specConst.second);
                }
                elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV_SC_IDS, NEO::Elf::SectionNamesOpenCl::spirvSpecConstIds,
                                         ArrayRef<const uint8_t>::fromAny(specConstIds.begin(), specConstIds.size()));
                elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV_SC_VALUES, NEO::Elf::SectionNamesOpenCl::spirvSpecConstValues,
                                         ArrayRef<const uint8_t>::fromAny(specConstValues.begin(), specConstValues.size()));
            }

            auto sectionType = pInputProgObj->getIsSpirV() ? NEO::Elf::SHT_OPENCL_SPIRV : NEO::Elf::SHT_OPENCL_LLVM_BINARY;
            ConstStringRef sectionName = pInputProgObj->getIsSpirV() ? NEO::Elf::SectionNamesOpenCl::spirvObject : NEO::Elf::SectionNamesOpenCl::llvmObject;
            elfEncoder.appendSection(sectionType, sectionName, ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(pInputProgObj->irBinary.get()), pInputProgObj->irBinarySize));
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        auto clLinkInput = elfEncoder.encode();

        CompilerInterface *pCompilerInterface = defaultDevice.getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::undefined};

        inputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(clLinkInput.data()), clLinkInput.size());
        inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
        inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
        inputArgs.GTPinInput = gtpinGetIgcInit();

        if (!isCreateLibrary) {
            for (const auto &device : deviceVector) {
                auto rootDeviceIndex = device->getRootDeviceIndex();
                inputArgs.outType = IGC::CodeType::oclGenBin;
                NEO::TranslationOutput compilerOuput = {};
                auto compilerErr = pCompilerInterface->link(device->getDevice(), inputArgs, compilerOuput);
                this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
                this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
                retVal = asClError(compilerErr);
                if (retVal != CL_SUCCESS) {
                    break;
                }

                this->replaceDeviceBinary(std::move(compilerOuput.deviceBinary.mem), compilerOuput.deviceBinary.size, rootDeviceIndex);
                this->buildInfos[device->getRootDeviceIndex()].debugData = std::move(compilerOuput.debugData.mem);
                this->buildInfos[device->getRootDeviceIndex()].debugDataSize = compilerOuput.debugData.size;

                retVal = processGenBinary(*device);
                if (retVal != CL_SUCCESS) {
                    break;
                }
                binaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;

                if (isKernelDebugEnabled()) {
                    if (kernelDebugDataNotified[rootDeviceIndex]) {
                        continue;
                    }
                    notifyDebuggerWithDebugData(device);
                    kernelDebugDataNotified[device->getRootDeviceIndex()] = true;
                }
            }

        } else {
            inputArgs.outType = IGC::CodeType::llvmBc;
            NEO::TranslationOutput compilerOuput = {};
            auto compilerErr = pCompilerInterface->createLibrary(defaultDevice, inputArgs, compilerOuput);
            for (const auto &device : deviceVector) {
                this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
                this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
            }
            retVal = asClError(compilerErr);
            if (retVal != CL_SUCCESS) {
                break;
            }
            this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
            this->irBinarySize = compilerOuput.intermediateRepresentation.size;
            this->isSpirV = (compilerOuput.intermediateCodeType == IGC::CodeType::spirV);
            for (const auto &device : deviceVector) {
                this->buildInfos[device->getRootDeviceIndex()].debugData = std::move(compilerOuput.debugData.mem);
                this->buildInfos[device->getRootDeviceIndex()].debugDataSize = compilerOuput.debugData.size;
            }
            binaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
        }
        if (retVal != CL_SUCCESS) {
            break;
        }
        updateNonUniformFlag(&*inputProgramsInternal.begin(), inputProgramsInternal.size());
    } while (false);

    if (retVal != CL_SUCCESS) {
        for (const auto &device : deviceVector) {
            deviceBuildInfos[device].buildStatus = CL_BUILD_ERROR;
            deviceBuildInfos[device].programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
        }
    } else {
        setBuildStatusSuccess(deviceVector, binaryType);
    }

    return retVal;
}
} // namespace NEO
