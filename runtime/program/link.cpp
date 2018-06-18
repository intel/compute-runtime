/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/helpers/validators.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "program.h"
#include "elf/writer.h"
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
    CLElfLib::CElfWriter *pElfWriter = nullptr;
    Program *pInputProgObj;
    size_t dataSize;
    char *pData = nullptr;
    bool isCreateLibrary;
    CLElfLib::SSectionNode sectionNode;

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

        isCreateLibrary = (strstr(options.c_str(), "-create-library") != nullptr);

        buildStatus = CL_BUILD_IN_PROGRESS;

        pElfWriter = CLElfLib::CElfWriter::create(CLElfLib::EH_TYPE_OPENCL_OBJECTS, CLElfLib::EH_MACHINE_NONE, 0);

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
            sectionNode.Name = "";
            if (pInputProgObj->getIsSpirV()) {
                sectionNode.Type = CLElfLib::SH_TYPE_SPIRV;
            } else {
                sectionNode.Type = CLElfLib::SH_TYPE_OPENCL_LLVM_BINARY;
            }
            sectionNode.Flags = 0;
            sectionNode.pData = pInputProgObj->irBinary;
            sectionNode.DataSize = static_cast<unsigned int>(pInputProgObj->irBinarySize);

            pElfWriter->addSection(&sectionNode);
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        pElfWriter->resolveBinary(nullptr, dataSize);
        pData = new char[dataSize];
        pElfWriter->resolveBinary(pData, dataSize);

        CompilerInterface *pCompilerInterface = getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationArgs inputArgs = {};

        inputArgs.pInput = pData;
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

    CLElfLib::CElfWriter::destroy(pElfWriter);
    delete[] pData;
    internalOptions.clear();

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}
} // namespace OCLRT
