/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/registry/built_ins_registry.h"

#include <string>

namespace NEO {

static RegisterEmbeddedResource registerCopyBufferToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToBufferStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToBufferStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRect,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_rect.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferRectStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRectStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_rect_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferRectStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferRectStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_rect_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillBufferStatelessHeaplessSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillBufferStatelessHeapless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyBufferToImage3dStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_buffer_to_image3d_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImage3dToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferStatelessSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImage3dToBufferStateless,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image3d_to_buffer_stateless.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image1d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image2d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::copyImageToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_image_to_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image1d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image2d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image3d.builtin_kernel"
        ));

static RegisterEmbeddedResource registerAuxTranslationSrc(
    createBuiltinResourceName(
        EBuiltInOps::auxTranslation,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/aux_translation.builtin_kernel"
        ));

static RegisterEmbeddedResource registerCopyKernelTimestampsSrc(
    createBuiltinResourceName(
        EBuiltInOps::queryKernelTimestamps,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/copy_kernel_timestamps.builtin_kernel"
        ));

static RegisterEmbeddedResource registerFillImage1dBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::fillImage1dBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::source))
        .c_str(),
    std::string(
#include "shared/source/built_ins/kernels/fill_image1d_buffer.builtin_kernel"
        ));

} // namespace NEO
