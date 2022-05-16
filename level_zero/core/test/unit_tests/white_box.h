/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {
namespace ult {

template <typename Type>
struct WhiteBox : public Type {
    using Type::Type;
};

template <typename Type>
WhiteBox<Type> *whiteboxCast(Type *obj) {
    return static_cast<WhiteBox<Type> *>(obj);
}

template <typename Type>
WhiteBox<Type> &whiteboxCast(Type &obj) {
    return static_cast<WhiteBox<Type> &>(obj);
}

template <typename Type>
Type *blackboxCast(WhiteBox<Type> *obj) {
    return static_cast<Type *>(obj);
}

} // namespace ult
} // namespace L0
