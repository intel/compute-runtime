/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_error_mappers.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace NEO {

struct MemoryProperties;

namespace LEO {

class CommandBuffer;
class CommandQueue;
class MemObj;
class Buffer;
class Image;
class Kernel;
class Event;
class Context;
class ClDevice;
class Platform;
class Program;
class Sampler;

using EventWaitList = std::span<const cl_event>;
using DeviceList = std::span<const cl_device_id>;
using MemObjList = std::span<const cl_mem>;

enum class NonZeroBufferSize : size_t {};
enum class PatternSize : size_t {};

// TypedMemObj wrapper for type-specific mem object validation
template <typename T>
struct TypedMemObj {
    const cl_mem memObj;

    constexpr explicit TypedMemObj(cl_mem memObj) noexcept : memObj(memObj) {}
};

// Convenience type aliases
using BufferObj = TypedMemObj<Buffer>;
using ImageObj = TypedMemObj<Image>;

[[nodiscard]] inline constexpr cl_int validateObject(void *ptr) noexcept {
    return ptr != nullptr ? CL_SUCCESS : CL_INVALID_VALUE;
}

[[nodiscard]] inline constexpr cl_int validateObject(bool isValid) noexcept {
    return isValid ? CL_SUCCESS : CL_INVALID_VALUE;
}

[[nodiscard]] inline constexpr cl_int validateObject(NonZeroBufferSize size) noexcept {
    return static_cast<size_t>(size) != 0 ? CL_SUCCESS : CL_INVALID_BUFFER_SIZE;
}

[[nodiscard]] inline constexpr cl_int validateObject(PatternSize ps) noexcept {
    const auto size = static_cast<size_t>(ps);
    return (size >= 1 && size <= 128 && (size & (size - 1)) == 0)
               ? CL_SUCCESS
               : CL_INVALID_VALUE;
}

[[nodiscard]] cl_int validateObject(cl_context object) noexcept;
[[nodiscard]] cl_int validateObject(cl_device_id object) noexcept;
[[nodiscard]] cl_int validateObject(cl_platform_id object) noexcept;
[[nodiscard]] cl_int validateObject(cl_command_queue object) noexcept;
[[nodiscard]] cl_int validateObject(cl_command_buffer_khr object) noexcept;
[[nodiscard]] cl_int validateObject(cl_event object) noexcept;
[[nodiscard]] cl_int validateObject(cl_mem object) noexcept;
[[nodiscard]] cl_int validateObject(cl_sampler object) noexcept;
[[nodiscard]] cl_int validateObject(cl_program object) noexcept;
[[nodiscard]] cl_int validateObject(cl_kernel object) noexcept;

[[nodiscard]] cl_int validateObject(EventWaitList eventWaitList) noexcept;
[[nodiscard]] cl_int validateObject(DeviceList deviceList) noexcept;
[[nodiscard]] cl_int validateObject(MemObjList memObjList) noexcept;

// Validate TypedMemObj
template <typename T>
[[nodiscard]] inline cl_int validateObject(TypedMemObj<T> typedMemObj) noexcept {
    return castToObject<T>(typedMemObj.memObj) ? CL_SUCCESS : CL_INVALID_MEM_OBJECT;
}

namespace detail {

template <typename T>
struct InternalObjectTypeHelper;

template <>
struct InternalObjectTypeHelper<cl_command_queue> {
    using type = CommandQueue;
};

template <>
struct InternalObjectTypeHelper<cl_command_buffer_khr> {
    using type = CommandBuffer;
};

template <>
struct InternalObjectTypeHelper<cl_mem> {
    using type = MemObj;
};

template <>
struct InternalObjectTypeHelper<cl_kernel> {
    using type = Kernel;
};

template <>
struct InternalObjectTypeHelper<cl_event> {
    using type = Event;
};

template <>
struct InternalObjectTypeHelper<cl_context> {
    using type = Context;
};

template <>
struct InternalObjectTypeHelper<cl_device_id> {
    using type = ClDevice;
};

template <>
struct InternalObjectTypeHelper<cl_platform_id> {
    using type = Platform;
};

template <>
struct InternalObjectTypeHelper<cl_program> {
    using type = Program;
};

template <>
struct InternalObjectTypeHelper<cl_sampler> {
    using type = Sampler;
};

template <typename T>
struct InternalObjectTypeHelper<TypedMemObj<T>> {
    using type = T;
};

template <typename T>
using InternalObjectType = typename InternalObjectTypeHelper<std::decay_t<T>>::type;

template <typename InternalType, typename T>
[[nodiscard]] inline InternalType *castToInternalObject(T object) noexcept {
    return castToObject<InternalType>(object);
}

template <typename InternalType, typename T>
[[nodiscard]] inline InternalType *castToInternalObject(TypedMemObj<T> typedMemObj) noexcept {
    return castToObject<T>(typedMemObj.memObj);
}

template <typename... CastObjs>
[[nodiscard]] inline std::tuple<cl_int, InternalObjectType<CastObjs> *...> validateAndCastImpl(CastObjs... castObjs) noexcept {
    cl_int errCode = CL_SUCCESS;

    if constexpr (sizeof...(CastObjs) > 0) {
        ((errCode == CL_SUCCESS ? errCode = validateObject(castObjs) : errCode), ...);
    }

    if (errCode != CL_SUCCESS) [[unlikely]] {
        return {errCode, static_cast<InternalObjectType<CastObjs> *>(nullptr)...};
    }

    return {CL_SUCCESS, castToInternalObject<InternalObjectType<CastObjs>>(castObjs)...};
}

} // namespace detail

template <typename... CastObjs, typename... NonCastObjs>
[[nodiscard]] inline auto validateAndCast(std::tuple<CastObjs...> castObjs, std::tuple<NonCastObjs...> nonCastObjs) noexcept {
    cl_int errCode = CL_SUCCESS;

    if constexpr (sizeof...(NonCastObjs) > 0) {
        std::apply([&errCode](auto &&...args) {
            ((errCode == CL_SUCCESS ? errCode = validateObject(std::forward<decltype(args)>(args)) : errCode), ...);
        },
                   nonCastObjs);
    }

    if (errCode != CL_SUCCESS) [[unlikely]] {
        return std::tuple_cat(std::make_tuple(errCode), std::apply([](auto &&...args) { return std::make_tuple(static_cast<detail::InternalObjectType<std::decay_t<decltype(args)>> *>(nullptr)...); }, castObjs));
    }

    return std::apply([](auto &&...args) { return detail::validateAndCastImpl(std::forward<decltype(args)>(args)...); }, castObjs);
}

template <typename... CastObjs>
[[nodiscard]] inline auto validateAndCast(std::tuple<CastObjs...> castObjs) noexcept {
    return std::apply([](auto &&...args) { return detail::validateAndCastImpl(std::forward<decltype(args)>(args)...); }, castObjs);
}

// Level Zero expresses copy regions and pitches in 32 bits (ze_copy_region_t and the pitch arguments of
// zeCommandListAppendMemoryCopyRegion / zeCommandListAppendImageCopy*Ext), while OpenCL passes them as size_t.
// A value that does not fit cannot be forwarded, so it has to be rejected instead of being silently truncated
// into a transfer at the wrong offset.
[[nodiscard]] inline constexpr bool fitsInUint32(size_t value) noexcept {
    return value <= std::numeric_limits<uint32_t>::max();
}

[[nodiscard]] inline constexpr bool rectArgsFitInUint32(const size_t *origin, const size_t *region,
                                                        size_t rowPitch, size_t slicePitch) noexcept {
    return fitsInUint32(origin[0]) && fitsInUint32(origin[1]) && fitsInUint32(origin[2]) &&
           fitsInUint32(region[0]) && fitsInUint32(region[1]) && fitsInUint32(region[2]) &&
           fitsInUint32(rowPitch) && fitsInUint32(slicePitch);
}

[[nodiscard]] cl_int validateYuvOperation(const size_t *origin, const size_t *region) noexcept;
[[nodiscard]] bool isPackedYuvImage(const cl_image_format *imageFormat) noexcept;
[[nodiscard]] bool isNV12Image(const cl_image_format *imageFormat) noexcept;

[[nodiscard]] cl_int validateImageFormat(const cl_image_format *imageFormat) noexcept;

[[nodiscard]] cl_int validateStandaloneImageDescriptor(const ClDevice &device,
                                                       const MemoryProperties &memoryProperties,
                                                       cl_mem_flags flags,
                                                       const cl_image_format *imageFormat,
                                                       const cl_image_desc *imageDesc,
                                                       const void *hostPtr) noexcept;

[[nodiscard]] cl_int validateImageCopy(const cl_image_format &srcFormat,
                                       const cl_image_format &dstFormat,
                                       const size_t *srcOrigin,
                                       const size_t *dstOrigin,
                                       const size_t *region) noexcept;

} // namespace LEO
} // namespace NEO
