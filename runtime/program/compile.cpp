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

#include "elf/writer.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/helpers/validators.h"
#include "program.h"
#include <cstring>

namespace OCLRT {

cl_int Program::compile(
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *buildOptions,
    cl_uint numInputHeaders,
    const cl_program *inputHeaders,
    const char **headerIncludeNames,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData) {
    cl_int retVal = CL_SUCCESS;
    cl_program program;
    CLElfLib::CElfWriter *pElfWriter = nullptr;
    Program *pHeaderProgObj;
    size_t compileDataSize;
    char *pCompileData = nullptr;

    do {
        if (((deviceList == nullptr) && (numDevices != 0)) ||
            ((deviceList != nullptr) && (numDevices == 0))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (numInputHeaders == 0) {
            if ((headerIncludeNames != nullptr) || (inputHeaders != nullptr)) {
                retVal = CL_INVALID_VALUE;
                break;
            }
        } else {
            if ((headerIncludeNames == nullptr) || (inputHeaders == nullptr)) {
                retVal = CL_INVALID_VALUE;
                break;
            }
        }

        if ((funcNotify == nullptr) &&
            (userData != nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        // if a device_list is specified, make sure it points to our device
        // NOTE: a null device_list is ok - it means "all devices"
        if ((deviceList != nullptr) && validateObject(*deviceList) != CL_SUCCESS) {
            retVal = CL_INVALID_DEVICE;
            break;
        }

        if (buildStatus == CL_BUILD_IN_PROGRESS) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        buildStatus = CL_BUILD_IN_PROGRESS;

        options = (buildOptions != nullptr) ? buildOptions : "";
        std::string reraStr = "-cl-intel-gtpin-rera";
        size_t pos = options.find(reraStr);
        if (pos != std::string::npos) {
            // compile option "-cl-intel-gtpin-rera" is present, move it to internalOptions
            size_t reraLen = reraStr.length();
            options.erase(pos, reraLen);
            internalOptions.append(reraStr);
            internalOptions.append(" ");
        }

        // create ELF writer to process all sources to be compiled
        pElfWriter = CLElfLib::CElfWriter::create(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_SOURCE, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);
        UNRECOVERABLE_IF(pElfWriter == nullptr);

        CLElfLib::SSectionNode sectionNode;

        // create main section
        sectionNode.Name = "CLMain";
        sectionNode.pData = (char *)sourceCode.c_str();
        sectionNode.DataSize = (unsigned int)(strlen(sourceCode.c_str()) + 1);
        sectionNode.Flags = CLElfLib::E_SH_FLAG::SH_FLAG_NONE;
        sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_SOURCE;

        // add main program's source
        pElfWriter->addSection(&sectionNode);

        for (cl_uint i = 0; i < numInputHeaders; i++) {
            program = inputHeaders[i];
            if (program == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            pHeaderProgObj = castToObject<Program>(program);
            if (pHeaderProgObj == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            sectionNode.Name = headerIncludeNames[i];
            sectionNode.Type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_HEADER;
            sectionNode.Flags = CLElfLib::E_SH_FLAG::SH_FLAG_NONE;
            // collect required data from the header
            retVal = pHeaderProgObj->getSource(sectionNode.pData, sectionNode.DataSize);
            if (retVal != CL_SUCCESS) {
                break;
            }
            pElfWriter->addSection(&sectionNode);
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        pElfWriter->resolveBinary(nullptr, compileDataSize);
        pCompileData = new char[compileDataSize];
        pElfWriter->resolveBinary(pCompileData, compileDataSize);

        CompilerInterface *pCompilerInterface = getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationArgs inputArgs = {};

        // set parameters for compilation
        internalOptions.append(platform()->peekCompilerExtensions());

        if (isKernelDebugEnabled()) {
            internalOptions.append(CompilerOptions::debugKernelEnable);
            options.append(" -g ");
            if (pDevice->getSourceLevelDebugger()) {
                if (pDevice->getSourceLevelDebugger()->isOptimizationDisabled()) {
                    options.append("-cl-opt-disable ");
                }
                std::string filename;
                pDevice->getSourceLevelDebugger()->notifySourceCode(sourceCode.c_str(), sourceCode.size(), filename);
                if (!filename.empty()) {
                    options = std::string("-s ") + filename + " " + options;
                }
            }
        }

        inputArgs.pInput = pCompileData;
        inputArgs.InputSize = (uint32_t)compileDataSize;
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = (uint32_t)options.length();
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = (uint32_t)internalOptions.length();
        inputArgs.pTracingOptions = nullptr;
        inputArgs.TracingOptionsCount = 0;

        retVal = pCompilerInterface->compile(*this, inputArgs);
        if (retVal != CL_SUCCESS) {
            break;
        }
        updateNonUniformFlag();
    } while (false);

    if (retVal != CL_SUCCESS) {
        buildStatus = CL_BUILD_ERROR;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    } else {
        buildStatus = CL_BUILD_SUCCESS;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
    }

    CLElfLib::CElfWriter::destroy(pElfWriter);
    delete[] pCompileData;
    internalOptions.clear();

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}
} // namespace OCLRT
