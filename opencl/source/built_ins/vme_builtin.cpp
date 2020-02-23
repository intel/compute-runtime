/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/vme_builtin.h"

#include "shared/source/device/device.h"
#include "opencl/source/built_ins/built_in_ops_vme.h"
#include "opencl/source/built_ins/built_ins.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/built_ins/populate_built_ins.inl"
#include "opencl/source/built_ins/vme_dispatch_builder.h"
#include "opencl/source/program/program.h"

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
    std::make_tuple("block_motion_estimate_intel", blockMotionEstimateIntelSrc),
    std::make_tuple("block_advanced_motion_estimate_check_intel", blockAdvancedMotionEstimateCheckIntelSrc),
    std::make_tuple("block_advanced_motion_estimate_bidirectional_check_intel", blockAdvancedMotionEstimateBidirectionalCheckIntelSrc),
};

// Unlike other built-ins media kernels are not stored in BuiltIns object.
// Pointer to program with built in kernels is returned to the user through API
// call and user is responsible for releasing it by calling clReleaseProgram.
Program *Vme::createBuiltInProgram(
    Context &context,
    Device &device,
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
    pBuiltInProgram = Program::create(programSourceStr.c_str(), &context, device, true, nullptr);

    if (pBuiltInProgram) {
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtinsBuilders;
        builtinsBuilders["block_motion_estimate_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, device);
        builtinsBuilders["block_advanced_motion_estimate_check_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, device);
        builtinsBuilders["block_advanced_motion_estimate_bidirectional_check_intel"] =
            &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, device);

        errcodeRet = pBuiltInProgram->build(&device, mediaKernelsBuildOptions, true, builtinsBuilders);
    } else {
        errcodeRet = CL_INVALID_VALUE;
    }
    return pBuiltInProgram;
}

const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin) {
    switch (builtin) {
    default:
        return nullptr;
    case EBuiltInOps::VmeBlockMotionEstimateIntel:
        return "vme_block_motion_estimate_intel.builtin_kernel";
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel:
        return "vme_block_advanced_motion_estimate_check_intel.builtin_kernel";
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        return "vme_block_advanced_motion_estimate_bidirectional_check_intel";
    }
}

BuiltinDispatchInfoBuilder &Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, Device &device) {
    auto &builtins = *device.getExecutionEnvironment()->getBuiltIns();
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &operationBuilder = builtins.BuiltinOpsBuilders[operationId];
    switch (operation) {
    default:
        return BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(operation, device);
    case EBuiltInOps::VmeBlockMotionEstimateIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel>>(builtins, device); });
        break;
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel>>(builtins, device); });
        break;
    case EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel:
        std::call_once(operationBuilder.second, [&] { operationBuilder.first = std::make_unique<BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel>>(builtins, device); });
        break;
    }
    return *operationBuilder.first;
}
} // namespace NEO