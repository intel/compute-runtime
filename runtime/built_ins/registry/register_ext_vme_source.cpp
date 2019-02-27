/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/registry/built_ins_registry.h"

#include <string>

namespace OCLRT {

static RegisterEmbeddedResource registerVmeSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockMotionEstimateIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/vme_block_motion_estimate_intel.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerVmeAdvancedSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerVmeAdvancedBidirectionalSrc(
    createBuiltinResourceName(
        EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/vme_block_advanced_motion_estimate_bidirectional_check_intel.igdrcl_built_in"
        ));

} // namespace OCLRT
