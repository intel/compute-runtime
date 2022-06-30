/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/registry/built_ins_registry.h"

#include <string>

namespace NEO {

static RegisterEmbeddedResource registerCopyBufferToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferRect,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferRectStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferRectStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_rect_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToImage3dStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImage3dToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImage3dToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image1d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image2d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image1d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image2d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerAuxTranslationSrc(
    createBuiltinResourceName(
        EBuiltInOps::AuxTranslation,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/aux_translation.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyKernelTimestampsSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_kernel_timestamps.builtin_kernel"
        ));

} // namespace NEO
