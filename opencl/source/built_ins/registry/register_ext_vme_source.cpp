/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/registry/built_ins_registry.h"

#include "opencl/source/built_ins/built_in_ops_vme.h"

#include <string>

namespace NEO {

static RegisterEmbeddedResource registerVmeSrc(
    createBuiltinResourceName(
        EBuiltInOps::vmeBlockMotionEstimateIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_motion_estimate_intel.builtin_kernel"
        ));

static RegisterEmbeddedResource registerVmeAdvancedSrc(
    createBuiltinResourceName(
        EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_advanced_motion_estimate_check_intel.builtin_kernel"
        ));

static RegisterEmbeddedResource registerVmeAdvancedBidirectionalSrc(
    createBuiltinResourceName(
        EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.builtin_kernel"
        ));

} // namespace NEO
