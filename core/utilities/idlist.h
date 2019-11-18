/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/debug_helpers.h"

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>

namespace NEO {

template <typename NodeObjectType>
struct IDNode {
    IDNode()
        : prev(nullptr),
          next(nullptr) {
    }

    virtual ~IDNode() = default;

    void insertOneNext(NodeObjectType &nd) {
        nd.next = next;
        if (next != nullptr) {
            next->prev = &nd;
        }
        nd.prev = static_cast<NodeObjectType *>(this);
        next = &nd;
    }

    void insertOnePrev(NodeObjectType &nd) {
        if (prev != nullptr) {
            prev->next = &nd;
        }
        nd.next = static_cast<NodeObjectType *>(this);
        nd.prev = prev;
        prev = &nd;
    }

    NodeObjectType *slice() {
        NodeObjectType *rest = next;
        next = nullptr;
        if (rest != nullptr) {
            rest->prev = nullptr;
        }
        return rest;
    }

    void insertAllNext(NodeObjectType &rhs) {
        NodeObjectType *rhsTail = rhs.getTail();
        rhsTail->next = next;
        if (next != nullptr) {
            next->prev = rhsTail;
        }
        rhs.prev = static_cast<NodeObjectType *>(this);
        next = &rhs;
    }

    NodeObjectType *getTail() {
        NodeObjectType *curr = static_cast<NodeObjectType *>(this);
        while (curr->next != nullptr) {
            curr = curr->next;
        }
        return curr;
    }

    NodeObjectType *getHead() {
        NodeObjectType *curr = static_cast<NodeObjectType *>(this);
        while (curr->prev != nullptr) {
            curr = curr->prev;
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
        if (this->prev != nullptr) {
            this->prev->next = nullptr;
        }
        this->next = nullptr;
        return std::unique_ptr<NodeObjectType>(static_cast<NodeObjectType *>(this));
    }

    std::unique_ptr<NodeObjectType> deleteThisAndAllPrev() {
        NodeObjectType *curr = this->prev;
        while (curr != nullptr) {
            auto n = curr->prev;
            delete curr;
            curr = n;
        }
        if (this->next != nullptr) {
            this->next->prev = nullptr;
        }
        this->prev = nullptr;
        return std::unique_ptr<NodeObjectType>(static_cast<NodeObjectType *>(this));
    }

    std::unique_ptr<NodeObjectType> deleteThisAndAllConnected() {
        deleteThisAndAllNext().release();
        return deleteThisAndAllPrev();
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

    size_t countPredecessors() const {
        const NodeObjectType *curr = static_cast<const NodeObjectType *>(this);
        size_t successors = 0;
        while (curr->prev != nullptr) {
            curr = curr->prev;
            ++successors;
        }
        return successors;
    }

    size_t countThisAndAllConnected() const {
        return 1 + countPredecessors() + countSuccessors();
    }

    bool isPredecessorOf(NodeObjectType &rhs) const {
        NodeObjectType *curr = next;
        while (curr != nullptr) {
            if (curr == &rhs) {
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    bool isSuccessorOf(NodeObjectType &lhs) const {
        NodeObjectType *curr = prev;
        while (curr != nullptr) {
            if (curr == &lhs) {
                return true;
            }
            curr = curr->prev;
        }
        return false;
    }

    bool isConnectedWith(NodeObjectType &node) const {
        if (this == &node) {
            return true;
        }

        return isSuccessorOf(node) || isPredecessorOf(node);
    }

    NodeObjectType *prev;
    NodeObjectType *next;
};

template <typename NodeObjectType, bool ThreadSafe = true, bool OwnsNodes = false, bool SupportRecursiveLock = true>
class IDList {
  public:
    using ThisType = IDList<NodeObjectType, ThreadSafe, OwnsNodes, SupportRecursiveLock>;

    IDList()
        : head(nullptr), tail(nullptr), locked(), spinLockedListener(nullptr) {
        locked.clear(std::memory_order_release);
    }

    IDList(NodeObjectType *node)
        : head(node), tail(nullptr), locked(), spinLockedListener(nullptr) {
        locked.clear(std::memory_order_release);
        head = node;
        if (node == nullptr) {
            tail = nullptr;
        } else {
            tail = node->getTail();
        }
    }

    ~IDList() {
        this->cleanup();
    }

    IDList(const IDList &) = delete;
    IDList &operator=(const IDList &) = delete;

    void pushFrontOne(NodeObjectType &node) {
        processLocked<ThisType, &ThisType::pushFrontOneImpl>(&node);
    }

    void pushTailOne(NodeObjectType &node) {
        processLocked<ThisType, &ThisType::pushTailOneImpl>(&node);
    }

    std::unique_ptr<NodeObjectType> removeOne(NodeObjectType &node) {
        return std::unique_ptr<NodeObjectType>(processLocked<ThisType, &ThisType::removeOneImpl>(&node));
    }

    std::unique_ptr<NodeObjectType> removeFrontOne() {
        return std::unique_ptr<NodeObjectType>(processLocked<ThisType, &ThisType::removeFrontOneImpl>(nullptr));
    }

    NodeObjectType *detachSequence(NodeObjectType &first, NodeObjectType &last) {
        return processLocked<ThisType, &ThisType::detachSequenceImpl>(&first, &last);
    }

    NodeObjectType *detachNodes() {
        return processLocked<ThisType, &ThisType::detachNodesImpl>();
    }

    void splice(NodeObjectType &nodes) {
        processLocked<ThisType, &ThisType::spliceImpl>(&nodes);
    }

    void deleteAll() {
        NodeObjectType *nodes = detachNodes();
        nodes->deleteThisAndAllNext();
    }

    NodeObjectType *peekHead() {
        return processLocked<ThisType, &ThisType::peekHeadImpl>();
    }

    NodeObjectType *peekTail() {
        return processLocked<ThisType, &ThisType::peekTailImpl>();
    }

    bool peekIsEmpty() {
        return (peekHead() == nullptr);
    }

    bool peekContains(NodeObjectType &node) {
        return (processLocked<ThisType, &ThisType::peekContainsImpl>(&node) != nullptr);
    }

  protected:
    NodeObjectType *peekHeadImpl(NodeObjectType *, void *) {
        return head;
    }
    NodeObjectType *peekTailImpl(NodeObjectType *, void *) {
        return tail;
    }
    template <bool C = OwnsNodes>
    typename std::enable_if<C, void>::type cleanup() {
        if (head != nullptr) {
            head->deleteThisAndAllNext();
        }
        head = nullptr;
        tail = nullptr;
    }

    template <bool C = OwnsNodes>
    typename std::enable_if<!C, void>::type cleanup() {
        ;
    }

    void notifySpinLocked() {
        if (spinLockedListener != nullptr) {
            (*spinLockedListener)(*this);
        }
    }

    template <typename T, NodeObjectType *(T::*Process)(NodeObjectType *node1, void *data), bool C1 = ThreadSafe, bool C2 = SupportRecursiveLock>
    typename std::enable_if<C1 && !C2, NodeObjectType *>::type processLocked(NodeObjectType *node1 = nullptr, void *data = nullptr) {
        while (locked.test_and_set(std::memory_order_acquire)) {
            notifySpinLocked();
        }

        NodeObjectType *ret = nullptr;
        try {
            ret = (static_cast<T *>(this)->*Process)(node1, data);
        } catch (...) {
            locked.clear(std::memory_order_release);
            throw;
        }

        locked.clear(std::memory_order_release);

        return ret;
    }

    template <typename T, NodeObjectType *(T::*Process)(NodeObjectType *node1, void *data), bool C1 = ThreadSafe, bool C2 = SupportRecursiveLock>
    typename std::enable_if<C1 && C2, NodeObjectType *>::type processLocked(NodeObjectType *node1 = nullptr, void *data = nullptr) {
        std::thread::id currentThreadId = std::this_thread::get_id();
        if (lockOwner == currentThreadId) {
            return (static_cast<T *>(this)->*Process)(node1, data);
        }

        while (locked.test_and_set(std::memory_order_acquire)) {
            notifySpinLocked();
        }

        lockOwner = currentThreadId;

        NodeObjectType *ret = nullptr;
        try {
            ret = (static_cast<T *>(this)->*Process)(node1, data);
        } catch (...) {
            lockOwner = std::thread::id();
            locked.clear(std::memory_order_release);
            throw;
        }

        lockOwner = std::thread::id();
        locked.clear(std::memory_order_release);

        return ret;
    }

    template <typename T, NodeObjectType *(T::*Process)(NodeObjectType *node, void *data), bool C = ThreadSafe>
    typename std::enable_if<!C, NodeObjectType *>::type processLocked(NodeObjectType *node = nullptr, void *data = nullptr) {
        return (this->*Process)(node, data);
    }

    NodeObjectType *pushFrontOneImpl(NodeObjectType *node, void *) {
        if (head == nullptr) {
            DEBUG_BREAK_IF(tail != nullptr);
            pushTailOneImpl(node, nullptr);
            return nullptr;
        }

        node->prev = nullptr;
        node->next = head;
        head->prev = node;
        head = node;

        return nullptr;
    }

    NodeObjectType *pushTailOneImpl(NodeObjectType *node, void *) {
        if (tail == nullptr) {
            DEBUG_BREAK_IF(head != nullptr);
            node->prev = nullptr;
            node->next = nullptr;
            head = node;
            tail = node;
            return nullptr;
        }

        node->next = nullptr;
        node->prev = tail;
        tail->next = node;
        tail = node;

        return nullptr;
    }

    NodeObjectType *removeOneImpl(NodeObjectType *node, void *) {
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        }
        if (tail == node) {
            tail = node->prev;
        }
        if (head == node) {
            head = node->next;
        }
        node->prev = nullptr;
        node->next = nullptr;
        return node;
    }

    NodeObjectType *removeFrontOneImpl(NodeObjectType *, void *) {
        if (head == nullptr) {
            return nullptr;
        }
        return removeOneImpl(head, nullptr);
    }

    NodeObjectType *detachSequenceImpl(NodeObjectType *node, void *data) {
        NodeObjectType *first = node;
        NodeObjectType *last = static_cast<NodeObjectType *>(data);
        if (first == last) {
            return removeOneImpl(first, nullptr);
        }

        if (first->prev != nullptr) {
            first->prev->next = last->next;
        }

        if (last->next != nullptr) {
            last->next->prev = first->prev;
        }

        if (head == first) {
            head = last->next;
        }

        if (tail == last) {
            tail = first->prev;
        }

        first->prev = nullptr;
        last->next = nullptr;

        return first;
    }

    NodeObjectType *detachNodesImpl(NodeObjectType *, void *) {
        NodeObjectType *rest = head;
        head = nullptr;
        tail = nullptr;
        return rest;
    }

    NodeObjectType *spliceImpl(NodeObjectType *node, void *) {
        if (tail == nullptr) {
            DEBUG_BREAK_IF(head != nullptr);
            head = node;
            node->prev = nullptr;
            tail = node->getTail();
            return nullptr;
        }

        tail->next = node;
        node->prev = tail;
        tail = node->getTail();
        return nullptr;
    }

    NodeObjectType *peekContainsImpl(NodeObjectType *node, void *) {
        NodeObjectType *curr = head;
        while (curr != nullptr) {
            if (curr == node) {
                return node;
            }
            curr = curr->next;
        }
        return nullptr;
    }

    NodeObjectType *head;
    NodeObjectType *tail;

    std::atomic_flag locked;
    std::atomic<std::thread::id> lockOwner;
    void (*spinLockedListener)(ThisType &list);
};

template <typename NodeObjectType>
struct IDNodeRef : IDNode<IDNodeRef<NodeObjectType>> {
    IDNodeRef(NodeObjectType *ref)
        : ref(ref) {
    }
    NodeObjectType *ref;
};

template <typename NodeObjectType, bool ThreadSafe = true, bool OwnsNodes = true>
class IDRefList : public IDList<IDNodeRef<NodeObjectType>, ThreadSafe, OwnsNodes> {
  public:
    void pushRefFrontOne(NodeObjectType &node) {
        auto refNode = std::unique_ptr<IDNodeRef<NodeObjectType>>(new IDNodeRef<NodeObjectType>(&node));
        this->pushFrontOne(*refNode);
        refNode.release();
    }
};
} // namespace NEO
