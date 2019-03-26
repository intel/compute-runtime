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

namespace NEO {

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
    Program *pHeaderProgObj;
    size_t compileDataSize;

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
        CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_SOURCE, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

        CLElfLib::SSectionNode sectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_SOURCE, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "CLMain", sourceCode, static_cast<uint32_t>(sourceCode.size() + 1u));

        // add main program's source
        elfWriter.addSection(sectionNode);

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
            sectionNode.name = headerIncludeNames[i];
            sectionNode.type = CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_HEADER;
            sectionNode.flag = CLElfLib::E_SH_FLAG::SH_FLAG_NONE;
            // collect required data from the header
            retVal = pHeaderProgObj->getSource(sectionNode.data);
            if (retVal != CL_SUCCESS) {
                break;
            }

            sectionNode.dataSize = static_cast<uint32_t>(sectionNode.data.size());
            elfWriter.addSection(sectionNode);
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        compileDataSize = elfWriter.getTotalBinarySize();
        CLElfLib::ElfBinaryStorage compileData(compileDataSize);
        elfWriter.resolveBinary(compileData);

        CompilerInterface *pCompilerInterface = this->executionEnvironment.getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationArgs inputArgs = {};

        // set parameters for compilation
        internalOptions.append(platform()->peekCompilerExtensions());

        if (isKernelDebugEnabled()) {
            std::string filename;
            appendKernelDebugOptions();
            notifyDebuggerWithSourceCode(filename);
            if (!filename.empty()) {
                options = std::string("-s ") + filename + " " + options;
            }
        }

        inputArgs.pInput = compileData.data();
        inputArgs.InputSize = static_cast<uint32_t>(compileDataSize);
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = static_cast<uint32_t>(options.length());
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = static_cast<uint32_t>(internalOptions.length());
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

    internalOptions.clear();

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}
} // namespace NEO
