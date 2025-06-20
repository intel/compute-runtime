/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/kernel/kernel_arg_metadata.h"
#include "shared/source/utilities/arrayref.h"

#include <type_traits>

namespace NEO {

using CrossThreadDataOffset = uint16_t;
using DynamicStateHeapOffset = uint16_t;
using SurfaceStateHeapOffset = uint16_t;
using InlineDataOffset = uint8_t;

template <typename T>
static constexpr T undefined = std::numeric_limits<T>::max();

template <typename T>
constexpr bool isUndefined(T value) {
    return value == undefined<T>;
}

template <typename T>
constexpr bool isDefined(T value) {
    return value != undefined<T>;
}

template <typename T>
    requires(!std::is_pointer_v<T>)
constexpr bool isUndefinedOffset(T offset) {
    return undefined<T> == offset;
}

template <typename T>
constexpr bool isValidOffset(T offset) {
    return false == isUndefinedOffset<T>(offset);
}

struct ArgDescPointer final {
    SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>;
    CrossThreadDataOffset stateless = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset bindless = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset bufferOffset = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset bufferSize = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset slmOffset = undefined<CrossThreadDataOffset>;
    uint8_t requiredSlmAlignment = 0;
    uint8_t pointerSize = 0;
    bool accessedUsingStatelessAddressingMode = true;

    bool isPureStateful() const {
        return false == accessedUsingStatelessAddressingMode;
    }
};

struct ArgDescInlineDataPointer {
    InlineDataOffset offset = undefined<InlineDataOffset>;
    uint8_t pointerSize = undefined<uint8_t>;
};

enum class NEOImageType : uint8_t {
    imageTypeUnknown,
    imageTypeBuffer,
    imageType1D,
    imageType1DArray,
    imageType2D,
    imageType2DArray,
    imageType3D,
    imageTypeCube,
    imageTypeCubeArray,
    imageType2DDepth,
    imageType2DArrayDepth,
    imageType2DMSAA,
    imageType2DMSAADepth,
    imageType2DArrayMSAA,
    imageType2DArrayMSAADepth,
    imageType2DMedia,
    imageType2DMediaBlock,
};

struct ArgDescImage final {
    SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>; // stateful with BTI
    CrossThreadDataOffset bindless = undefined<CrossThreadDataOffset>;
    struct {
        CrossThreadDataOffset imgWidth = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset imgHeight = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset imgDepth = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset channelDataType = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset channelOrder = undefined<CrossThreadDataOffset>;

        CrossThreadDataOffset arraySize = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset numSamples = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset numMipLevels = undefined<CrossThreadDataOffset>;

        CrossThreadDataOffset flatBaseOffset = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset flatWidth = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset flatHeight = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset flatPitch = undefined<CrossThreadDataOffset>;
    } metadataPayload;
    NEOImageType imageType;
    uint8_t size = undefined<uint8_t>;
};

struct ArgDescSampler final {
    uint32_t samplerType = 0;
    DynamicStateHeapOffset bindful = undefined<DynamicStateHeapOffset>;
    CrossThreadDataOffset bindless = undefined<CrossThreadDataOffset>;
    struct {
        CrossThreadDataOffset samplerSnapWa = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset samplerAddressingMode = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset samplerNormalizedCoords = undefined<CrossThreadDataOffset>;
    } metadataPayload;
    uint8_t index = undefined<uint8_t>;
    uint8_t size = undefined<uint8_t>;
};

struct ArgDescValue final {
    struct Element {
        CrossThreadDataOffset offset = undefined<CrossThreadDataOffset>;
        uint16_t size = 0U;
        uint16_t sourceOffset = 0U;
        bool isPtr = false;
    };
    StackVec<Element, 1> elements;
};

struct ArgDescriptor final {
    enum ArgType : uint8_t {
        argTUnknown,
        argTPointer,
        argTImage,
        argTSampler,
        argTValue
    };

    struct ExtendedTypeInfo {
        ExtendedTypeInfo() {
            packed = 0U;
        }
        union {
            struct {
                bool isAccelerator : 1;
                bool isDeviceQueue : 1;
                bool isMediaImage : 1;
                bool isMediaBlockImage : 1;
                bool isTransformable : 1;
                bool needsPatch : 1;
                bool hasVmeExtendedDescriptor : 1;
            };
            uint32_t packed;
        };
    };

    ArgDescriptor(ArgType type);

    ArgDescriptor()
        : ArgDescriptor(argTUnknown) {
    }

    ArgDescriptor &operator=(const ArgDescriptor &rhs);
    ArgDescriptor(const ArgDescriptor &rhs) {
        *this = rhs;
    }

    template <typename T>
    const T &as() const;

    template <typename T>
    T &as(bool initIfUnknown = false);

    template <ArgType type>
    bool is() const {
        return type == this->type;
    }

    ArgTypeTraits &getTraits() {
        return traits;
    }

    const ArgTypeTraits &getTraits() const {
        return traits;
    }

    ExtendedTypeInfo &getExtendedTypeInfo() {
        return extendedTypeInfo;
    }

    const ExtendedTypeInfo &getExtendedTypeInfo() const {
        return extendedTypeInfo;
    }

    bool isReadOnly() const {
        switch (type) {
        default:
            return true;
        case argTImage:
            return (KernelArgMetadata::AccessReadOnly == traits.accessQualifier);
        case argTPointer:
            return (KernelArgMetadata::AddrConstant == traits.addressQualifier) ||
                   (KernelArgMetadata::AccessReadOnly == traits.accessQualifier) ||
                   traits.typeQualifiers.constQual;
        }
    }

  protected:
    ArgDescValue asByValue;
    ArgTypeTraits traits;
    union {
        ArgDescPointer asPointer;
        ArgDescImage asImage;
        ArgDescSampler asSampler;
    };

    ExtendedTypeInfo extendedTypeInfo;

  public:
    ArgType type;
};

namespace {
constexpr auto argSize = sizeof(ArgDescriptor);
static_assert(argSize <= 72, "Keep it small");
} // namespace

template <>
inline const ArgDescPointer &ArgDescriptor::as<ArgDescPointer>() const {
    UNRECOVERABLE_IF(type != argTPointer);
    return this->asPointer;
}

template <>
inline const ArgDescImage &ArgDescriptor::as<ArgDescImage>() const {
    UNRECOVERABLE_IF(type != argTImage);
    return this->asImage;
}

template <>
inline const ArgDescSampler &ArgDescriptor::as<ArgDescSampler>() const {
    UNRECOVERABLE_IF(type != argTSampler);
    return this->asSampler;
}

template <>
inline const ArgDescValue &ArgDescriptor::as<ArgDescValue>() const {
    UNRECOVERABLE_IF(type != argTValue);
    return this->asByValue;
}

template <>
inline ArgDescPointer &ArgDescriptor::as<ArgDescPointer>(bool initIfUnknown) {
    if ((argTUnknown == type) && initIfUnknown) {
        this->type = argTPointer;
        this->asPointer = {};
    }
    UNRECOVERABLE_IF(type != argTPointer);
    return this->asPointer;
}

template <>
inline ArgDescImage &ArgDescriptor::as<ArgDescImage>(bool initIfUnknown) {
    if ((argTUnknown == type) && initIfUnknown) {
        this->type = argTImage;
        this->asImage = {};
    }
    UNRECOVERABLE_IF(type != argTImage);
    return this->asImage;
}

template <>
inline ArgDescSampler &ArgDescriptor::as<ArgDescSampler>(bool initIfUnknown) {
    if ((argTUnknown == type) && initIfUnknown) {
        this->type = argTSampler;
        this->asSampler = {};
    }
    UNRECOVERABLE_IF(type != argTSampler);
    return this->asSampler;
}

template <>
inline ArgDescValue &ArgDescriptor::as<ArgDescValue>(bool initIfUnknown) {
    if ((argTUnknown == type) && initIfUnknown) {
        this->type = argTValue;
        this->asByValue.elements.clear();
    }
    UNRECOVERABLE_IF(type != argTValue);
    return this->asByValue;
}

template <uint32_t vecSize, typename T>
inline void setOffsetsVec(CrossThreadDataOffset (&dst)[vecSize], const T (&src)[vecSize]) {
    for (uint32_t i = 0; i < vecSize; ++i) {
        dst[i] = src[i];
    }
}

template <typename DstT, typename SrcT>
inline bool patchNonPointer(ArrayRef<uint8_t> buffer, CrossThreadDataOffset location, const SrcT &value) {
    if (undefined<CrossThreadDataOffset> == location) {
        return false;
    }
    DEBUG_BREAK_IF(location + sizeof(DstT) > buffer.size());
    *reinterpret_cast<DstT *>(buffer.begin() + location) = static_cast<DstT>(value);
    return true;
}

template <uint32_t vecSize, typename T>
inline void patchVecNonPointer(ArrayRef<uint8_t> buffer, const CrossThreadDataOffset (&location)[vecSize], const T (&value)[vecSize]) {
    for (uint32_t i = 0; i < vecSize; ++i) {
        patchNonPointer<T, T>(buffer, location[i], value[i]);
    }
    return;
}

inline bool patchPointer(ArrayRef<uint8_t> buffer, const ArgDescPointer &arg, uintptr_t value) {
    if (arg.pointerSize == 8) {
        return patchNonPointer<uint64_t, uint64_t>(buffer, arg.stateless, static_cast<uint64_t>(value));
    } else {
        UNRECOVERABLE_IF((arg.pointerSize != 4) && isValidOffset(arg.stateless));
        return patchNonPointer<uint32_t, uint32_t>(buffer, arg.stateless, static_cast<uint32_t>(value));
    }
}

inline ArgDescriptor &ArgDescriptor::operator=(const ArgDescriptor &rhs) {
    if (this == &rhs) {
        return *this;
    }
    this->type = argTUnknown;
    switch (rhs.type) {
    default:
        break;
    case argTPointer:
        this->as<ArgDescPointer>(true) = rhs.as<ArgDescPointer>();
        break;
    case argTImage:
        this->as<ArgDescImage>(true) = rhs.as<ArgDescImage>();
        break;
    case argTSampler:
        this->as<ArgDescSampler>(true) = rhs.as<ArgDescSampler>();
        break;
    case argTValue:
        this->as<ArgDescValue>(true) = rhs.as<ArgDescValue>();
        break;
    }
    this->type = rhs.type;
    this->traits = rhs.traits;
    this->extendedTypeInfo = rhs.extendedTypeInfo;
    return *this;
}

inline ArgDescriptor::ArgDescriptor(ArgType type) : type(type) {
    switch (type) {
    default:
        break;
    case argTPointer:
        this->as<ArgDescPointer>(true);
        break;
    case argTImage:
        this->as<ArgDescImage>(true);
        break;
    case argTSampler:
        this->as<ArgDescSampler>(true);
        break;
    case argTValue:
        this->as<ArgDescValue>(true);
        break;
    }
}

struct ArgDescriptorExtended {
    virtual ~ArgDescriptorExtended() = default;
};

} // namespace NEO
