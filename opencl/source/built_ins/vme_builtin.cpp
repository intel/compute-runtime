/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/vme_builtin.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/device/device.h"

#include "opencl/source/built_ins/built_in_ops_vme.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/built_ins/populate_built_ins.inl"
#include "opencl/source/built_ins/vme_dispatch_builder.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/program/program.h"

#include <sstream>

namespace NEO {

static const char *blockMotionEstimateIntelSrc = {
#include "kernels/vme_block_motion_estimate_intel_frontend.builtin_kernel"
};

static const char *blockAdvancedMotionEstimateCheckIntelSrc = {
#include "kernels/vme_block_advanced_motion_estimate_check_intel_frontend.builtin_kernel"
};

static const char *blockAdvancedMotionEstimateBidirectionalCheckIntelSrc = {
#include "kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel_frontend.builtin_kernel"
};

static const std::tuple<const char *, const char *> mediaBuiltIns[] = {
    {"block_motion_estimate_intel", blockMotionEstimateIntelSrc},
    {"block_advanced_motion_estimate_check_intel", blockAdvancedMotionEstimateCheckIntelSrc},
    {"block_advanced_motion_estimate_bidirectional_check_intel", blockAdvancedMotionEstimateBidirectionalCheckIntelSrc}};

// Unlike other built-ins media kernels are not stored in BuiltIns object.
// Pointer to program with built in kernels is returned to the user through API
// call and user is responsible for releasing it by calling clReleaseProgram.
Program *Vme::createBuiltInProgram(
    Context &context,
    const ClDeviceVector &deviceVector,
    const char *kernelNames,
    int &errcodeRet) {
    std::string programSourceStr = "";
    std::istringstream ss(kernelNames);
    std::string currentKernelName;

    while (std::getline(ss, currentKernelName, ';')) {
        bool found = false;
        for (auto &builtInTuple : mediaBuiltIns) {
            if (currentKernelName == std::get<0>(builtInTuple)) {
                programSourceStr += std::get<1>(builtInTuple);
                found = true;
                break;
            }
        }
        if (!found) {
            errcodeRet = CL_INVALID_VALUE;
            return nullptr;
        }
    }
    if (programSourceStr.empty() == true) {
        errcodeRet = CL_INVALID_VALUE;
        return nullptr;
    }

    Program *pBuiltInProgram = nullptr;
    pBuiltInProgram = Program::createBuiltInFromSource(programSourceStr.c_str(), &context, deviceVector, nullptr);

    auto &device = *deviceVector[0];

    if (pBuiltInProgram) {
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtinsBuilders;
        builtinsBuilders["block_motion_estimate_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::vmeBlockMotionEstimateIntel, device);
        builtinsBuilders["block_advanced_motion_estimate_check_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, device);
        builtinsBuilders["block_advanced_motion_estimate_bidirectional_check_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, device);

        errcodeRet = pBuiltInProgram->build(deviceVector, mediaKernelsBuildOptions, builtinsBuilders);
    } else {
        errcodeRet = CL_INVALID_VALUE;
    }
    return pBuiltInProgram;
}

const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin) {
    switch (builtin) {
    default:
        return nullptr;
    case EBuiltInOps::vmeBlockMotionEstimateIntel:
        return "vme_block_motion_estimate_intel.builtin_kernel";
    case EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel:
        return "vme_block_advanced_motion_estimate_check_intel.builtin_kernel";
    case EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        return "vme_block_advanced_motion_estimate_bidirectional_check_intel";
    }
}

BuiltinDispatchInfoBuilder &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, ClDevice &device) {
    auto &builtins = *device.getDevice().getBuiltIns();
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto clExecutionEnvironment = static_cast<ClExecutionEnvironment *>(device.getExecutionEnvironment());
    auto &operationBuilder = clExecutionEnvironment->peekBuilders(device.getRootDeviceIndex())[operationId];
    switch (operation) {
    default:
        return BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(operation, device);
    case EBuiltInOps::vmeBlockMotionEstimateIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::vmeBlockMotionEstimateIntel>>(builtins, device); });
        break;
    case EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel>>(builtins, device); });
        break;
    case EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel>>(builtins, device); });
        break;
    }
    return *operationBuilder.first;
}
} // namespace NEO
