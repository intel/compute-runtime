/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

#include <cstdint>
#include <limits>
#include <string>

namespace NEO {
namespace BuiltIn {

struct AddressingMode {
    enum class ImageMode : uint8_t { bindful,
                                     bindless };

    enum class BufferMode : uint8_t { bindful,
                                      bindless,
                                      stateless };

    ImageMode imageMode = ImageMode::bindless;
    BufferMode bufferMode = BufferMode::stateless;
    bool wideMode = false;

    constexpr AddressingMode() = default;
    constexpr AddressingMode(ImageMode img, BufferMode buf, bool wide)
        : imageMode(img), bufferMode(buf), wideMode(wide) {}

    static constexpr AddressingMode getDefaultMode(bool useBindless, bool useStateless) {
        AddressingMode mode;
        mode.imageMode = useBindless ? ImageMode::bindless : ImageMode::bindful;
        mode.bufferMode = useStateless ? BufferMode::stateless : (useBindless ? BufferMode::bindless : BufferMode::bindful);
        return mode;
    }

    void adjustToWideStatelessIfRequired(uint64_t maxBufferSize) {
        if (maxBufferSize >= 4ull * MemoryConstants::gigaByte) {
            this->bufferMode = BufferMode::stateless;
            this->wideMode = true;
        }
    }

    std::string toString() const {
        std::string mode;

        if (imageMode == ImageMode::bindless) {
            mode += "bindless_image_";
        } else {
            mode += "bindful_image_";
        }

        if (bufferMode == BufferMode::stateless) {
            mode += "stateless_buffer";
        } else if (bufferMode == BufferMode::bindless) {
            mode += "bindless_buffer";
        } else {
            mode += "bindful_buffer";
        }

        if (wideMode) {
            mode += "_wide";
        }

        mode += "_";

        return mode;
    }

    constexpr uint32_t toIndex() const;

    bool operator==(const AddressingMode &other) const = default;
};

inline constexpr AddressingMode bindfulImageBindfulBuffer{AddressingMode::ImageMode::bindful, AddressingMode::BufferMode::bindful, false};
inline constexpr AddressingMode bindfulImageStatelessBuffer{AddressingMode::ImageMode::bindful, AddressingMode::BufferMode::stateless, false};
inline constexpr AddressingMode bindfulImageStatelessBufferWide{AddressingMode::ImageMode::bindful, AddressingMode::BufferMode::stateless, true};
inline constexpr AddressingMode bindlessImageBindlessBuffer{AddressingMode::ImageMode::bindless, AddressingMode::BufferMode::bindless, false};
inline constexpr AddressingMode bindlessImageStatelessBuffer{AddressingMode::ImageMode::bindless, AddressingMode::BufferMode::stateless, false};
inline constexpr AddressingMode bindlessImageStatelessBufferWide{AddressingMode::ImageMode::bindless, AddressingMode::BufferMode::stateless, true};

inline constexpr AddressingMode allAddressingModes[] = {
    bindfulImageBindfulBuffer,
    bindfulImageStatelessBuffer,
    bindfulImageStatelessBufferWide,
    bindlessImageBindlessBuffer,
    bindlessImageStatelessBuffer,
    bindlessImageStatelessBufferWide};

inline constexpr uint32_t addressingModeCount = 6u;

constexpr uint32_t AddressingMode::toIndex() const {
    for (uint32_t i = 0u; i < addressingModeCount; ++i) {
        if (allAddressingModes[i] == *this) {
            return i;
        }
    }
    return std::numeric_limits<uint32_t>::max();
}

enum class BaseKernel : uint32_t {
    copyBufferToBuffer = 0,
    copyBufferRect,
    fillBuffer,
    copyBufferToImage3d,
    copyImage3dToBuffer,
    copyImageToImage1d,
    copyImageToImage2d,
    copyImageToImage3d,
    fillImage1d,
    fillImage2d,
    fillImage3d,
    fillImage1dBuffer,
    copyKernelTimestamps,
    auxTranslation,
    count
};

struct BuiltInId {
    BaseKernel kernel;
    AddressingMode mode;
};

constexpr uint32_t toIndex(BaseKernel kernel) { return static_cast<uint32_t>(kernel); }
constexpr uint32_t builderIndex(BaseKernel kernel, const AddressingMode &mode) {
    return toIndex(kernel) * addressingModeCount + mode.toIndex();
}
constexpr uint32_t maxBuilderIndex() { return static_cast<uint32_t>(BaseKernel::count) * addressingModeCount; }

constexpr const char *getAsString(BaseKernel kernel) {
    switch (kernel) {
    case BaseKernel::copyBufferToBuffer:
        return "copy_buffer_to_buffer.builtin_kernel";
    case BaseKernel::copyBufferRect:
        return "copy_buffer_rect.builtin_kernel";
    case BaseKernel::fillBuffer:
        return "fill_buffer.builtin_kernel";
    case BaseKernel::copyBufferToImage3d:
        return "copy_buffer_to_image3d.builtin_kernel";
    case BaseKernel::copyImage3dToBuffer:
        return "copy_image3d_to_buffer.builtin_kernel";
    case BaseKernel::copyImageToImage1d:
        return "copy_image_to_image1d.builtin_kernel";
    case BaseKernel::copyImageToImage2d:
        return "copy_image_to_image2d.builtin_kernel";
    case BaseKernel::copyImageToImage3d:
        return "copy_image_to_image3d.builtin_kernel";
    case BaseKernel::fillImage1d:
        return "fill_image1d.builtin_kernel";
    case BaseKernel::fillImage2d:
        return "fill_image2d.builtin_kernel";
    case BaseKernel::fillImage3d:
        return "fill_image3d.builtin_kernel";
    case BaseKernel::fillImage1dBuffer:
        return "fill_image1d_buffer.builtin_kernel";
    case BaseKernel::copyKernelTimestamps:
        return "copy_kernel_timestamps.builtin_kernel";
    case BaseKernel::auxTranslation:
        return "aux_translation.builtin_kernel";
    default:
        return "unknown";
    }
}

} // namespace BuiltIn
} // namespace NEO
