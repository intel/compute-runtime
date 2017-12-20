/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string>
#include "runtime/built_ins/registry/built_ins_registry.h"

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
