/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>

template <typename Arg1>
size_t countArgs(const Arg1 &arg1) {
    return 1;
}

template <typename Arg1, typename... Rest>
size_t countArgs(const Arg1 &arg1, const Rest &... rest) {
    return 1 + countArgs(rest...);
}

template <typename NodeObjectType, typename... Args>
int verifySequence(const NodeObjectType *base, int nodeNum, const NodeObjectType *last) {
    if (base == last) {
        return -1;
    } else {
        return nodeNum;
    }
}

template <typename NodeObjectType, typename... Rest>
int verifySequence(const NodeObjectType *base, int nodeNum, const NodeObjectType *current, const Rest *... rest) {
    if (base == nullptr) {
        return nodeNum - 1;
    } else if (base == current) {
        return verifySequence(base->next, nodeNum + 1, rest...);
    } else {
        return nodeNum;
    }
}

// Helper function for testing if nodes are placed exactly in given order
// Negative return values indicates that the sequence is ordered as requested.
// Non-negative value indicates first node that breaks the order.
template <typename NodeObjectType, typename... Rest>
int verifySequence(const NodeObjectType *base, const NodeObjectType *first, const Rest *... rest) {
    return verifySequence(base, 0, first, rest...);
}

// Helper function for testing if nodes are placed exactly in given order
// Negative return values indicates that the sequence is ordered as requested.
// Non-negative value indicates first node that breaks the order.
// Note : verifies also "last->tail == nulptr"
template <typename NodeObjectType, typename... Rest>
int verifyFListOrder(const NodeObjectType *base, const NodeObjectType *first, const Rest *... rest) {
    NodeObjectType *sentinel = nullptr;
    int sequenceRet = verifySequence(base, first, rest..., sentinel);
    if (sequenceRet < 0) {
        return sequenceRet;
    }
    int totalReferenceNodes = (int)countArgs(first, rest...);
    if (sequenceRet >= totalReferenceNodes) {
        // base defines longer sequence than expected
        return totalReferenceNodes - 1;
    }

    return sequenceRet;
}

// Helper function for testing if nodes are ordered exactly in given order
// Negative return values indicates that the sequence is ordered as requested.
// Non-negative value indicates first node that breaks the order.
// Note : verifies also "first->prev == nullptr" and "last->tail == nulptr"
template <typename NodeObjectType, typename... Rest>
int verifyDListOrder(const NodeObjectType *base, const NodeObjectType *first, const Rest *... rest) {
    if (base->prev != nullptr) {
        return 0;
    }
    return verifyFListOrder(base, first, rest...);
}
