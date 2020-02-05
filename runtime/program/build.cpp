/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/compiler_interface.h"
#include "core/execution_environment/execution_environment.h"
#include "core/utilities/time_measure_wrapper.h"
#include "runtime/device/cl_device.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/validators.h"
#include "runtime/platform/platform.h"
#include "runtime/program/kernel_info.h"
#include "runtime/program/program.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/utilities/logger.h"

#include "compiler_options.h"

#include <cstring>
#include <iterator>
#include <sstream>

namespace NEO {

cl_int Program::build(
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *buildOptions,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData,
    bool enableCaching) {
    cl_int retVal = CL_SUCCESS;

    do {
        if (((deviceList == nullptr) && (numDevices != 0)) ||
            ((deviceList != nullptr) && (numDevices == 0))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((funcNotify == nullptr) &&
            (userData != nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        // if a device_list is specified, make sure it points to our device
        // NOTE: a null device_list is ok - it means "all devices"
        if (deviceList && validateObject(*deviceList) != CL_SUCCESS) {
            retVal = CL_INVALID_DEVICE;
            break;
        }

        // check to see if a previous build request is in progress
        if (buildStatus == CL_BUILD_IN_PROGRESS) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        if (isCreatedFromBinary == false) {
            buildStatus = CL_BUILD_IN_PROGRESS;

            options = (buildOptions) ? buildOptions : "";
            extractInternalOptions(options);
            applyAdditionalOptions();

            CompilerInterface *pCompilerInterface = this->executionEnvironment.getCompilerInterface();
            if (!pCompilerInterface) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
            if ((createdFrom == CreatedFrom::IL) || (this->programBinaryType == CL_PROGRAM_BINARY_TYPE_INTERMEDIATE)) {
                inputArgs.srcType = isSpirV ? IGC::CodeType::spirV : IGC::CodeType::llvmBc;
                inputArgs.src = ArrayRef<const char>(irBinary.get(), irBinarySize);
            } else {
                inputArgs.src = ArrayRef<const char>(sourceCode.c_str(), sourceCode.size());
            }

            if (inputArgs.src.size() == 0) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            if (isKernelDebugEnabled()) {
                std::string filename;
                appendKernelDebugOptions();
                notifyDebuggerWithSourceCode(filename);
                if (!filename.empty()) {
                    // Add "-s" flag first so it will be ignored by clang in case the options already have this flag set.
                    options = std::string("-s ") + filename + " " + options;
                }
            }

            auto compilerExtensionsOptions = this->pDevice->peekCompilerExtensions();
            if (internalOptions.find(compilerExtensionsOptions) == std::string::npos) {
                CompilerOptions::concatenateAppend(internalOptions, compilerExtensionsOptions);
            }

            inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
            inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
            inputArgs.GTPinInput = gtpinGetIgcInit();
            inputArgs.specConstants.idsBuffer = this->specConstantsIds.get();
            inputArgs.specConstants.sizesBuffer = this->specConstantsSizes.get();
            inputArgs.specConstants.valuesBuffer = this->specConstantsValues.get();
            DBG_LOG(LogApiCalls,
                    "Build Options", inputArgs.apiOptions.begin(),
                    "\nBuild Internal Options", inputArgs.internalOptions.begin());
            inputArgs.allowCaching = enableCaching;
            NEO::TranslationOutput compilerOuput = {};
            auto compilerErr = pCompilerInterface->build(this->pDevice->getDevice(), inputArgs, compilerOuput);
            this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
            this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
            retVal = asClError(compilerErr);
            if (retVal != CL_SUCCESS) {
                break;
            }
            if (inputArgs.srcType == IGC::CodeType::oclC) {
                this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
                this->irBinarySize = compilerOuput.intermediateRepresentation.size;
                this->isSpirV = compilerOuput.intermediateCodeType == IGC::CodeType::spirV;
            }
            this->genBinary = std::move(compilerOuput.deviceBinary.mem);
            this->genBinarySize = compilerOuput.deviceBinary.size;
            this->debugData = std::move(compilerOuput.debugData.mem);
            this->debugDataSize = compilerOuput.debugData.size;
        }
        updateNonUniformFlag();

        if (DebugManager.flags.PrintProgramBinaryProcessingTime.get()) {
            retVal = TimeMeasureWrapper::functionExecution(*this, &Program::processGenBinary);
        } else {
            retVal = processGenBinary();
        }

        if (retVal != CL_SUCCESS) {
            break;
        }

        if (isKernelDebugEnabled()) {
            processDebugData();
            if (pDevice->getSourceLevelDebugger()) {
                for (auto kernelInfo : kernelInfoArray) {
                    pDevice->getSourceLevelDebugger()->notifyKernelDebugData(kernelInfo);
                }
            }
        }

        separateBlockKernels();
    } while (false);

    if (retVal != CL_SUCCESS) {
        buildStatus = CL_BUILD_ERROR;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    } else {
        buildStatus = CL_BUILD_SUCCESS;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    }

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}

bool Program::appendKernelDebugOptions() {
    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::debugKernelEnable);
    CompilerOptions::concatenateAppend(options, CompilerOptions::generateDebugInfo);
    auto sourceLevelDebugger = pDevice->getSourceLevelDebugger();
    if (sourceLevelDebugger && sourceLevelDebugger->isOptimizationDisabled()) {
        CompilerOptions::concatenateAppend(options, CompilerOptions::optDisable);
    }
    return true;
}

void Program::notifyDebuggerWithSourceCode(std::string &filename) {
    if (pDevice->getSourceLevelDebugger()) {
        pDevice->getSourceLevelDebugger()->notifySourceCode(sourceCode.c_str(), sourceCode.size(), filename);
    }
}

cl_int Program::build(const Device *pDevice, const char *buildOptions, bool enableCaching,
                      std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap) {
    cl_device_id deviceId = pDevice->getSpecializedDevice<ClDevice>();
    auto ret = this->build(1, &deviceId, buildOptions, nullptr, nullptr, enableCaching);
    if (ret != CL_SUCCESS) {
        return ret;
    }

    for (auto &ki : this->kernelInfoArray) {
        auto fit = builtinsMap.find(ki->name);
        if (fit == builtinsMap.end()) {
            continue;
        }
        ki->builtinDispatchBuilder = fit->second;
    }
    return ret;
}

void Program::extractInternalOptions(const std::string &options) {
    auto tokenized = CompilerOptions::tokenize(options);
    for (auto &optionString : internalOptionsToExtract) {
        auto element = std::find(tokenized.begin(), tokenized.end(), optionString);
        if (element == tokenized.end()) {
            continue;
        }

        if (isFlagOption(optionString)) {
            CompilerOptions::concatenateAppend(internalOptions, optionString);
        } else if ((element + 1 != tokenized.end()) &&
                   isOptionValueValid(optionString, *(element + 1))) {
            CompilerOptions::concatenateAppend(internalOptions, optionString);
            CompilerOptions::concatenateAppend(internalOptions, *(element + 1));
        }
    }
}

} // namespace NEO
