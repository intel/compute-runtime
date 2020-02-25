/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/kernel/kernel_arg_metadata.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include <cinttypes>
#include <cstddef>
#include <limits>

namespace NEO {

using CrossThreadDataOffset = uint16_t;
using DynamicStateHeapOffset = uint16_t;
using SurfaceStateHeapOffset = uint16_t;

template <typename T>
static constexpr T undefined = std::numeric_limits<T>::max();

template <typename T>
bool isUndefinedOffset(T offset) {
    return undefined<T> == offset;
}

template <typename T>
bool isValidOffset(T offset) {
    return false == isUndefinedOffset<T>(offset);
}

struct ArgDescPointer final {
    SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>;
    CrossThreadDataOffset stateless = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset bindless = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset bufferOffset = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset slmOffset = undefined<CrossThreadDataOffset>;
    uint8_t requiredSlmAlignment = 0;
    uint8_t pointerSize = 0;
    bool accessedUsingStatelessAddressingMode = true;

    bool isPureStateful() const {
        return false == accessedUsingStatelessAddressingMode;
    }
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
};

struct ArgDescValue final {
    struct Element {
        CrossThreadDataOffset offset = undefined<CrossThreadDataOffset>;
        uint16_t size = 0U;
        uint16_t sourceOffset = 0U;
    };
    StackVec<Element, 1> elements;
};

struct ArgDescriptor final {
    enum ArgType : uint8_t {
        ArgTUnknown,
        ArgTPointer,
        ArgTImage,
        ArgTSampler,
        ArgTValue
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
                bool hasDeviceSideEnqueueExtendedDescriptor : 1;
            };
            uint32_t packed;
        };
    };

    ArgDescriptor(ArgType type);

    ArgDescriptor()
        : ArgDescriptor(ArgTUnknown) {
    }

    ArgDescriptor &operator=(const ArgDescriptor &rhs);
    ArgDescriptor(const ArgDescriptor &rhs) {
        *this = rhs;
    }

    template <typename T>
    const T &as() const;

    template <typename T>
    T &as(bool initIfUnknown = false);

    template <ArgType Type>
    bool is() const {
        return Type == this->type;
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
        case ArgTImage:
            return (KernelArgMetadata::AccessReadOnly == traits.accessQualifier);
        case ArgTPointer:
            return (KernelArgMetadata::AddrConstant == traits.addressQualifier) || (traits.typeQualifiers.constQual);
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
constexpr auto ArgSize = sizeof(ArgDescriptor);
static_assert(ArgSize <= 64, "Keep it small");
} // namespace

template <>
inline const ArgDescPointer &ArgDescriptor::as<ArgDescPointer>() const {
    UNRECOVERABLE_IF(type != ArgTPointer);
    return this->asPointer;
}

template <>
inline const ArgDescImage &ArgDescriptor::as<ArgDescImage>() const {
    UNRECOVERABLE_IF(type != ArgTImage);
    return this->asImage;
}

template <>
inline const ArgDescSampler &ArgDescriptor::as<ArgDescSampler>() const {
    UNRECOVERABLE_IF(type != ArgTSampler);
    return this->asSampler;
}

template <>
inline const ArgDescValue &ArgDescriptor::as<ArgDescValue>() const {
    UNRECOVERABLE_IF(type != ArgTValue);
    return this->asByValue;
}

template <>
inline ArgDescPointer &ArgDescriptor::as<ArgDescPointer>(bool initIfUnknown) {
    if ((ArgTUnknown == type) & initIfUnknown) {
        this->type = ArgTPointer;
        this->asPointer = {};
    }
    UNRECOVERABLE_IF(type != ArgTPointer);
    return this->asPointer;
}

template <>
inline ArgDescImage &ArgDescriptor::as<ArgDescImage>(bool initIfUnknown) {
    if ((ArgTUnknown == type) & initIfUnknown) {
        this->type = ArgTImage;
        this->asImage = {};
    }
    UNRECOVERABLE_IF(type != ArgTImage);
    return this->asImage;
}

template <>
inline ArgDescSampler &ArgDescriptor::as<ArgDescSampler>(bool initIfUnknown) {
    if ((ArgTUnknown == type) & initIfUnknown) {
        this->type = ArgTSampler;
        this->asSampler = {};
    }
    UNRECOVERABLE_IF(type != ArgTSampler);
    return this->asSampler;
}

template <>
inline ArgDescValue &ArgDescriptor::as<ArgDescValue>(bool initIfUnknown) {
    if ((ArgTUnknown == type) & initIfUnknown) {
        this->type = ArgTValue;
        this->asByValue = {};
    }
    UNRECOVERABLE_IF(type != ArgTValue);
    return this->asByValue;
}

template <uint32_t VecSize, typename T>
inline void setOffsetsVec(CrossThreadDataOffset (&dst)[VecSize], const T (&src)[VecSize]) {
    for (uint32_t i = 0; i < VecSize; ++i) {
        dst[i] = src[i];
    }
}

template <typename T>
inline bool patchNonPointer(ArrayRef<uint8_t> buffer, CrossThreadDataOffset location, const T &value) {
    if (undefined<CrossThreadDataOffset> == location) {
        return false;
    }
    UNRECOVERABLE_IF(location + sizeof(T) > buffer.size());
    *reinterpret_cast<T *>(buffer.begin() + location) = value;
    return true;
}

template <uint32_t VecSize, typename T>
inline uint32_t patchVecNonPointer(ArrayRef<uint8_t> buffer, const CrossThreadDataOffset (&location)[VecSize], const T (&value)[VecSize]) {
    uint32_t numPatched = 0;
    for (uint32_t i = 0; i < VecSize; ++i) {
        numPatched += patchNonPointer(buffer, location[i], value[i]) ? 1 : 0;
    }
    return numPatched;
}

inline bool patchPointer(ArrayRef<uint8_t> buffer, const ArgDescPointer &arg, uintptr_t value) {
    if (arg.pointerSize == 8) {
        return patchNonPointer(buffer, arg.stateless, static_cast<uint64_t>(value));
    } else {
        UNRECOVERABLE_IF(arg.pointerSize != 4);
        return patchNonPointer(buffer, arg.stateless, static_cast<uint32_t>(value));
    }
}

inline ArgDescriptor &ArgDescriptor::operator=(const ArgDescriptor &rhs) {
    this->type = ArgTUnknown;
    switch (rhs.type) {
    default:
        break;
    case ArgTPointer:
        this->as<ArgDescPointer>(true) = rhs.as<ArgDescPointer>();
        break;
    case ArgTImage:
        this->as<ArgDescImage>(true) = rhs.as<ArgDescImage>();
        break;
    case ArgTSampler:
        this->as<ArgDescSampler>(true) = rhs.as<ArgDescSampler>();
        break;
    case ArgTValue:
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
    case ArgTPointer:
        this->as<ArgDescPointer>(true);
        break;
    case ArgTImage:
        this->as<ArgDescImage>(true);
        break;
    case ArgTSampler:
        this->as<ArgDescSampler>(true);
        break;
    case ArgTValue:
        this->as<ArgDescValue>(true);
        break;
    }
}

struct ArgDescriptorExtended {
    virtual ~ArgDescriptorExtended() = default;
};

} // namespace NEO
