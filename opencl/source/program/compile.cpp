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

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/validators.h"
#include "opencl/source/platform/platform.h"

#include "compiler_options.h"
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

        if ((createdFrom == CreatedFrom::IL) || (this->programBinaryType == CL_PROGRAM_BINARY_TYPE_INTERMEDIATE)) {
            retVal = CL_SUCCESS;
            break;
        }

        buildStatus = CL_BUILD_IN_PROGRESS;

        options = (buildOptions != nullptr) ? buildOptions : "";

        for (const auto optionString : {CompilerOptions::gtpinRera, CompilerOptions::greaterThan4gbBuffersRequired}) {
            size_t pos = options.find(optionString);
            if (pos != std::string::npos) {
                options.erase(pos, optionString.length());
                CompilerOptions::concatenateAppend(internalOptions, optionString);
            }
        }

        // create ELF writer to process all sources to be compiled
        NEO::Elf::ElfEncoder<> elfEncoder(true, true, 1U);
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_SOURCE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SOURCE, "CLMain", sourceCode);

        for (cl_uint i = 0; i < numInputHeaders; i++) {
            auto program = inputHeaders[i];
            if (program == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            auto pHeaderProgObj = castToObject<Program>(program);
            if (pHeaderProgObj == nullptr) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            std::string includeHeaderSource;
            retVal = pHeaderProgObj->getSource(includeHeaderSource);
            if (retVal != CL_SUCCESS) {
                break;
            }

            elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_HEADER, ConstStringRef(headerIncludeNames[i], strlen(headerIncludeNames[i])), includeHeaderSource);
        }
        if (retVal != CL_SUCCESS) {
            break;
        }

        std::vector<uint8_t> compileData = elfEncoder.encode();

        CompilerInterface *pCompilerInterface = pDevice->getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::undefined};

        // set parameters for compilation
        auto clDevice = this->pDevice->getSpecializedDevice<ClDevice>();
        UNRECOVERABLE_IF(clDevice == nullptr);
        auto compilerExtensionsOptions = clDevice->peekCompilerExtensions();
        CompilerOptions::concatenateAppend(internalOptions, compilerExtensionsOptions);

        if (isKernelDebugEnabled()) {
            std::string filename;
            appendKernelDebugOptions();
            notifyDebuggerWithSourceCode(filename);
            if (!filename.empty()) {
                options = std::string("-s ") + filename + " " + options;
            }
        }

        inputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(compileData.data()), compileData.size());
        inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
        inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());

        TranslationOutput compilerOuput;
        auto compilerErr = pCompilerInterface->compile(*this->pDevice, inputArgs, compilerOuput);
        this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
        this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
        retVal = asClError(compilerErr);
        if (retVal != CL_SUCCESS) {
            break;
        }

        this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
        this->irBinarySize = compilerOuput.intermediateRepresentation.size;
        this->isSpirV = compilerOuput.intermediateCodeType == IGC::CodeType::spirV;
        this->debugData = std::move(compilerOuput.debugData.mem);
        this->debugDataSize = compilerOuput.debugData.size;

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
