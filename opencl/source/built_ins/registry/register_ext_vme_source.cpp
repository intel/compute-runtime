/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/built_ins/built_in_ops_vme.h"
#include "opencl/source/built_ins/registry/built_ins_registry.h"

#include <string>

namespace NEO {

static RegisterEmbeddedResource registerVmeSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockMotionEstimateIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_motion_estimate_intel.builtin_kernel"
        ));

static RegisterEmbeddedResource registerVmeAdvancedSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_advanced_motion_estimate_check_intel.builtin_kernel"
        ));

static RegisterEmbeddedResource registerVmeAdvancedBidirectionalSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "opencl/source/built_ins/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.builtin_kernel"
        ));

} // namespace NEO
