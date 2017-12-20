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

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

template <typename DataType>
class ArrayRef {
  public:
    template <typename IteratorType>
    ArrayRef(IteratorType b, IteratorType e)
        : b(&*b), e(&*(e - 1) + 1) {
    }

    template <typename IteratorType>
    ArrayRef(IteratorType b, size_t s)
        : ArrayRef(b, b + s) {
    }

    template <typename SequentialContainerType>
    ArrayRef(SequentialContainerType &ctr)
        : b(&*ctr.begin()), e(&*(ctr.end() - 1) + 1) {
    }

    template <size_t Size>
    ArrayRef(DataType (&array)[Size])
        : b(&array[0]), e(&array[Size]) {
    }

    ArrayRef()
        : b(0), e(0) {
    }

    size_t size() const {
        return e - b;
    }

    DataType &operator[](std::size_t idx) {
        return b[idx];
    }

    const DataType &operator[](std::size_t idx) const {
        return b[idx];
    }

    DataType *begin() {
        return b;
    }

    const DataType *begin() const {
        return b;
    }

    DataType *end() {
        return e;
    }

    const DataType *end() const {
        return e;
    }

    void swap(ArrayRef &rhs) {
        std::swap(b, rhs.b);
        std::swap(e, rhs.e);
    }

    operator ArrayRef<const DataType>() {
        return ArrayRef<const DataType>(b, e);
    }

  private:
    DataType *b;
    DataType *e;
};
