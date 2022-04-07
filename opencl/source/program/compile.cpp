/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/platform/platform.h"

#include "compiler_options.h"
#include "program.h"

#include <cstring>

namespace NEO {

cl_int Program::compile(
    const ClDeviceVector &deviceVector,
    const char *buildOptions,
    cl_uint numInputHeaders,
    const cl_program *inputHeaders,
    const char **headerIncludeNames) {
    cl_int retVal = CL_SUCCESS;

    auto defaultClDevice = deviceVector[0];
    UNRECOVERABLE_IF(defaultClDevice == nullptr);
    auto &defaultDevice = defaultClDevice->getDevice();
    auto internalOptions = getInternalOptions();
    std::unordered_map<uint32_t, bool> sourceLevelDebuggerNotified;
    do {
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

        if (std::any_of(deviceVector.begin(), deviceVector.end(), [&](auto device) { return CL_BUILD_IN_PROGRESS == deviceBuildInfos[device].buildStatus; })) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        if ((createdFrom == CreatedFrom::IL) || std::all_of(deviceVector.begin(), deviceVector.end(), [&](auto device) { return CL_PROGRAM_BINARY_TYPE_INTERMEDIATE == deviceBuildInfos[device].programBinaryType; })) {
            retVal = CL_SUCCESS;
            break;
        }
        for (const auto &device : deviceVector) {
            sourceLevelDebuggerNotified[device->getRootDeviceIndex()] = false;
            deviceBuildInfos[device].buildStatus = CL_BUILD_IN_PROGRESS;
        }

        options = (buildOptions != nullptr) ? buildOptions : "";
        const auto shouldSuppressRebuildWarning{CompilerOptions::extract(CompilerOptions::noRecompiledFromIr, options)};

        for (const auto &optionString : {CompilerOptions::gtpinRera, CompilerOptions::greaterThan4gbBuffersRequired}) {
            const auto wasExtracted{CompilerOptions::extract(optionString, options)};
            if (wasExtracted) {
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

        CompilerInterface *pCompilerInterface = defaultDevice.getCompilerInterface();
        if (!pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::undefined};

        // set parameters for compilation
        std::string extensions = requiresOpenClCFeatures(options) ? defaultClDevice->peekCompilerExtensionsWithFeatures()
                                                                  : defaultClDevice->peekCompilerExtensions();
        if (requiresAdditionalExtensions(options)) {
            extensions.erase(extensions.length() - 1);
            extensions += ",+cl_khr_3d_image_writes ";
        }
        CompilerOptions::concatenateAppend(internalOptions, extensions);

        if (isKernelDebugEnabled()) {
            for (const auto &device : deviceVector) {
                if (sourceLevelDebuggerNotified[device->getRootDeviceIndex()]) {
                    continue;
                }
                std::string filename;
                appendKernelDebugOptions(*device, internalOptions);
                notifyDebuggerWithSourceCode(*device, filename);
                prependFilePathToOptions(filename);

                sourceLevelDebuggerNotified[device->getRootDeviceIndex()] = true;
            }
        }

        if (!this->getIsBuiltIn() && DebugManager.flags.InjectInternalBuildOptions.get() != "unk") {
            NEO::CompilerOptions::concatenateAppend(internalOptions, NEO::DebugManager.flags.InjectInternalBuildOptions.get());
        }

        inputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(compileData.data()), compileData.size());
        inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
        inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());

        TranslationOutput compilerOuput;
        auto compilerErr = pCompilerInterface->compile(defaultDevice, inputArgs, compilerOuput);
        for (const auto &device : deviceVector) {
            if (requiresRebuild && !shouldSuppressRebuildWarning) {
                this->updateBuildLog(device->getRootDeviceIndex(), CompilerWarnings::recompiledFromIr.data(), CompilerWarnings::recompiledFromIr.length());
            }

            this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
            this->updateBuildLog(device->getRootDeviceIndex(), compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
        }
        retVal = asClError(compilerErr);
        if (retVal != CL_SUCCESS) {
            break;
        }

        this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
        this->irBinarySize = compilerOuput.intermediateRepresentation.size;
        this->isSpirV = compilerOuput.intermediateCodeType == IGC::CodeType::spirV;
        for (const auto &device : deviceVector) {
            this->buildInfos[device->getRootDeviceIndex()].debugData = std::move(compilerOuput.debugData.mem);
            this->buildInfos[device->getRootDeviceIndex()].debugDataSize = compilerOuput.debugData.size;
        }
        updateNonUniformFlag();
    } while (false);

    if (retVal != CL_SUCCESS) {
        for (const auto &device : deviceVector) {
            deviceBuildInfos[device].buildStatus = CL_BUILD_ERROR;
            deviceBuildInfos[device].programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
        }
    } else {
        setBuildStatusSuccess(deviceVector, CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT);
    }

    return retVal;
}
} // namespace NEO
