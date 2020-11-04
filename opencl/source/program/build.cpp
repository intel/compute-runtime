/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/time_measure_wrapper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/validators.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/program.h"
#include "opencl/source/utilities/logger.h"

#include "compiler_options.h"

#include <cstring>
#include <iterator>
#include <sstream>

namespace NEO {

cl_int Program::build(
    const ClDeviceVector &deviceVector,
    const char *buildOptions,
    bool enableCaching) {
    cl_int retVal = CL_SUCCESS;
    std::string internalOptions;
    initInternalOptions(internalOptions);
    auto defaultClDevice = deviceVector[0];
    UNRECOVERABLE_IF(defaultClDevice == nullptr);
    auto &defaultDevice = defaultClDevice->getDevice();

    enum class BuildPhase {
        Init,
        SourceCodeNotification,
        BinaryCreation,
        BinaryProcessing,
        DebugDataNotification
    };

    std::unordered_map<uint32_t, BuildPhase> phaseReached;
    for (const auto &clDevice : deviceVector) {
        phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::Init;
    }
    do {
        // check to see if a previous build request is in progress
        if (std::any_of(deviceVector.begin(), deviceVector.end(), [&](auto device) { return CL_BUILD_IN_PROGRESS == deviceBuildInfos[device].buildStatus; })) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        if (isCreatedFromBinary == false) {
            for (const auto &device : deviceVector) {
                deviceBuildInfos[device].buildStatus = CL_BUILD_IN_PROGRESS;
            }

            if (nullptr != buildOptions) {
                options = buildOptions;
            } else if (this->createdFrom != CreatedFrom::BINARY) {
                options = "";
            }
            extractInternalOptions(options, internalOptions);
            applyAdditionalOptions(internalOptions);

            CompilerInterface *pCompilerInterface = defaultDevice.getCompilerInterface();
            if (!pCompilerInterface) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
            if (createdFrom != CreatedFrom::SOURCE) {
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
                for (const auto &clDevice : deviceVector) {
                    if (BuildPhase::SourceCodeNotification == phaseReached[clDevice->getRootDeviceIndex()]) {
                        continue;
                    }
                    appendKernelDebugOptions(*clDevice, internalOptions);
                    notifyDebuggerWithSourceCode(*clDevice, filename);
                    if (!filename.empty()) {
                        // Add "-s" flag first so it will be ignored by clang in case the options already have this flag set.
                        options = std::string("-s ") + filename + " " + options;
                    }

                    phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::SourceCodeNotification;
                }
            }

            if (requiresOpenClCFeatures(options)) {
                auto compilerExtensionsWithFeaturesOptions = defaultClDevice->peekCompilerExtensionsWithFeatures();
                CompilerOptions::concatenateAppend(internalOptions, compilerExtensionsWithFeaturesOptions);
                auto compilerFeaturesOptions = defaultClDevice->peekCompilerFeatures();
                CompilerOptions::concatenateAppend(internalOptions, compilerFeaturesOptions);
            } else {
                auto compilerExtensionsOptions = defaultClDevice->peekCompilerExtensions();
                CompilerOptions::concatenateAppend(internalOptions, compilerExtensionsOptions);
            }

            inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
            inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
            inputArgs.GTPinInput = gtpinGetIgcInit();
            inputArgs.specializedValues = this->specConstantsValues;
            DBG_LOG(LogApiCalls,
                    "Build Options", inputArgs.apiOptions.begin(),
                    "\nBuild Internal Options", inputArgs.internalOptions.begin());
            inputArgs.allowCaching = enableCaching;
            NEO::TranslationOutput compilerOuput = {};

            for (const auto &clDevice : deviceVector) {
                auto compilerErr = pCompilerInterface->build(clDevice->getDevice(), inputArgs, compilerOuput);
                this->updateBuildLog(clDevice->getRootDeviceIndex(), compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
                this->updateBuildLog(clDevice->getRootDeviceIndex(), compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
                retVal = asClError(compilerErr);
                if (retVal != CL_SUCCESS) {
                    break;
                }
                if (inputArgs.srcType == IGC::CodeType::oclC) {
                    this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
                    this->irBinarySize = compilerOuput.intermediateRepresentation.size;
                    this->isSpirV = compilerOuput.intermediateCodeType == IGC::CodeType::spirV;
                }
                if (BuildPhase::BinaryCreation == phaseReached[clDevice->getRootDeviceIndex()]) {
                    continue;
                }
                this->replaceDeviceBinary(std::move(compilerOuput.deviceBinary.mem), compilerOuput.deviceBinary.size, clDevice->getRootDeviceIndex());
                phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::BinaryCreation;
            }
            if (retVal != CL_SUCCESS) {
                break;
            }
            this->debugData = std::move(compilerOuput.debugData.mem);
            this->debugDataSize = compilerOuput.debugData.size;
        }
        updateNonUniformFlag();

        for (auto &clDevice : deviceVector) {
            if (BuildPhase::BinaryProcessing == phaseReached[clDevice->getRootDeviceIndex()]) {
                continue;
            }
            if (DebugManager.flags.PrintProgramBinaryProcessingTime.get()) {
                retVal = TimeMeasureWrapper::functionExecution(*this, &Program::processGenBinary, clDevice->getRootDeviceIndex());
            } else {
                retVal = processGenBinary(clDevice->getRootDeviceIndex());
            }

            if (retVal != CL_SUCCESS) {
                break;
            }
            phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::BinaryProcessing;
        }

        if (retVal != CL_SUCCESS) {
            break;
        }

        if (isKernelDebugEnabled()) {
            processDebugData();

            for (auto &clDevice : deviceVector) {
                if (BuildPhase::DebugDataNotification == phaseReached[clDevice->getRootDeviceIndex()]) {
                    continue;
                }
                if (clDevice->getSourceLevelDebugger()) {
                    for (auto kernelInfo : kernelInfoArray) {
                        clDevice->getSourceLevelDebugger()->notifyKernelDebugData(&kernelInfo->debugData,
                                                                                  kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
                                                                                  kernelInfo->heapInfo.pKernelHeap,
                                                                                  kernelInfo->heapInfo.KernelHeapSize);
                    }
                }
                phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::DebugDataNotification;
            }
        }

        separateBlockKernels();
    } while (false);

    if (retVal != CL_SUCCESS) {
        for (const auto &device : deviceVector) {
            deviceBuildInfos[device].buildStatus = CL_BUILD_ERROR;
            deviceBuildInfos[device].programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
        }
    } else {
        setBuildStatusSuccess(deviceVector, CL_PROGRAM_BINARY_TYPE_EXECUTABLE);
    }

    return retVal;
}

bool Program::appendKernelDebugOptions(ClDevice &clDevice, std::string &internalOptions) {
    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::debugKernelEnable);
    CompilerOptions::concatenateAppend(options, CompilerOptions::generateDebugInfo);

    auto debugger = clDevice.getSourceLevelDebugger();
    if (debugger && debugger->isOptimizationDisabled()) {
        CompilerOptions::concatenateAppend(options, CompilerOptions::optDisable);
    }
    return true;
}

void Program::notifyDebuggerWithSourceCode(ClDevice &clDevice, std::string &filename) {
    if (clDevice.getSourceLevelDebugger()) {
        clDevice.getSourceLevelDebugger()->notifySourceCode(sourceCode.c_str(), sourceCode.size(), filename);
    }
}

cl_int Program::build(const ClDeviceVector &deviceVector, const char *buildOptions, bool enableCaching,
                      std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap) {
    auto ret = this->build(deviceVector, buildOptions, enableCaching);
    if (ret != CL_SUCCESS) {
        return ret;
    }

    for (auto &ki : this->kernelInfoArray) {
        auto fit = builtinsMap.find(ki->kernelDescriptor.kernelMetadata.kernelName);
        if (fit == builtinsMap.end()) {
            continue;
        }
        ki->builtinDispatchBuilder = fit->second;
    }
    return ret;
}

void Program::extractInternalOptions(const std::string &options, std::string &internalOptions) {
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
