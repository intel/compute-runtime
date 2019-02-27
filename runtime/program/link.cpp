/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "elf/writer.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "runtime/helpers/validators.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "program.h"

#include <cstring>

namespace OCLRT {

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
                                                        CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "", std::string(pInputProgObj->irBinary, pInputProgObj->irBinarySize), static_cast<uint32_t>(pInputProgObj->irBinarySize)));
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

        TranslationArgs inputArgs = {};

        inputArgs.pInput = data.data();
        inputArgs.InputSize = (uint32_t)dataSize;
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = (uint32_t)options.length();
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = (uint32_t)internalOptions.length();
        inputArgs.pTracingOptions = nullptr;
        inputArgs.TracingOptionsCount = 0;

        if (!isCreateLibrary) {
            retVal = pCompilerInterface->link(*this, inputArgs);
            if (retVal != CL_SUCCESS) {
                break;
            }

            retVal = processGenBinary();
            if (retVal != CL_SUCCESS) {
                break;
            }
            programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;

            if (isKernelDebugEnabled()) {
                processDebugData();
                for (size_t i = 0; i < kernelInfoArray.size(); i++) {
                    pDevice->getSourceLevelDebugger()->notifyKernelDebugData(kernelInfoArray[i]);
                }
            }
        } else {
            retVal = pCompilerInterface->createLibrary(*this, inputArgs);
            if (retVal != CL_SUCCESS) {
                break;
            }
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
} // namespace OCLRT
