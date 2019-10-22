/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/compiler_interface.h"
#include "core/elf/writer.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "runtime/device/device.h"
#include "runtime/helpers/validators.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

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
    cl_program program;
    Program *pInputProgObj;
    size_t dataSize;
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

        isCreateLibrary = (strstr(options.c_str(), "-create-library") != nullptr);

        buildStatus = CL_BUILD_IN_PROGRESS;

        CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

        StackVec<const Program *, 16> inputProgramsInternal;
        for (cl_uint i = 0; i < numInputPrograms; i++) {
            program = inputPrograms[i];
            if (program == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            pInputProgObj = castToObject<Program>(program);
            if (pInputProgObj == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            inputProgramsInternal.push_back(pInputProgObj);
            if ((pInputProgObj->irBinary == nullptr) || (pInputProgObj->irBinarySize == 0)) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            elfWriter.addSection(CLElfLib::SSectionNode(pInputProgObj->getIsSpirV() ? CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV : CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY,
                                                        CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "", std::string(pInputProgObj->irBinary.get(), pInputProgObj->irBinarySize), static_cast<uint32_t>(pInputProgObj->irBinarySize)));
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        dataSize = elfWriter.getTotalBinarySize();
        CLElfLib::ElfBinaryStorage data(dataSize);
        elfWriter.resolveBinary(data);

        CompilerInterface *pCompilerInterface = this->executionEnvironment.getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::undefined};

        inputArgs.src = ArrayRef<const char>(data.data(), dataSize);
        inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
        inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());

        if (!isCreateLibrary) {
            inputArgs.outType = IGC::CodeType::oclGenBin;
            NEO::TranslationOutput compilerOuput = {};
            auto compilerErr = pCompilerInterface->link(*this->pDevice, inputArgs, compilerOuput);
            this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
            this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
            retVal = asClError(compilerErr);
            if (retVal != CL_SUCCESS) {
                break;
            }

            this->genBinary = std::move(compilerOuput.deviceBinary.mem);
            this->genBinarySize = compilerOuput.deviceBinary.size;
            this->debugData = std::move(compilerOuput.debugData.mem);
            this->debugDataSize = compilerOuput.debugData.size;

            retVal = processGenBinary();
            if (retVal != CL_SUCCESS) {
                break;
            }
            programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;

            if (isKernelDebugEnabled()) {
                processDebugData();
                for (auto kernelInfo : kernelInfoArray) {
                    pDevice->getSourceLevelDebugger()->notifyKernelDebugData(kernelInfo);
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
