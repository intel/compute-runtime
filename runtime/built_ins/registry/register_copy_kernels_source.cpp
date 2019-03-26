/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/registry/built_ins_registry.h"

#include <string>

namespace NEO {

static RegisterEmbeddedResource registerCopyBufferToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_to_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferRect,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_rect.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_to_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImage3dToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image3d_to_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image1d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image2d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image1d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image2d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerAuxTranslationSrc(
    createBuiltinResourceName(
        EBuiltInOps::AuxTranslation,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/aux_translation.igdrcl_built_in"
        ));

} // namespace NEO
