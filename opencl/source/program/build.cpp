/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/utilities/logger.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/program/program.h"

namespace NEO {

cl_int Program::build(
    const ClDeviceVector &deviceVector,
    const char *buildOptions) {
    cl_int retVal = CL_SUCCESS;
    auto internalOptions = getInternalOptions();
    auto defaultClDevice = deviceVector[0];
    UNRECOVERABLE_IF(defaultClDevice == nullptr);
    auto &defaultDevice = defaultClDevice->getDevice();

    std::unordered_map<uint32_t, BuildPhase> phaseReached;
    for (const auto &clDevice : deviceVector) {
        phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::init;
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

            if (false == requiresRebuild) {
                if (nullptr != buildOptions) {
                    options = buildOptions;
                } else if (this->createdFrom != CreatedFrom::binary) {
                    options = "";
                }
            }

            const bool shouldSuppressRebuildWarning{CompilerOptions::extract(CompilerOptions::noRecompiledFromIr, options)};
            extractInternalOptions(options, internalOptions);
            CompilerOptions::applyAdditionalApiOptions(options);
            CompilerOptions::applyAdditionalInternalOptions(internalOptions);

            CompilerInterface *pCompilerInterface = defaultDevice.getCompilerInterface();
            if (!pCompilerInterface) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
            if (createdFrom != CreatedFrom::source) {
                inputArgs.srcType = (intermediateRepresentation != IGC::CodeType::invalid)
                                        ? intermediateRepresentation
                                        : (isSpirV ? IGC::CodeType::spirV : IGC::CodeType::llvmBc);
                inputArgs.src = ArrayRef<const char>(irBinary.get(), irBinarySize);
            } else {
                inputArgs.src = ArrayRef<const char>(sourceCode.c_str(), sourceCode.size());
            }

            if (inputArgs.src.size() == 0) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            std::string extensions = requiresOpenClCFeatures(options) ? defaultClDevice->peekCompilerExtensionsWithFeatures()
                                                                      : defaultClDevice->peekCompilerExtensions();

            appendAdditionalExtensions(extensions, options, internalOptions);
            CompilerOptions::concatenateAppend(internalOptions, extensions);

            auto ailHelper = defaultDevice.getRootDeviceEnvironment().getAILConfigurationHelper();
            if (ailHelper && ailHelper->handleDivergentBarriers()) {
                CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableDivergentBarriers);
            }

            if (!this->getIsBuiltIn() && debugManager.flags.InjectInternalBuildOptions.get() != "unk") {
                NEO::CompilerOptions::concatenateAppend(internalOptions, NEO::debugManager.flags.InjectInternalBuildOptions.get());
            }

            inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
            inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
            inputArgs.gtPinInput = gtpinGetIgcInit();
            inputArgs.specializedValues = this->specConstantsValues;
            DBG_LOG(LogApiCalls,
                    "Build Options", inputArgs.apiOptions.begin(),
                    "\nBuild Internal Options", inputArgs.internalOptions.begin());
            NEO::TranslationOutput compilerOuput = {};

            for (const auto &clDevice : deviceVector) {
                if (requiresRebuild && !shouldSuppressRebuildWarning) {
                    this->updateBuildLog(clDevice->getRootDeviceIndex(), CompilerWarnings::recompiledFromIr.data(), CompilerWarnings::recompiledFromIr.length());
                }
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
                this->buildInfos[clDevice->getRootDeviceIndex()].debugData = std::move(compilerOuput.debugData.mem);
                this->buildInfos[clDevice->getRootDeviceIndex()].debugDataSize = compilerOuput.debugData.size;
                if (BuildPhase::binaryCreation == phaseReached[clDevice->getRootDeviceIndex()]) {
                    continue;
                }
                this->replaceDeviceBinary(std::move(compilerOuput.deviceBinary.mem), compilerOuput.deviceBinary.size, clDevice->getRootDeviceIndex());
                phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::binaryCreation;
            }
            if (retVal != CL_SUCCESS) {
                break;
            }
        }
        updateNonUniformFlag();

        retVal = processGenBinaries(deviceVector, phaseReached);

        auto skipLastExplicitArg = isGTPinInitialized;
        auto containsStatefulAccess = AddressingModeHelper::containsStatefulAccess(buildInfos[clDevices[0]->getRootDeviceIndex()].kernelInfoArray, skipLastExplicitArg);
        auto isUserKernel = !isBuiltIn;

        auto failBuildProgram = containsStatefulAccess &&
                                isUserKernel &&
                                AddressingModeHelper::failBuildProgramWithStatefulAccess(clDevices[0]->getRootDeviceEnvironment()) &&
                                isGeneratedByIgc;

        if (failBuildProgram) {
            retVal = CL_BUILD_PROGRAM_FAILURE;
        }

        if (retVal != CL_SUCCESS) {
            break;
        }

        if (gtpinIsGTPinInitialized()) {
            debugNotify(deviceVector, phaseReached);
        }
        notifyModuleCreate();
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

cl_int Program::build(const ClDeviceVector &deviceVector, const char *buildOptions,
                      std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap) {
    auto ret = this->build(deviceVector, buildOptions);
    if (ret != CL_SUCCESS) {
        return ret;
    }

    for (auto &ki : buildInfos[deviceVector[0]->getRootDeviceIndex()].kernelInfoArray) {
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

void Program::debugNotify(const ClDeviceVector &deviceVector, std::unordered_map<uint32_t, BuildPhase> &phasesReached) {
    for (auto &clDevice : deviceVector) {
        auto rootDeviceIndex = clDevice->getRootDeviceIndex();
        if (BuildPhase::debugDataNotification == phasesReached[rootDeviceIndex]) {
            continue;
        }
        createDebugData(clDevice);
        phasesReached[rootDeviceIndex] = BuildPhase::debugDataNotification;
    }
}

} // namespace NEO
