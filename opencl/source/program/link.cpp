/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/source/device/cl_device.h"
#include "opencl/source/helpers/validators.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/program.h"

#include "compiler_options.h"

#include <cstring>

namespace NEO {

cl_int Program::link(
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *buildOptions,
    cl_uint numInputPrograms,
    const cl_program *inputPrograms,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData) {
    cl_int retVal = CL_SUCCESS;
    bool isCreateLibrary;

    do {
        if (((deviceList == nullptr) && (numDevices != 0)) ||
            ((deviceList != nullptr) && (numDevices == 0))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((numInputPrograms == 0) || (inputPrograms == nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((funcNotify == nullptr) &&
            (userData != nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((deviceList != nullptr) && validateObject(*deviceList) != CL_SUCCESS) {
            retVal = CL_INVALID_DEVICE;
            break;
        }

        if (buildStatus == CL_BUILD_IN_PROGRESS) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        options = (buildOptions != nullptr) ? buildOptions : "";

        if (isKernelDebugEnabled()) {
            appendKernelDebugOptions();
        }

        isCreateLibrary = CompilerOptions::contains(options, CompilerOptions::createLibrary);

        buildStatus = CL_BUILD_IN_PROGRESS;

        NEO::Elf::ElfEncoder<> elfEncoder(true, false, 1U);
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

        StackVec<const Program *, 16> inputProgramsInternal;
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

            auto sectionType = pInputProgObj->getIsSpirV() ? NEO::Elf::SHT_OPENCL_SPIRV : NEO::Elf::SHT_OPENCL_LLVM_BINARY;
            ConstStringRef sectionName = pInputProgObj->getIsSpirV() ? NEO::Elf::SectionNamesOpenCl::spirvObject : NEO::Elf::SectionNamesOpenCl::llvmObject;
            elfEncoder.appendSection(sectionType, sectionName, ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(pInputProgObj->irBinary.get()), pInputProgObj->irBinarySize));
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        auto clLinkInput = elfEncoder.encode();

        CompilerInterface *pCompilerInterface = pDevice->getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::undefined};

        inputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(clLinkInput.data()), clLinkInput.size());
        inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
        inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());

        if (!isCreateLibrary) {
            inputArgs.outType = IGC::CodeType::oclGenBin;
            NEO::TranslationOutput compilerOuput = {};
            auto compilerErr = pCompilerInterface->link(this->getDevice(), inputArgs, compilerOuput);
            this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
            this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
            retVal = asClError(compilerErr);
            if (retVal != CL_SUCCESS) {
                break;
            }

            this->replaceDeviceBinary(std::move(compilerOuput.deviceBinary.mem), compilerOuput.deviceBinary.size);
            this->debugData = std::move(compilerOuput.debugData.mem);
            this->debugDataSize = compilerOuput.debugData.size;

            retVal = processGenBinary();
            if (retVal != CL_SUCCESS) {
                break;
            }
            programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;

            if (isKernelDebugEnabled()) {
                processDebugData();
                auto clDevice = this->getDevice().getSpecializedDevice<ClDevice>();
                UNRECOVERABLE_IF(clDevice == nullptr);
                for (auto kernelInfo : kernelInfoArray) {
                    clDevice->getSourceLevelDebugger()->notifyKernelDebugData(&kernelInfo->debugData,
                                                                              kernelInfo->name,
                                                                              kernelInfo->heapInfo.pKernelHeap,
                                                                              kernelInfo->heapInfo.pKernelHeader->KernelHeapSize);
                }
            }
        } else {
            inputArgs.outType = IGC::CodeType::llvmBc;
            NEO::TranslationOutput compilerOuput = {};
            auto compilerErr = pCompilerInterface->createLibrary(*this->pDevice, inputArgs, compilerOuput);
            this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
            this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
            retVal = asClError(compilerErr);
            if (retVal != CL_SUCCESS) {
                break;
            }
            this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
            this->irBinarySize = compilerOuput.intermediateRepresentation.size;
            this->isSpirV = (compilerOuput.intermediateCodeType == IGC::CodeType::spirV);
            this->debugData = std::move(compilerOuput.debugData.mem);
            this->debugDataSize = compilerOuput.debugData.size;
            programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
        }
        updateNonUniformFlag(&*inputProgramsInternal.begin(), inputProgramsInternal.size());
        separateBlockKernels();
    } while (false);

    if (retVal != CL_SUCCESS) {
        buildStatus = CL_BUILD_ERROR;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    } else {
        buildStatus = CL_BUILD_SUCCESS;
    }

    internalOptions.clear();

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}
} // namespace NEO
