/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include <atomic>
#include <memory>
#include <type_traits>

namespace OCLRT {

template <typename NodeObjectType, bool Atomic>
struct PtrType {
};

template <typename NodeObjectType>
struct PtrType<NodeObjectType, true> {
    using type = std::atomic<NodeObjectType *>;
};

template <typename NodeObjectType>
struct PtrType<NodeObjectType, false> {
    using type = NodeObjectType *;
};

template <typename NodeObjectType, bool Atomic>
using PtrType_t = typename PtrType<NodeObjectType, Atomic>::type;

template <typename NodeObjectType>
struct IFNode {
    IFNode()
        : next(nullptr) {
    }

    void insertOneNext(NodeObjectType &nd) {
        nd.next = next;
        next = &nd;
    }

    NodeObjectType *slice() {
        NodeObjectType *rest = next;
        next = nullptr;
        return rest;
    }

    void insertAllNext(NodeObjectType &rhs) {
        NodeObjectType *rhsTail = rhs.getTail();
        rhsTail->next = next;
        next = &rhs;
    }

    NodeObjectType *getTail() {
        NodeObjectType *curr = static_cast<NodeObjectType *>(this);
        while (curr->next != nullptr) {
            curr = curr->next;
        }
        return curr;
    }

    std::unique_ptr<NodeObjectType> deleteThisAndAllNext() {
        NodeObjectType *curr = this->next;
        while (curr != nullptr) {
            auto n = curr->next;
            delete curr;
            curr = n;
        }
        this->next = nullptr;
        return std::unique_ptr<NodeObjectType>(static_cast<NodeObjectType *>(this));
    }

    size_t countSuccessors() const {
        const NodeObjectType *curr = static_cast<const NodeObjectType *>(this);
        size_t successors = 0;
        while (curr->next != nullptr) {
            curr = curr->next;
            ++successors;
        }
        return successors;
    }

    NodeObjectType *next;
};

template <typename NodeObjectType, bool ThreadSafe = true, bool OwnsNodes = false>
class IFList {
  public:
    IFList()
        : head(nullptr) {
    }

    IFList(NodeObjectType *node) {
        head = node;
    }

    ~IFList() {
        this->cleanup();
    }

    IFList(const IFList &) = delete;
    IFList &operator=(const IFList &) = delete;

    template <bool C = ThreadSafe>
    typename std::enable_if<C, void>::type pushFrontOne(NodeObjectType &node) {
        node.next = head;
        compareExchangeHead(node.next, &node);
    }

    template <bool C = ThreadSafe>
    typename std::enable_if<C, NodeObjectType *>::type detachNodes() {
        NodeObjectType *rest = head;
        compareExchangeHead(rest, nullptr);
        return rest;
    }

    template <bool C = ThreadSafe>
    typename std::enable_if<!C, void>::type pushFrontOne(NodeObjectType &node) {
        node.next = head;
        head = &node;
    }

    template <bool C = ThreadSafe>
    typename std::enable_if<!C, NodeObjectType *>::type detachNodes() {
        NodeObjectType *rest = head;
        head = nullptr;
        return rest;
    }

    template <bool C = ThreadSafe>
    typename std::enable_if<!C, void>::type splice(NodeObjectType &nodes) {
        if (head == nullptr) {
            head = &nodes;
        } else {
            head->getTail()->next = &nodes;
        }
    }

    void deleteAll() {
        NodeObjectType *nodes = detachNodes();
        if (nodes != nullptr) {
            nodes->deleteThisAndAllNext();
        }
    }

    NodeObjectType *peekHead() {
        return head;
    }

    bool peekIsEmpty() {
        return (peekHead() == nullptr);
    }

  protected:
    template <bool C = OwnsNodes>
    typename std::enable_if<C, void>::type cleanup() {
        deleteAll();
    }

    template <bool C = OwnsNodes>
    typename std::enable_if<!C, void>::type cleanup() {
        ;
    }

    template <bool C = ThreadSafe>
    typename std::enable_if<C, void>::type compareExchangeHead(NodeObjectType *&expected, NodeObjectType *desired) {
        while (!std::atomic_compare_exchange_weak(&head, &expected, desired)) {
            ;
        }
    }

    PtrType_t<NodeObjectType, ThreadSafe> head;
};

template <typename NodeObjectType>
struct IFNodeRef : IFNode<IFNodeRef<NodeObjectType>> {
    IFNodeRef(NodeObjectType *ref)
        : ref(ref) {
    }
    NodeObjectType *ref;
};

template <typename NodeObjectType, bool ThreadSafe = true, bool OwnsNodes = true>
class IFRefList : public IFList<IFNodeRef<NodeObjectType>, ThreadSafe, OwnsNodes> {
  public:
    void pushRefFrontOne(NodeObjectType &node) {
        auto up = std::unique_ptr<IFNodeRef<NodeObjectType>>(new IFNodeRef<NodeObjectType>(&node));
        this->pushFrontOne(*up);
        up.release();
    }
};
} // namespace OCLRT
