/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/bitcontainers.h"
#include "shared/source/utilities/idlist.h"
#include "shared/source/utilities/iflist.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/utilities/range.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/unit_test/utilities/containers_tests_helpers.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

using namespace NEO;

struct DummyFNode : IFNode<DummyFNode> {
    DummyFNode(uint32_t *destructorsCounter = nullptr)
        : destructorsCounter(destructorsCounter) {
    }
    ~DummyFNode() override {
        if (destructorsCounter != nullptr) {
            ++(*destructorsCounter);
        }
    }

    void setPrev(DummyFNode *) {
    }

    uint32_t *destructorsCounter = nullptr;
};

struct DummyDNode : IDNode<DummyDNode> {
    DummyDNode(uint32_t *destructorsCounter = nullptr)
        : destructorsCounter(destructorsCounter) {
    }
    ~DummyDNode() override {
        if (destructorsCounter != nullptr) {
            ++(*destructorsCounter);
        }
    }

    void setPrev(DummyDNode *nd) {
        prev = nd;
    }

    uint32_t *destructorsCounter;
};

template <typename NodeType = DummyFNode, size_t arraySize>
void makeList(NodeType *(&nodes)[arraySize], uint32_t *destructorsCounter = nullptr) {
    NodeType *prev = nullptr;
    for (NodeType *&nd : nodes) {
        nd = new NodeType(destructorsCounter);
        if (prev != nullptr) {
            prev->next = nd;
            nd->setPrev(prev);
        }
        prev = nd;
    }
}

TEST(IFNode, WhenInsertingOneThenNodeIsAppendedAtEnd) {
    DummyFNode head;
    ASSERT_EQ(nullptr, head.next);

    DummyFNode successor;
    successor.next = &head; // garbage value
    head.insertOneNext(successor);
    ASSERT_EQ(&successor, head.next);
    ASSERT_EQ(nullptr, successor.next);

    DummyFNode successor2;
    successor2.next = &head; // garbage value
    head.insertOneNext(successor2);
    ASSERT_EQ(&successor2, head.next);
    ASSERT_EQ(&successor, successor2.next);
    ASSERT_EQ(nullptr, successor.next);
}

TEST(IFNode, WhenInsertingAllThenAllNodesAreInserted) {
    DummyFNode *listA[5];
    DummyFNode *listB[7];
    makeList(listA);
    makeList(listB);

    listA[2]->insertAllNext(*listB[0]);
    ASSERT_EQ(listB[0], listA[2]->next);
    ASSERT_EQ(listA[3], listB[sizeof(listB) / sizeof(listB[0]) - 1]->next);

    DummyFNode *curr = listA[0];
    int nodesCount = 0;
    while (curr) {
        auto next = curr->next;
        delete curr;
        ++nodesCount;
        curr = next;
    }
    int totalNodes = sizeof(listB) / sizeof(listB[0]) + sizeof(listA) / sizeof(listA[0]);
    ASSERT_EQ(totalNodes, nodesCount);
}

TEST(IFNode, GivenFirstElementWhenDeletingThisAndAllNextThenAllElementsAreDeleted) {
    DummyFNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    list[0]->deleteThisAndAllNext();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]), destructorCounter);
}

TEST(IFNode, WhenGettingTailThenLastElementIsReturned) {
    DummyFNode *list[7];
    makeList(list, nullptr);
    auto tail = list[0]->getTail();
    list[0]->deleteThisAndAllNext();
    ASSERT_EQ(list[sizeof(list) / sizeof(list[0]) - 1], tail);
}

TEST(IFNode, WhenSlicingThenListIsSplitCorrectly) {
    DummyFNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    auto listB = list[3]->slice();
    ASSERT_EQ(nullptr, list[3]->next);
    ASSERT_EQ(list[4], listB);

    list[0]->deleteThisAndAllNext();
    listB->deleteThisAndAllNext();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]), destructorCounter);
}

TEST(IFNode, WhenCountingSuccessorsThenCorrectValueIsReturned) {
    DummyFNode *nodes[7];
    makeList(nodes, 0);
    ASSERT_EQ(sizeof(nodes) / sizeof(nodes[0]), 1 + nodes[0]->countSuccessors());
    for (DummyFNode *n : nodes) {
        delete n;
    }
}

TEST(IFNode, WhenVerifingSequenceThenCorrectValueIsReturned) {
    DummyFNode *nodes[4];
    DummyFNode additional;
    makeList(nodes, 0);
    // valid list
    ASSERT_EQ(-1, verifyFListOrder(nodes[0], nodes[0], nodes[1], nodes[2], nodes[3]));

    // last->next != nullptr
    nodes[3]->next = &additional;
    ASSERT_EQ(3, verifyFListOrder(nodes[0], nodes[0], nodes[1], nodes[2], nodes[3]));

    // valid list
    nodes[2]->next = nullptr;
    ASSERT_EQ(-1, verifyFListOrder(nodes[0], nodes[0], nodes[1], nodes[2]));

    // list too short
    ASSERT_EQ(2, verifyFListOrder(nodes[0], nodes[0], nodes[1], nodes[2], nodes[3]));

    // wrong order (first)
    ASSERT_EQ(0, verifyFListOrder(nodes[0], nodes[1], nodes[0], nodes[2]));

    // wrong order (second)
    ASSERT_EQ(1, verifyFListOrder(nodes[0], nodes[0], nodes[2], nodes[1]));

    // wrong order (last)
    ASSERT_EQ(2, verifyFListOrder(nodes[0], nodes[0], nodes[1], nodes[3]));

    for (DummyFNode *n : nodes) {
        delete n;
    }
}

template <bool threadSafe>
void iFListTestPushFrontOne() {
    IFList<DummyFNode, threadSafe, false> list;
    ASSERT_TRUE(list.peekIsEmpty());

    DummyFNode node1;
    node1.next = &node1; // garbage
    list.pushFrontOne(node1);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(nullptr, node1.next);

    DummyFNode node2;
    node2.next = &node1; // garbage
    list.pushFrontOne(node2);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node2, list.peekHead());
    ASSERT_EQ(&node1, node2.next);
}

TEST(IFList, GivenThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iFListTestPushFrontOne<true>();
}

TEST(IFList, GivenNonThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iFListTestPushFrontOne<false>();
}

TEST(IFList, WhenResettingThenListIsEmpty) {
    DummyFNode *nodes[7];
    uint32_t destructorCounter = 0;
    makeList(nodes, &destructorCounter);
    auto list = std::unique_ptr<IFList<DummyFNode, false, true>>(new IFList<DummyFNode, false, true>(nodes[0]));
    ASSERT_FALSE(list->peekIsEmpty());
    ASSERT_EQ(nodes[0], list->peekHead());
    list.reset();
    ASSERT_EQ(sizeof(nodes) / sizeof(nodes[0]), destructorCounter);
}

TEST(IFList, WhenSplicingAndDeletingAllThenListIsEmpty) {
    DummyFNode *nodes[7];
    DummyFNode *nodes2[sizeof(nodes) / sizeof(nodes[0])];
    uint32_t destructorCounter = 0;
    makeList(nodes, &destructorCounter);
    makeList(nodes2, &destructorCounter);
    IFList<DummyFNode, false, false> list;
    list.splice(*nodes[0]);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(nodes[0], list.peekHead());

    list.splice(*nodes2[0]);

    list.deleteAll();
    ASSERT_EQ(2 * sizeof(nodes) / sizeof(nodes[0]), destructorCounter);
}

template <bool threadSafe>
void iFListTestDetachNodes() {
    IFList<DummyFNode, threadSafe, false> list;

    uint32_t destructorCounter = 0;
    static const uint32_t maxNodes = 17;
    for (uint32_t i = 0; i < maxNodes; ++i) {
        list.pushFrontOne(*new DummyFNode(&destructorCounter));
    }
    ASSERT_FALSE(list.peekIsEmpty());

    auto nodes = list.detachNodes();
    ASSERT_TRUE(list.peekIsEmpty());

    nodes->deleteThisAndAllNext();
    ASSERT_EQ(maxNodes, destructorCounter);
}

TEST(IFList, GivenThreadSafeWhenDetachingNodesThenResultIsCorrect) {
    iFListTestDetachNodes<true>();
}

TEST(IFList, GivenNonThreadSafeWhenDetachingNodesThenResultIsCorrect) {
    iFListTestDetachNodes<false>();
}

TEST(IFList, WhenExchangingHeadThenResultIsCorrect) {
    struct DummyList : IFList<DummyFNode, true, false> {
        void testCompareExchangeHead(DummyFNode *preSet, DummyFNode *&expected, DummyFNode *desired) {
            head = preSet;
            compareExchangeHead(expected, desired);
        }
    };

    DummyList list;
    DummyFNode nd;
    nd.next = nullptr;
    list.testCompareExchangeHead(nullptr, nd.next, &nd);
    EXPECT_EQ(list.peekHead(), &nd);
    EXPECT_EQ(nullptr, nd.next);

    DummyFNode nd2;
    nd2.next = &nd;
    list.testCompareExchangeHead(nullptr, nd2.next, &nd2);
    EXPECT_EQ(list.peekHead(), &nd2);
    EXPECT_EQ(nullptr, nd2.next);
}

template <bool threadSafe>
void iFRefListTestPushFrontOne() {
    uint32_t destructorCounter = 0;
    {
        auto list = std::unique_ptr<IFRefList<DummyFNode, threadSafe, true>>(new IFRefList<DummyFNode, threadSafe, true>());
        ASSERT_TRUE(list->peekIsEmpty());

        DummyFNode node1(&destructorCounter);
        node1.next = nullptr; // garbage
        list->pushRefFrontOne(node1);
        ASSERT_FALSE(list->peekIsEmpty());
        ASSERT_EQ(&node1, list->peekHead()->ref);
        ASSERT_EQ(nullptr, node1.next);

        DummyFNode node2(&destructorCounter);
        node2.next = nullptr; // garbage
        list->pushRefFrontOne(node2);
        ASSERT_FALSE(list->peekIsEmpty());
        ASSERT_EQ(&node2, list->peekHead()->ref);
        ASSERT_EQ(nullptr, node2.next);

        ASSERT_EQ(&node2, list->peekHead()->ref);
        ASSERT_EQ(&node1, list->peekHead()->next->ref);

        EXPECT_EQ(0u, destructorCounter);
        list.reset();
        EXPECT_EQ(0u, destructorCounter);
    }
    EXPECT_EQ(2u, destructorCounter);
}

TEST(IFRefList, GivenThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iFRefListTestPushFrontOne<true>();
}

TEST(IFRefList, GivenNonThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iFRefListTestPushFrontOne<false>();
}

TEST(IDNode, GivenNextOrPrevWhenInsertingOneThenNodeInsertedInCorrectPlace) {
    DummyDNode head;
    ASSERT_EQ(nullptr, head.prev);
    ASSERT_EQ(nullptr, head.next);

    DummyDNode successor, successor2;
    successor.next = &head;      // garbage value
    successor.prev = &successor; // garbage value
    head.insertOneNext(successor);
    ASSERT_EQ(&successor, head.next);
    ASSERT_EQ(&head, successor.prev);
    ASSERT_EQ(nullptr, successor.next);
    head.insertOneNext(successor2);
    ASSERT_EQ(&successor2, successor.prev);

    DummyDNode predecessor, predecessor2;
    predecessor.next = &successor; // garbage value
    predecessor.prev = &head;      // garbage value
    head.insertOnePrev(predecessor);
    ASSERT_EQ(&predecessor, head.prev);
    ASSERT_EQ(&head, predecessor.next);
    ASSERT_EQ(nullptr, predecessor.prev);
    head.insertOnePrev(predecessor2);
    ASSERT_EQ(&predecessor2, predecessor.next);
}

TEST(IDNode, WhenInsertingAllNextThenAllNodesInsertedAtEnd) {
    DummyDNode *listA[5];
    DummyDNode *listB[7];
    DummyDNode *listC[1];
    makeList(listA);
    makeList(listB);
    makeList(listC);

    listA[2]->insertAllNext(*listB[0]);
    EXPECT_EQ(listB[0], listA[2]->next);
    EXPECT_EQ(listB[0]->prev, listA[2]);
    EXPECT_EQ(listA[3], listB[sizeof(listB) / sizeof(listB[0]) - 1]->next);
    listA[4]->insertAllNext(*listC[0]);
    int totalNodes = sizeof(listA) / sizeof(listA[0]) + sizeof(listB) / sizeof(listB[0]) + sizeof(listC) / sizeof(listC[0]);

    DummyDNode *curr = listA[0];
    int nodesCount = 0;
    while (curr && curr->next) {
        auto next = curr->next;
        ++nodesCount;
        curr = next;
    }
    ++nodesCount;

    ASSERT_EQ(totalNodes, nodesCount);

    nodesCount = 0;
    while (curr) {
        auto prev = curr->prev;
        delete curr;
        ++nodesCount;
        curr = prev;
    }

    ASSERT_EQ(totalNodes, nodesCount);
}

TEST(IDNode, WhenDeletingThisAndAllNextThenAllCorrectNodesAreDeleted) {
    DummyFNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    list[0]->deleteThisAndAllNext();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]), destructorCounter);
}

TEST(IDNode, WhenDeletingThisAndAllPrevThenAllCorrectNodesAreDeleted) {
    DummyDNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    list[sizeof(list) / sizeof(list[0]) - 2]->deleteThisAndAllPrev();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]) - 1, destructorCounter);

    destructorCounter = 0;
    list[sizeof(list) / sizeof(list[0]) - 1]->deleteThisAndAllPrev();
    ASSERT_EQ(1U, destructorCounter);
}

TEST(IDNode, WhenDeletingThisAndAllConnectedThenAllCorrectNodesAreDeleted) {
    DummyDNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    list[sizeof(list) / sizeof(list[0]) / 2]->deleteThisAndAllConnected();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]), destructorCounter);
}

TEST(IDNode, WhenGettingHeadAndGettingTailThenCorrectNodeIsReturned) {
    DummyDNode *list[7];
    makeList(list, nullptr);
    auto head = list[5]->getHead();
    auto tail = list[0]->getTail();
    list[0]->deleteThisAndAllNext();
    ASSERT_EQ(list[0], head);
    ASSERT_EQ(list[sizeof(list) / sizeof(list[0]) - 1], tail);
}

TEST(IDNode, WhenSlicingThenListsAreCorrect) {
    DummyDNode *list[7];
    uint32_t destructorCounter = 0;
    makeList(list, &destructorCounter);
    auto listB = list[3]->slice();
    ASSERT_EQ(nullptr, list[6]->slice());
    ASSERT_EQ(nullptr, list[3]->next);
    ASSERT_EQ(list[4], listB);
    ASSERT_EQ(nullptr, listB->prev);

    list[0]->deleteThisAndAllNext();
    listB->deleteThisAndAllNext();
    ASSERT_EQ(sizeof(list) / sizeof(list[0]), destructorCounter);
}

TEST(IDNode, WhenCountingConnectedThenCountIsCorrect) {
    DummyDNode *nodes[7];
    makeList(nodes, 0);

    ASSERT_EQ(3U, nodes[3]->countSuccessors());
    ASSERT_EQ(2U, nodes[2]->countPredecessors());
    ASSERT_EQ(7U, nodes[2]->countThisAndAllConnected());
    for (auto &d : nodes) {
        delete d;
    }
}

TEST(IDNode, WhenCheckingRelationshipThenResultIsCorrect) {
    DummyDNode *nodes[7];
    makeList(nodes, 0);

    DummyDNode unatachedNode;

    ASSERT_FALSE(nodes[3]->isPredecessorOf(*nodes[0]));
    ASSERT_FALSE(nodes[3]->isPredecessorOf(*nodes[1]));
    ASSERT_FALSE(nodes[3]->isPredecessorOf(*nodes[2]));
    ASSERT_FALSE(nodes[3]->isPredecessorOf(*nodes[3]));
    ASSERT_TRUE(nodes[3]->isPredecessorOf(*nodes[4]));
    ASSERT_TRUE(nodes[3]->isPredecessorOf(*nodes[5]));
    ASSERT_TRUE(nodes[3]->isPredecessorOf(*nodes[6]));
    ASSERT_FALSE(nodes[3]->isPredecessorOf(unatachedNode));

    ASSERT_TRUE(nodes[3]->isSuccessorOf(*nodes[0]));
    ASSERT_TRUE(nodes[3]->isSuccessorOf(*nodes[1]));
    ASSERT_TRUE(nodes[3]->isSuccessorOf(*nodes[2]));
    ASSERT_FALSE(nodes[3]->isSuccessorOf(*nodes[3]));
    ASSERT_FALSE(nodes[3]->isSuccessorOf(*nodes[4]));
    ASSERT_FALSE(nodes[3]->isSuccessorOf(*nodes[5]));
    ASSERT_FALSE(nodes[3]->isSuccessorOf(*nodes[6]));
    ASSERT_FALSE(nodes[3]->isSuccessorOf(unatachedNode));

    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[0]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[1]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[2]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[3]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[4]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[5]));
    ASSERT_TRUE(nodes[3]->isConnectedWith(*nodes[6]));
    ASSERT_FALSE(nodes[3]->isConnectedWith(unatachedNode));

    for (auto n : nodes) {
        delete n;
    }
}

TEST(IDNode, WhenVerifyingSequenceThenResultIsCorrect) {
    DummyDNode *nodes[4];
    makeList(nodes, 0);
    // valid list
    ASSERT_EQ(-1, verifyDListOrder(nodes[0], nodes[0], nodes[1], nodes[2], nodes[3]));

    // first->prev != nullptr
    nodes[0]->prev = nodes[1];
    ASSERT_EQ(0, verifyDListOrder(nodes[0], nodes[0], nodes[1], nodes[2], nodes[3]));

    for (DummyDNode *n : nodes) {
        delete n;
    }
}

template <bool threadSafe>
void iDListTestPushOne() {
    IDList<DummyDNode, threadSafe, false, false> list(nullptr);
    ASSERT_TRUE(list.peekIsEmpty());
    ASSERT_EQ(nullptr, list.peekHead());
    ASSERT_EQ(nullptr, list.peekTail());

    DummyDNode node1;
    node1.next = &node1; // garbage
    node1.prev = &node1; // garbage
    list.pushFrontOne(node1);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(nullptr, node1.next);
    ASSERT_EQ(nullptr, node1.prev);
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(&node1, list.peekTail());

    DummyDNode node2;
    node2.next = &node2; // garbage
    node2.prev = &node2; // garbage
    list.pushFrontOne(node2);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node2, list.peekHead());
    ASSERT_EQ(&node1, node2.next);
    ASSERT_EQ(&node2, node1.prev);
    ASSERT_EQ(nullptr, node2.prev);
    ASSERT_EQ(nullptr, node1.next);
    ASSERT_EQ(&node2, list.peekHead());
    ASSERT_EQ(&node1, list.peekTail());

    DummyDNode node3;
    node3.next = &node3; // garbage
    node3.prev = &node3; // garbage
    list.pushTailOne(node3);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node2, list.peekHead());
    ASSERT_EQ(&node1, node2.next);
    ASSERT_EQ(&node2, node1.prev);
    ASSERT_EQ(nullptr, node2.prev);
    ASSERT_EQ(&node3, node1.next);
    ASSERT_EQ(&node1, node3.prev);
    ASSERT_EQ(nullptr, node3.next);
    ASSERT_EQ(&node2, list.peekHead());
    ASSERT_EQ(&node3, list.peekTail());

    list.detachNodes();
    ASSERT_TRUE(list.peekIsEmpty());
    ASSERT_EQ(nullptr, list.peekHead());
    ASSERT_EQ(nullptr, list.peekTail());

    node1.next = &node1; // garbage
    node1.prev = &node1; // garbage
    list.pushTailOne(node1);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(nullptr, node1.next);
    ASSERT_EQ(nullptr, node1.prev);
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(&node1, list.peekTail());

    node2.next = &node2; // garbage
    node2.prev = &node2; // garbage
    list.pushTailOne(node2);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(&node2, node1.next);
    ASSERT_EQ(&node1, node2.prev);
    ASSERT_EQ(nullptr, node1.prev);
    ASSERT_EQ(nullptr, node2.next);
    ASSERT_EQ(&node1, list.peekHead());
    ASSERT_EQ(&node2, list.peekTail());
}

TEST(IDList, GivenThreadSafeWhenPushingOneThenResultIsCorrect) {
    iDListTestPushOne<true>();
}

TEST(IDList, GivenNonThreadSafeWhenPushingOneThenResultIsCorrect) {
    iDListTestPushOne<false>();
}

TEST(IDList, WhenResettingThenListIsEmpty) {
    DummyDNode *nodes[7];
    uint32_t destructorCounter = 0;
    makeList(nodes, &destructorCounter);
    auto list = std::unique_ptr<IDList<DummyDNode, false, true, false>>(new IDList<DummyDNode, false, true, false>(nodes[0]));
    ASSERT_FALSE(list->peekIsEmpty());
    ASSERT_EQ(nodes[0], list->peekHead());
    list.reset();
    ASSERT_EQ(sizeof(nodes) / sizeof(nodes[0]), destructorCounter);
}

template <bool threadSafe>
void iDListSpliceAndDeleteAll() {
    DummyDNode *nodes[7];
    DummyDNode *nodes2[sizeof(nodes) / sizeof(nodes[0])];
    uint32_t destructorCounter = 0;
    makeList(nodes, &destructorCounter);
    makeList(nodes2, &destructorCounter);
    IDList<DummyDNode, threadSafe, false, false> list;
    list.splice(*nodes[0]);
    EXPECT_FALSE(list.peekIsEmpty());
    EXPECT_EQ(nodes[0], list.peekHead());

    list.splice(*nodes2[0]);

    DummyDNode *nd = list.peekHead();
    while (nd && nd->next) {
        if (nd->prev) {
            EXPECT_EQ(nd, nd->prev->next);
        }
        nd = nd->next;
    }
    if (nd && nd->prev) {
        EXPECT_EQ(nd, nd->prev->next);
    }
    EXPECT_EQ(list.peekTail(), nd);

    list.deleteAll();
    EXPECT_EQ(2 * sizeof(nodes) / sizeof(nodes[0]), destructorCounter);
}

TEST(IDList, GivenThreadSafeWhenSplicingAndDeletingAllThenResultIsCorrect) {
    iDListSpliceAndDeleteAll<true>();
}

TEST(IDList, GivenNonThreadSafeWhenSplicingAndDeletingAllThenResultIsCorrect) {
    iDListSpliceAndDeleteAll<false>();
}

template <bool threadSafe>
void iDListTestDetachNodes() {
    IDList<DummyDNode, threadSafe, false, false> list;

    uint32_t destructorCounter = 0;
    static const uint32_t maxNodes = 17;
    for (uint32_t i = 0; i < maxNodes; ++i) {
        list.pushFrontOne(*new DummyDNode(&destructorCounter));
    }
    ASSERT_FALSE(list.peekIsEmpty());

    auto nodes = list.detachNodes();
    ASSERT_TRUE(list.peekIsEmpty());

    nodes->deleteThisAndAllNext();
    ASSERT_EQ(maxNodes, destructorCounter);
}

TEST(IDList, GivenThreadSafeWhenDetachingNodesThenResultIsCorrect) {
    iDListTestDetachNodes<true>();
}

TEST(IDList, GivenNonThreadSafeWhenDetachingNodesThenResultIsCorrect) {
    iDListTestDetachNodes<false>();
}

template <bool threadSafe>
void iDListTestRemoveOne() {
    IDList<DummyDNode, threadSafe, false, false> list;

    DummyDNode nodes[3];

    list.pushFrontOne(nodes[0]);
    list.pushFrontOne(nodes[1]);
    list.pushFrontOne(nodes[2]);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&nodes[2], list.peekHead());
    ASSERT_EQ(&nodes[0], list.peekTail());

    auto removedNode = list.removeOne(nodes[1]);
    ASSERT_EQ(&nodes[2], list.peekHead());
    ASSERT_EQ(&nodes[0], list.peekTail());
    ASSERT_EQ(&nodes[2], nodes[0].prev);
    ASSERT_EQ(&nodes[0], nodes[2].next);
    ASSERT_EQ(nullptr, removedNode->prev);
    ASSERT_EQ(nullptr, removedNode->next);
    removedNode.release();

    removedNode = list.removeOne(nodes[2]);
    ASSERT_EQ(&nodes[0], list.peekHead());
    ASSERT_EQ(&nodes[0], list.peekTail());
    ASSERT_EQ(nullptr, nodes[0].prev);
    ASSERT_EQ(nullptr, nodes[0].next);
    ASSERT_EQ(nullptr, removedNode->prev);
    ASSERT_EQ(nullptr, removedNode->next);
    removedNode.release();

    removedNode = list.removeOne(nodes[0]);
    ASSERT_TRUE(list.peekIsEmpty());
    ASSERT_EQ(nullptr, list.peekHead());
    ASSERT_EQ(nullptr, list.peekTail());
    ASSERT_EQ(nullptr, removedNode->prev);
    ASSERT_EQ(nullptr, removedNode->next);
    removedNode.release();

    list.pushFrontOne(nodes[0]);
    list.pushFrontOne(nodes[1]);
    ASSERT_FALSE(list.peekIsEmpty());
    ASSERT_EQ(&nodes[1], list.peekHead());
    ASSERT_EQ(&nodes[0], list.peekTail());
    removedNode = list.removeOne(nodes[0]);
    ASSERT_EQ(&nodes[1], list.peekHead());
    ASSERT_EQ(&nodes[1], list.peekTail());
    ASSERT_EQ(nullptr, removedNode->prev);
    ASSERT_EQ(nullptr, removedNode->next);
    removedNode.release();
}

TEST(IDList, GivenThreadSafeWhenRemovingOneThenResultIsCorrect) {
    iDListTestRemoveOne<true>();
}

TEST(IDList, GivenNonThreadSafeWhenRemovingOneThenResultIsCorrect) {
    iDListTestRemoveOne<false>();
}

template <bool threadSafe>
void iDListTestRemoveFrontOne() {
    IDList<DummyDNode, threadSafe, false, false> list;

    DummyDNode nodes[3];
    DummyDNode *head = nullptr;
    DummyDNode *tail = nullptr;

    list.pushFrontOne(nodes[0]);
    list.pushFrontOne(nodes[1]);
    list.pushFrontOne(nodes[2]);

    ASSERT_FALSE(list.peekIsEmpty());
    head = list.peekHead();
    tail = list.peekTail();
    ASSERT_NE(nullptr, head);
    ASSERT_NE(nullptr, tail);
    EXPECT_NE(head, tail);
    EXPECT_EQ(head, list.removeFrontOne().release());
    EXPECT_EQ(tail, list.peekTail());
    EXPECT_NE(head, list.peekHead());

    ASSERT_FALSE(list.peekIsEmpty());
    head = list.peekHead();
    tail = list.peekTail();
    ASSERT_NE(nullptr, head);
    ASSERT_NE(nullptr, tail);
    EXPECT_NE(head, tail);
    EXPECT_EQ(head, list.removeFrontOne().release());
    EXPECT_EQ(tail, list.peekTail());
    EXPECT_NE(head, list.peekHead());

    ASSERT_FALSE(list.peekIsEmpty());
    head = list.peekHead();
    tail = list.peekTail();
    ASSERT_NE(nullptr, head);
    ASSERT_NE(nullptr, tail);
    EXPECT_EQ(head, tail);
    EXPECT_EQ(head, list.removeFrontOne().release());
    EXPECT_EQ(nullptr, list.peekTail());
    EXPECT_EQ(nullptr, list.peekHead());

    EXPECT_EQ(nullptr, list.removeFrontOne().release());
    EXPECT_EQ(nullptr, list.peekHead());
    EXPECT_EQ(nullptr, list.peekTail());
    EXPECT_TRUE(list.peekIsEmpty());

    EXPECT_EQ(nullptr, list.removeFrontOne().release());
    EXPECT_EQ(nullptr, list.peekHead());
    EXPECT_EQ(nullptr, list.peekTail());
    EXPECT_TRUE(list.peekIsEmpty());
}

TEST(IDList, GivenThreadSafeWhenRemovingFrontOneThenResultIsCorrect) {
    iDListTestRemoveFrontOne<true>();
}

TEST(IDList, GivenNonThreadSafeWhenRemovingFrontOneThenResultIsCorrect) {
    iDListTestRemoveFrontOne<false>();
}

template <bool threadSafe>
void iDListTestDetachSequence() {
    DummyDNode *nodes[10];
    makeList(nodes);
    IDList<DummyDNode, threadSafe, false, false> list(nodes[0]);
    DummyDNode *detachedNodes = nullptr;

    detachedNodes = list.detachSequence(*nodes[1], *nodes[3]);
    ASSERT_EQ(nodes[1], detachedNodes);
    ASSERT_EQ(nullptr, nodes[1]->prev);
    ASSERT_EQ(nodes[2], nodes[1]->next);
    ASSERT_EQ(nodes[2], nodes[3]->prev);
    ASSERT_EQ(nullptr, nodes[3]->next);

    ASSERT_EQ(nodes[4], nodes[0]->next);
    ASSERT_NE(nullptr, nodes[4]);
    ASSERT_EQ(nodes[0], nodes[4]->prev);
    ASSERT_EQ(nodes[0], list.peekHead());
    ASSERT_EQ(nodes[9], list.peekTail());

    detachedNodes = list.detachSequence(*nodes[0], *nodes[0]);
    ASSERT_EQ(nodes[0], detachedNodes);
    ASSERT_EQ(nodes[4], list.peekHead());
    ASSERT_EQ(nodes[9], list.peekTail());
    ASSERT_EQ(nullptr, nodes[0]->next);
    ASSERT_EQ(nullptr, nodes[0]->prev);
    ASSERT_EQ(nullptr, nodes[4]->prev);

    detachedNodes = list.detachSequence(*nodes[4], *nodes[5]);
    ASSERT_EQ(nodes[4], detachedNodes);
    ASSERT_EQ(nodes[6], list.peekHead());
    ASSERT_EQ(nodes[9], list.peekTail());
    ASSERT_EQ(nullptr, nodes[4]->prev);
    ASSERT_EQ(nodes[5], nodes[4]->next);
    ASSERT_EQ(nodes[4], nodes[5]->prev);
    ASSERT_EQ(nullptr, nodes[5]->next);

    detachedNodes = list.detachSequence(*nodes[8], *nodes[9]);
    ASSERT_EQ(nodes[8], detachedNodes);
    ASSERT_EQ(nodes[6], list.peekHead());
    ASSERT_EQ(nodes[7], list.peekTail());
    ASSERT_EQ(nullptr, nodes[8]->prev);
    ASSERT_EQ(nodes[9], nodes[8]->next);
    ASSERT_EQ(nodes[8], nodes[9]->prev);
    ASSERT_EQ(nullptr, nodes[9]->next);

    detachedNodes = list.detachSequence(*nodes[6], *nodes[7]);
    ASSERT_EQ(nodes[6], detachedNodes);
    ASSERT_TRUE(list.peekIsEmpty());
    ASSERT_EQ(nullptr, list.peekHead());
    ASSERT_EQ(nullptr, list.peekTail());
    ASSERT_EQ(nullptr, nodes[6]->prev);
    ASSERT_EQ(nodes[7], nodes[6]->next);
    ASSERT_EQ(nodes[6], nodes[7]->prev);
    ASSERT_EQ(nullptr, nodes[7]->next);

    for (auto n : nodes) {
        delete n;
    }
}

TEST(IDList, GivenThreadSafeWhenDetachingSequenceThenResultIsCorrect) {
    iDListTestDetachSequence<true>();
}

TEST(IDList, GivenNonThreadSafeWhenDetachingSequenceThenResultIsCorrect) {
    iDListTestDetachSequence<false>();
}

template <bool threadSafe>
void iDListTestPeekContains() {
    IDList<DummyDNode, threadSafe, false, false> list;

    DummyDNode *node1 = new DummyDNode;
    EXPECT_FALSE(list.peekContains(*node1));
    list.pushFrontOne(*node1);
    EXPECT_TRUE(list.peekContains(*node1));

    DummyDNode *node2 = new DummyDNode;
    EXPECT_FALSE(list.peekContains(*node2));
    list.pushTailOne(*node2);
    EXPECT_TRUE(list.peekContains(*node2));

    DummyDNode *node3 = new DummyDNode;
    EXPECT_FALSE(list.peekContains(*node3));
    list.pushFrontOne(*node3);
    EXPECT_TRUE(list.peekContains(*node3));

    list.removeOne(*node2);
    EXPECT_FALSE(list.peekContains(*node2));
    EXPECT_TRUE(list.peekContains(*node1));
    EXPECT_TRUE(list.peekContains(*node3));

    list.removeOne(*node1);
    EXPECT_FALSE(list.peekContains(*node2));
    EXPECT_FALSE(list.peekContains(*node1));
    EXPECT_TRUE(list.peekContains(*node3));

    list.removeOne(*node3);
    EXPECT_FALSE(list.peekContains(*node2));
    EXPECT_FALSE(list.peekContains(*node1));
    EXPECT_FALSE(list.peekContains(*node3));
}

TEST(IDList, GivenThreadSafeWhenPeekingThenResultIsCorrect) {
    iDListTestPeekContains<true>();
}

TEST(IDList, GivenNonThreadSafeWhenPeekingThenResultIsCorrect) {
    iDListTestPeekContains<false>();
}

template <typename NodeObjectType, bool threadSafe, bool ownsNodes, bool supportRecursiveLock>
void testIDListSpinlock() {
    struct ListMock : IDList<NodeObjectType, threadSafe, ownsNodes, supportRecursiveLock> {
        std::atomic_flag &getLockedRef() { return this->locked; }
        std::atomic<std::thread::id> &getLockOwnerRef() { return this->lockOwner; }
        using ListenerT = void (*)(IDList<NodeObjectType, threadSafe, ownsNodes, supportRecursiveLock> &list);
        ListenerT &getSpinLockedListenerRef() { return this->spinLockedListener; }

        int32_t lockLoopCount = 0;
        static void listener(IDList<NodeObjectType, threadSafe, ownsNodes, supportRecursiveLock> &list) {
            ListMock &l = reinterpret_cast<ListMock &>(list);
            EXPECT_LT(0, l.lockLoopCount);
            --l.lockLoopCount;
            if (l.lockLoopCount == 0) {
                l.locked.clear(); // unlock
            }
        }

        void callNotifySpinlockedListener() {
            this->notifySpinLocked();
        }
    };

    ListMock l;
    l.getSpinLockedListenerRef() = &ListMock::listener;
    l.lockLoopCount = 1;
    l.callNotifySpinlockedListener();
    EXPECT_EQ(0, l.lockLoopCount);
    l.getSpinLockedListenerRef() = nullptr;
    l.callNotifySpinlockedListener();
    EXPECT_EQ(0, l.lockLoopCount);

    std::thread::id invalidThreadId;
    l.getLockedRef().test_and_set(std::memory_order_seq_cst);
    l.getLockOwnerRef() = invalidThreadId;
    l.getSpinLockedListenerRef() = &ListMock::listener;
    constexpr auto spinlockLoopCount = 10;
    l.lockLoopCount = spinlockLoopCount;
    auto ret = l.detachNodes();

    EXPECT_EQ(0, l.lockLoopCount);
    EXPECT_EQ(nullptr, ret);
}

TEST(IDList, GivenLockedIDListWhenProcessLockedIsUsedThenWaitsInSpinlock) {
    testIDListSpinlock<DummyDNode, true, false, true>();
    testIDListSpinlock<DummyDNode, true, false, false>();
}

template <typename NodeObjectType, bool threadSafe, bool ownsNodes, bool supportRecursiveLock>
void testIDListUnlockOnException() {
    using ExType = std::runtime_error;

    struct ListMock : IDList<NodeObjectType, threadSafe, ownsNodes, supportRecursiveLock> {
        void throwExFromLock() {
            this->template processLocked<ListMock, &ListMock::throwExFromLockImpl>(nullptr, nullptr);
        }

        std::atomic_flag &getLockedRef() { return this->locked; }

      protected:
        NodeObjectType *throwExFromLockImpl(NodeObjectType *, void *) {
            throw ExType{""};
        }
    };

    ListMock l;
    EXPECT_THROW(l.throwExFromLock(), ExType);

    EXPECT_FALSE(l.getLockedRef().test_and_set(std::memory_order_seq_cst));
}

TEST(IDList, GivenInsideLockWhenExceptionIsThrownThenUnlocksBeforeRethrowingException) {
    testIDListUnlockOnException<DummyDNode, true, false, true>();
    testIDListUnlockOnException<DummyDNode, true, false, false>();
}

template <bool threadSafe>
void iDRefListTestPushFrontOne() {
    auto list = std::unique_ptr<IDRefList<DummyDNode, threadSafe, true>>(new IDRefList<DummyDNode, threadSafe, true>());
    ASSERT_TRUE(list->peekIsEmpty());

    DummyDNode node1;
    node1.prev = nullptr; // garbage
    node1.next = nullptr; // garbage
    list->pushRefFrontOne(node1);
    ASSERT_FALSE(list->peekIsEmpty());
    ASSERT_EQ(&node1, list->peekHead()->ref);
    ASSERT_EQ(nullptr, node1.next);
    ASSERT_EQ(nullptr, node1.prev);

    DummyDNode node2;
    node2.prev = nullptr; // garbage
    node2.next = nullptr; // garbage
    list->pushRefFrontOne(node2);
    ASSERT_FALSE(list->peekIsEmpty());
    ASSERT_EQ(&node2, list->peekHead()->ref);
    ASSERT_EQ(nullptr, node2.prev);
    ASSERT_EQ(nullptr, node2.next);

    ASSERT_EQ(&node2, list->peekHead()->ref);
    ASSERT_EQ(&node1, list->peekHead()->next->ref);
    ASSERT_EQ(&node2, list->peekTail()->prev->ref);

    list.reset();
}

TEST(IDRefList, GivenThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iDRefListTestPushFrontOne<true>();
}

TEST(IDRefList, GivenNonThreadSafeWhenPushingFrontOneThenResultIsCorrect) {
    iDRefListTestPushFrontOne<false>();
}

TEST(IDRefList, GivenRecursiveLockWhenExecutingThenListIsNotEmpty) {
    class RecursiveLockList : public IDList<DummyDNode, true, true, true> {
      public:
        void execute() {
            processLocked<RecursiveLockList, &RecursiveLockList::executeImpl>(nullptr, nullptr);
        }

      protected:
        DummyDNode *executeImpl(DummyDNode *, void *data) {
            this->pushFrontOne(*(new DummyDNode())); // recursive lock
            return nullptr;
        }
    };

    RecursiveLockList list;

    ASSERT_TRUE(list.peekIsEmpty());
    list.execute();
    ASSERT_FALSE(list.peekIsEmpty());
}

// Checks if a pointer is contained (in terms of continous memory) within given object
template <typename ContainerType>
bool contains(const ContainerType *container, const void *ptr) {
    uintptr_t base = (intptr_t)container;
    uintptr_t tested = (intptr_t)ptr;
    size_t elementSize = sizeof(decltype(*container->begin()));
    return (tested >= base) && (tested + elementSize <= base + sizeof(ContainerType));
}

TEST(StackVec, WhenStackVecIsCreatedThenItIsCorrectlyPopulated) {
    using Type = int;
    static const uint32_t srcSize = 10;
    static const uint32_t overReserve = 3;
    static const uint32_t underReserve = 3;
    std::vector<Type> src(srcSize);
    for (uint32_t i = 0; i < srcSize; ++i) {
        src[i] = i;
    }

    ASSERT_GE(sizeof(StackVec<Type, srcSize>), sizeof(Type) * srcSize);

    StackVec<Type, srcSize + overReserve> bigger(src.begin(), src.end());
    StackVec<Type, srcSize> exact(src.begin(), src.end());
    StackVec<Type, srcSize - underReserve> smaller(src.begin(), src.end());

    ASSERT_EQ(srcSize, bigger.size());
    ASSERT_EQ(srcSize, exact.size());
    ASSERT_EQ(srcSize, smaller.size());

    ASSERT_EQ(srcSize + overReserve, bigger.capacity());
    ASSERT_EQ(srcSize, exact.capacity());
    ASSERT_TRUE(smaller.capacity() >= srcSize);

    // check if data was copied successfully
    ASSERT_EQ(0, memcmp(&*src.begin(), &*bigger.begin(), sizeof(Type) * srcSize));
    ASSERT_EQ(0, memcmp(&*src.begin(), &*exact.begin(), sizeof(Type) * srcSize));
    ASSERT_EQ(0, memcmp(&*src.begin(), &*smaller.begin(), sizeof(Type) * srcSize));

    // make sure data is allocated within the object (e.g. on stack) if the container is big enough
    ASSERT_TRUE(contains(&bigger, &*bigger.begin()));
    ASSERT_TRUE(contains(&exact, &*exact.begin()));
    ASSERT_FALSE(contains(&smaller, &*smaller.begin()));

    StackVec<Type, 4> withInitList{1, 2, 3, 5};
    EXPECT_EQ(1, withInitList[0]);
    EXPECT_EQ(2, withInitList[1]);
    EXPECT_EQ(3, withInitList[2]);
    EXPECT_EQ(5, withInitList[3]);
}

TEST(StackVec, GivenInitialSizeWhenCreatingThenResizeOccurs) {
    StackVec<int, 5> vec1(10);
    EXPECT_EQ(10U, vec1.size());
}

TEST(StackVec, WhenCopyingByConstructorThenCopyIsCorrect) {
    using Type = int;
    constexpr size_t sizeBase = 7;
    constexpr size_t sizeSmaller = 5;
    constexpr size_t sizeBigger = 9;

    size_t sizeConfigs[] = {0, sizeSmaller, sizeBase, sizeBigger};
    auto it = 0;
    for (auto sc : sizeConfigs) {
        ++it;
        StackVec<Type, sizeBase> src;
        Type refData[sizeBigger];
        for (uint32_t i = 0; i < sc; ++i) {
            refData[i] = i * it;
            src.push_back(refData[i]);
        }

        StackVec<Type, sizeBase> dst(src);
        ASSERT_EQ(sc, src.size());
        ASSERT_EQ(sc, dst.size());
        EXPECT_EQ(0, memcmp(refData, &*src.begin(), sc));
        EXPECT_EQ(0, memcmp(refData, &*dst.begin(), sc));
    }
}

TEST(StackVec, WhenCopyingByAssignmentThenCopyIsCorrect) {
    using Type = int;
    constexpr size_t sizeBase = 7;
    constexpr size_t sizeSmaller = 5;
    constexpr size_t sizeBigger = 9;

    size_t sizeConfigs[] = {0, sizeSmaller, sizeBase, sizeBigger};
    auto it = 0;
    for (auto dstSc : sizeConfigs) {
        for (auto srcSc : sizeConfigs) {
            ++it;
            StackVec<Type, sizeBase> src;
            Type refData[sizeBigger];
            for (uint32_t i = 0; i < srcSc; ++i) {
                refData[i] = i * it;
                src.push_back(refData[i]);
            }

            StackVec<Type, sizeBase> dst;
            for (uint32_t i = 0; i < dstSc; ++i) {
                dst.push_back(~i);
            }

            ASSERT_EQ(srcSc, src.size());
            ASSERT_EQ(dstSc, dst.size());

            dst = src;

            ASSERT_EQ(srcSc, src.size());
            ASSERT_EQ(srcSc, dst.size());
            EXPECT_EQ(0, memcmp(refData, &*src.begin(), srcSc));
            EXPECT_EQ(0, memcmp(refData, &*dst.begin(), srcSc));
        }
    }
}

TEST(StackVec, WhenMovingThenObjectIsCorrect) {
    using Type = int;
    constexpr size_t sizeBase = 7;
    constexpr size_t sizeSmaller = 5;
    constexpr size_t sizeBigger = 9;

    size_t sizeConfigs[] = {0, sizeSmaller, sizeBase, sizeBigger};
    auto it = 0;
    for (auto sc : sizeConfigs) {
        ++it;
        StackVec<Type, sizeBase> src;
        Type refData[sizeBigger];
        for (uint32_t i = 0; i < sc; ++i) {
            refData[i] = i * it;
            src.push_back(refData[i]);
        }

        StackVec<Type, sizeBase> dst(std::move(src));
        ASSERT_EQ(sc, dst.size());
        EXPECT_EQ(0, memcmp(&*refData, &*dst.begin(), sc));
    }
}

TEST(StackVec, WhenMovingByAssignmentThenObjectIsCorrect) {
    using Type = int;
    constexpr size_t sizeBase = 7;
    constexpr size_t sizeSmaller = 5;
    constexpr size_t sizeBigger = 9;

    size_t sizeConfigs[] = {0, sizeSmaller, sizeBase, sizeBigger};
    auto it = 0;
    for (auto dstSc : sizeConfigs) {
        for (auto srcSc : sizeConfigs) {
            ++it;
            StackVec<Type, sizeBase> src;
            Type refData[sizeBigger];
            for (uint32_t i = 0; i < srcSc; ++i) {
                refData[i] = i * it;
                src.push_back(refData[i]);
            }

            StackVec<Type, sizeBase> dst;
            for (uint32_t i = 0; i < dstSc; ++i) {
                dst.push_back(~i);
            }

            ASSERT_EQ(srcSc, src.size());
            ASSERT_EQ(dstSc, dst.size());

            dst = std::move(src);

            ASSERT_EQ(srcSc, dst.size());
            EXPECT_EQ(0, memcmp(refData, &*dst.begin(), srcSc));
        }
    }
}

TEST(StackVec, WhenStackVecIsCreatedThenItIsCorrectlyAligned) {
    StackVec<uint16_t, 4> s16;
    StackVec<uint32_t, 4> s32;
    StackVec<uint64_t, 4> s64;

    static_assert(sizeof(s16) <= 24u, "");
    static_assert(sizeof(s32) <= 32u, "");
    static_assert(sizeof(s64) <= 48u, "");

    s16.push_back(0);
    s32.push_back(0);
    s64.push_back(0);

    EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(s16.begin()) % 2);
    EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(s32.begin()) % 4);
    EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(s64.begin()) % 8);
}

TEST(StackVec, WhenPushingBackThenContentsAreCorrect) {
    using Type = uint32_t;
    StackVec<Type, 5> v;
    ASSERT_EQ(0U, v.size());

    for (uint32_t i = 0; i < 5; ++i) {
        v.push_back(i);
    }
    ASSERT_EQ(5U, v.size());
    ASSERT_TRUE(contains(&v, &*v.begin()));
    for (uint32_t i = 0; i < v.size(); ++i) {
        ASSERT_EQ(i, v[i]);
    }

    v.push_back(5);
    ASSERT_EQ(6U, v.size());
    ASSERT_FALSE(contains(&v, &*v.begin()));
    for (uint32_t i = 0; i < v.size(); ++i) {
        ASSERT_EQ(i, v[i]);
    }

    for (uint32_t i = 0; i < 5; ++i) {
        v.push_back(v[v.size() - 1] + 1);
    }

    for (uint32_t i = 0; i < v.size(); ++i) {
        ASSERT_EQ(i, v[i]);
    }

    ASSERT_FALSE(contains(&v, &*v.begin()));
}

TEST(StackVec, whenSwapThenContentsAreExchanged) {
    using Type = uint32_t;
    StackVec<Type, 5> vector1;
    StackVec<Type, 5> vector2;

    for (uint32_t i = 0; i < 3; ++i) {
        vector1.push_back(i);
    }

    for (uint32_t i = 0; i < 5; ++i) {
        vector2.push_back(i);
    }

    vector1.swap(vector2);
    EXPECT_EQ(5u, vector1.size());
    EXPECT_EQ(3u, vector2.size());
    EXPECT_EQ(4u, vector1.at(4u));

    vector1.swap(vector2);
    EXPECT_EQ(3u, vector1.size());
    EXPECT_EQ(5u, vector2.size());
}

TEST(StackVec, whenDynamicStorageSwapThenContentsAreExchanged) {
    using Type = uint32_t;
    StackVec<Type, 1> vector1;
    StackVec<Type, 1> vector2;

    for (uint32_t i = 0; i < 3; ++i) {
        vector1.push_back(i);
    }

    for (uint32_t i = 0; i < 5; ++i) {
        vector2.push_back(i);
    }

    vector1.swap(vector2);
    EXPECT_EQ(5u, vector1.size());
    EXPECT_EQ(3u, vector2.size());
    EXPECT_EQ(4u, vector1.at(4u));

    vector1.swap(vector2);
    EXPECT_EQ(3u, vector1.size());
    EXPECT_EQ(5u, vector2.size());
}

TEST(StackVec, whenDynamicToNonDynamicStorageSwapThenContentsAreExchanged) {
    using Type = uint32_t;
    StackVec<Type, 1> vector1;
    StackVec<Type, 5> vector2;

    for (uint32_t i = 0; i < 3; ++i) {
        vector1.push_back(i);
    }

    for (uint32_t i = 0; i < 5; ++i) {
        vector2.push_back(i);
    }

    vector1.swap(vector2);
    EXPECT_EQ(5u, vector1.size());
    EXPECT_EQ(3u, vector2.size());
    EXPECT_EQ(4u, vector1.at(4u));

    vector1.swap(vector2);
    EXPECT_EQ(3u, vector1.size());
    EXPECT_EQ(5u, vector2.size());
}

TEST(StackVecPopBack, GivenNonEmptyStackVecThenRemoveSingleElementFromTheEnd) {
    using Type = uint32_t;
    StackVec<Type, 2> v;
    ASSERT_EQ(2U, v.capacity());
    v.push_back(3);
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ(3U, *v.rbegin());
    v.push_back(5);
    EXPECT_EQ(2U, v.size());
    EXPECT_EQ(5U, *v.rbegin());

    v.pop_back();
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ(3U, *v.rbegin());
    v.pop_back();
    EXPECT_EQ(0U, v.size());

    v.push_back(3);
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ(3U, *v.rbegin());
    v.push_back(5);
    EXPECT_EQ(2U, v.size());
    EXPECT_EQ(5U, *v.rbegin());
    v.push_back(7);
    EXPECT_EQ(3U, v.size());
    EXPECT_EQ(7U, *v.rbegin());
    v.push_back(11);
    EXPECT_EQ(4U, v.size());
    EXPECT_EQ(11U, *v.rbegin());

    v.pop_back();
    EXPECT_EQ(3U, v.size());
    EXPECT_EQ(7U, *v.rbegin());
    v.pop_back();
    EXPECT_EQ(2U, v.size());
    EXPECT_EQ(5U, *v.rbegin());
    v.pop_back();
    EXPECT_EQ(1U, v.size());
    EXPECT_EQ(3U, *v.rbegin());
    v.pop_back();
    EXPECT_EQ(0U, v.size());
}

TEST(StackVec, WhenReservingThenCapacityIncreasesAndSizeDoesNot) {
    using Type = uint32_t;
    StackVec<Type, 5> v;
    ASSERT_EQ(5U, v.capacity());
    v.push_back(3);
    v.push_back(7);
    ASSERT_EQ(2U, v.size());
    v.reserve(10);
    ASSERT_LE(10U, v.capacity());
    ASSERT_EQ(2U, v.size());
    ASSERT_EQ(3U, v[0]);
    ASSERT_EQ(7U, v[1]);
    ASSERT_FALSE(contains(&v, &*v.begin()));

    v.reserve(1024);
    ASSERT_LE(1024U, v.capacity());
}

TEST(StackVec, WhenResizingThenElementsAreCorrectlyManaged) {
    struct Element {
        Element()
            : v(7) {
            data = new int[100];
        }

        Element(int v)
            : v(v) {
            data = new int[100];
        }

        ~Element() {
            if (data != nullptr) {
                delete[] data;
                data = nullptr;
            }
            v = -77;
        }

        Element(const Element &rhs) {
            this->v = rhs.v;
            this->data = new int[100];
        }

        Element(Element &&rhs) noexcept {
            this->v = rhs.v;
            this->data = rhs.data;
            rhs.data = nullptr;
        }

        Element &operator=(const Element &rhs) {
            this->v = rhs.v;
            delete[] this->data;
            this->data = new int[100];
            return *this;
        }

        Element &operator=(Element &&rhs) noexcept {
            this->v = rhs.v;
            delete[] this->data;
            this->data = rhs.data;
            rhs.data = nullptr;
            return *this;
        }

        int v = 9;
        int *data = nullptr;
    };

    StackVec<Element, 5> vec;
    vec.resize(1);
    ASSERT_EQ(1U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_TRUE(contains(&vec, &*vec.begin()));

    vec.resize(3, Element(11));
    ASSERT_EQ(3U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_EQ(11, vec[1].v);
    EXPECT_EQ(11, vec[2].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_NE(nullptr, vec[1].data);
    EXPECT_NE(nullptr, vec[2].data);
    EXPECT_TRUE(contains(&vec, &*vec.begin()));

    vec.resize(2);
    ASSERT_EQ(2U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_EQ(11, vec[1].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_NE(nullptr, vec[1].data);
    EXPECT_TRUE(contains(&vec, &*vec.begin()));

    vec.resize(7);
    ASSERT_EQ(7U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_EQ(11, vec[1].v);
    EXPECT_EQ(7, vec[2].v);
    EXPECT_EQ(7, vec[3].v);
    EXPECT_EQ(7, vec[4].v);
    EXPECT_EQ(7, vec[5].v);
    EXPECT_EQ(7, vec[6].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_NE(nullptr, vec[1].data);
    EXPECT_NE(nullptr, vec[2].data);
    EXPECT_NE(nullptr, vec[3].data);
    EXPECT_NE(nullptr, vec[4].data);
    EXPECT_NE(nullptr, vec[5].data);
    EXPECT_NE(nullptr, vec[6].data);
    EXPECT_FALSE(contains(&vec, &*vec.begin()));

    vec.resize(1);
    ASSERT_EQ(1U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_FALSE(contains(&vec, &*vec.begin()));

    vec.resize(3, Element(55));
    ASSERT_EQ(3U, vec.size());
    EXPECT_EQ(7, vec[0].v);
    EXPECT_EQ(55, vec[1].v);
    EXPECT_EQ(55, vec[2].v);
    EXPECT_NE(nullptr, vec[0].data);
    EXPECT_NE(nullptr, vec[1].data);
    EXPECT_NE(nullptr, vec[2].data);
    EXPECT_FALSE(contains(&vec, &*vec.begin()));
}

TEST(StackVec, WhenIteratingThenCorrectElementsAreReturned) {
    using Type = int;
    StackVec<Type, 5> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);
    v.push_back(5);
    ASSERT_TRUE(contains(&v, &*v.begin()));
    Type sum = 0;
    for (auto e : v) {
        sum += e;
    }
    ASSERT_EQ(15, sum);

    v.push_back(6);
    v.push_back(7);
    v.push_back(8);
    ASSERT_FALSE(contains(&v, &*v.begin()));
    sum = 0;
    for (auto e : v) {
        sum += e;
    }
    ASSERT_EQ(36, sum);
}

TEST(StackVec, WhenClearingThenAllElementsAreRemoved) {
    uint32_t destructorCounter = 0;
    DummyFNode nd1(&destructorCounter);
    DummyFNode nd2(&destructorCounter);
    DummyFNode nd3(&destructorCounter);
    StackVec<DummyFNode, 3> v;
    EXPECT_TRUE(v.empty());
    v.push_back(nd1);
    v.push_back(nd2);
    v.push_back(nd3);
    ASSERT_EQ(0U, destructorCounter);
    ASSERT_EQ(3U, v.size());
    EXPECT_FALSE(v.empty());
    v.clear();
    ASSERT_EQ(3U, destructorCounter);
    ASSERT_EQ(0U, v.size());
    EXPECT_TRUE(v.empty());

    StackVec<DummyFNode, 1> v2;
    v2.push_back(nd1);
    v2.push_back(nd2);
    v2.push_back(nd3);
    destructorCounter = 0;
    ASSERT_EQ(3U, v2.size());
    v2.clear();
    ASSERT_EQ(3U, destructorCounter);
    ASSERT_EQ(0U, v2.size());
}

TEST(StackVec, WhenGettingReverseThenCorrectElementIsReturned) {
    using VecType = StackVec<int, 1>;
    VecType v;
    v.push_back(5);

    ASSERT_EQ(v.begin(), v.rend().base());
    ASSERT_EQ(v.end(), v.rbegin().base());
    ASSERT_EQ(v.begin(), v.crend().base());
    ASSERT_EQ(v.end(), v.crbegin().base());
}

TEST(StackVec, WhenUsingConstMemberFunctionsThenResultsAreCorrect) {
    using VecType = StackVec<int, 3>;
    VecType v;
    ASSERT_EQ(v.begin(), ((const VecType *)&v)->begin());
    ASSERT_EQ(v.end(), ((const VecType *)&v)->end());
    ASSERT_EQ(v.begin(), v.end());
    v.push_back(5);
    ASSERT_EQ(v.begin(), ((const VecType *)&v)->begin());
    ASSERT_EQ(v.end(), ((const VecType *)&v)->end());
    ASSERT_NE(v.begin(), v.end());
    ASSERT_EQ(&v[0], &((const VecType *)&v)->operator[](0));

    v.push_back(6);
    v.push_back(1);
    v.push_back(3);
    ASSERT_EQ(v.begin(), ((const VecType *)&v)->begin());
    ASSERT_EQ(v.end(), ((const VecType *)&v)->end());
    ASSERT_NE(v.begin(), v.end());
    ASSERT_EQ(&v[0], &((const VecType *)&v)->operator[](0));
}

TEST(StackVec, GivenComplexElementsWhenCreatingThenCreationSucceeds) {
    std::vector<int> src(100, 5);

    static const int elementsCount = 10;
    auto v = std::unique_ptr<StackVec<std::vector<int>, elementsCount>>(new StackVec<std::vector<int>, elementsCount>());
    for (int i = 0; i < elementsCount * 2; ++i) {
        v->push_back(src);
    }

    v.reset();
}

TEST(StackVec, WhenGettingIteratorThenIteratorTypeIsCorrect) {
    struct S {
    };

    using StackVecT = StackVec<S, 5>;
    using iterator = StackVecT::iterator;
    using const_iterator = StackVecT::const_iterator;

    StackVecT vec;
    const StackVecT &constVec = vec;

    static_assert(std::is_same<iterator, decltype(vec.begin())>::value, "iterator types do not match");
    static_assert(std::is_same<iterator, decltype(vec.end())>::value, "iterator types do not match");
    static_assert(std::is_same<const_iterator, decltype(constVec.begin())>::value, "iterator types do not match");
    static_assert(std::is_same<const_iterator, decltype(constVec.end())>::value, "iterator types do not match");
}

TEST(StackVec, GivenStackVecsOfDifferentSizesWhenComparingThenEqualReturnsFalse) {
    StackVec<int, 5> longer;
    longer.resize(4, 0);

    StackVec<int, 10> shorter;
    shorter.resize(2, 0);

    EXPECT_FALSE(longer == shorter);
    EXPECT_FALSE(shorter == longer);
    EXPECT_TRUE(longer != shorter);
    EXPECT_TRUE(shorter != longer);
}

TEST(StackVec, GivenStackVecsContainingDifferentDataWhenComparingThenEqualReturnsFalse) {
    char dataA[] = {0, 1, 3, 4, 5};
    char dataB[] = {0, 1, 3, 5, 4};

    StackVec<char, 10> vecA{dataA, dataA + sizeof(dataA)};
    StackVec<char, 15> vecB{dataB, dataB + sizeof(dataB)};
    EXPECT_FALSE(vecA == vecB);
    EXPECT_TRUE(vecA != vecB);
}

TEST(StackVec, GivenStackVecsAreEmptyWhenComparingThenEqualReturnsTrue) {
    StackVec<char, 10> vecA;
    StackVec<char, 15> vecB;

    EXPECT_TRUE(vecA == vecB);
    EXPECT_FALSE(vecA != vecB);
}

TEST(StackVec, GivenStackVecsContainingSameDataWhenComparingThenEqualReturnsTrue) {
    char dataA[] = {0, 1, 3, 4, 5};
    char dataB[] = {0, 1, 3, 4, 5};

    StackVec<char, 10> vecA{dataA, dataA + sizeof(dataA)};
    StackVec<char, 15> vecB{dataB, dataB + sizeof(dataB)};
    EXPECT_TRUE(vecA == vecB);
    EXPECT_FALSE(vecA != vecB);
}

TEST(StackVec, WhenCallingDataThenVectorDataIsReturned) {
    char dataA[] = {0, 1, 3, 4, 5};
    char dataB[] = {5, 4, 3, 2, 1};

    StackVec<char, 1> stackVecA{dataA, dataA + sizeof(dataA)};
    const StackVec<char, 5> stackVecB{dataB, dataB + sizeof(dataB)};

    EXPECT_TRUE(stackVecA.usesDynamicMem());
    EXPECT_FALSE(stackVecB.usesDynamicMem());

    auto stackVecAData = stackVecA.data();
    auto stackVecBData = stackVecB.data();
    for (size_t i = 0; i < 5; i++) {
        EXPECT_EQ(dataA[i], stackVecAData[i]);
        EXPECT_EQ(dataB[i], stackVecBData[i]);
    }
}

TEST(StackVec, GivenStackVecWithDynamicMemWhenSelfAssignedThenMemoryIsReused) {
    char dataA[] = {0, 1, 3, 4, 5};

    StackVec<char, 1> stackVecA{dataA, dataA + sizeof(dataA)};

    EXPECT_TRUE(stackVecA.usesDynamicMem());

    {

        auto currentMemory = stackVecA.data();
        auto currentSize = stackVecA.size();
        auto &stackVecB = stackVecA;
        stackVecA = stackVecB;

        EXPECT_EQ(currentMemory, stackVecA.data());
        EXPECT_EQ(currentSize, stackVecA.size());

        stackVecA.push_back(7);
        currentMemory = stackVecA.data();
        EXPECT_EQ(7, currentMemory[5]);
        for (size_t i = 0; i < 5; i++) {
            EXPECT_EQ(dataA[i], currentMemory[i]);
        }
    }

    {

        auto currentMemory = stackVecA.data();
        auto currentSize = stackVecA.size();
        auto &stackVecB = stackVecA;
        stackVecA = std::move(stackVecB);

        EXPECT_EQ(currentMemory, stackVecA.data());
        EXPECT_EQ(currentSize, stackVecA.size());

        stackVecA.push_back(7);
        currentMemory = stackVecA.data();
        EXPECT_EQ(7, currentMemory[5]);
        for (size_t i = 0; i < 5; i++) {
            EXPECT_EQ(dataA[i], currentMemory[i]);
        }
    }
}

TEST(StackVec, whenPushingUniqueToRootDeviceIndicesContainerThenOnlyUniqueValuesArePopulated) {
    StackVec<uint32_t, 8> input = {6, 5, 4, 3, 2, 1, 5, 4, 5, 4, 2, 4, 3, 1, 6, 1, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0};
    ASSERT_EQ(input.size(), 27u);

    RootDeviceIndicesContainer rootDeviceIndices{};

    for (auto &index : input) {
        rootDeviceIndices.pushUnique(index);
    }

    EXPECT_EQ(rootDeviceIndices.size(), 17u);
    const RootDeviceIndicesContainer expectedContainer = {6, 5, 4, 3, 2, 1, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0};
    for (auto i = 0u; i < rootDeviceIndices.size(); i++) {
        EXPECT_EQ(rootDeviceIndices[i], expectedContainer[i]);
    }
}

TEST(StackVec, WhenInsertingAtGivenPositionThenOrderIsMaintained) {
    {
        StackVec<int, 10> tested;
        tested.insert(tested.end(), 12);

        StackVec<int, 10> expected{12};
        EXPECT_EQ(expected, tested);
    }
    {
        StackVec<int, 10> tested{3, 5, 7, 11};
        tested.insert(tested.end(), 12);
        tested.insert(tested.begin(), 2);
        tested.insert(tested.begin() + 2, 4);

        StackVec<int, 10> expected{2, 3, 4, 5, 7, 11, 12};
        EXPECT_EQ(expected, tested);
    }
    {
        StackVec<int, 1> testedB{3};
        testedB.insert(testedB.begin(), 2);
        StackVec<int, 1> testedE{2};
        testedE.insert(testedE.end(), 3);

        StackVec<int, 10> expected{2, 3};
        EXPECT_EQ(expected, testedB);
        EXPECT_EQ(expected, testedE);
    }
    {
        StackVec<int, 4> tested{3, 5, 7, 11};
        tested.insert(tested.end(), 12);
        tested.insert(tested.begin(), 2);
        tested.insert(tested.begin() + 2, 4);

        StackVec<int, 10> expected{2, 3, 4, 5, 7, 11, 12};
        EXPECT_EQ(expected, tested);
    }
}

int sum(ArrayRef<int> a) {
    int sum = 0;
    for (auto v : a) {
        sum += v;
    }
    return sum;
}

TEST(ArrayRef, WhenArrayRefIsCreatedThenBehaviorIsCorrect) {
    StackVec<int, 5> sv;
    sv.push_back(0);
    sv.push_back(1);
    sv.push_back(2);
    sv.push_back(3);
    sv.push_back(4);
    ASSERT_EQ(10, sum(sv));
    ASSERT_EQ(10, sum(ArrayRef<int>(sv.begin(), sv.end())));

    int carray[] = {5, 6, 7, 8, 9};
    ASSERT_EQ(35, sum(carray));

    ArrayRef<int> ar2;
    EXPECT_TRUE(ar2.empty());
    ASSERT_EQ(0U, ar2.size());
    ASSERT_EQ(nullptr, ar2.begin());
    ASSERT_EQ(nullptr, ar2.end());

    ar2 = carray;
    EXPECT_FALSE(ar2.empty());
    ASSERT_EQ(sizeof(carray) / sizeof(carray[0]), ar2.size());
    ASSERT_EQ(35, sum(ar2));

    std::vector<int> stdv(ar2.begin(), ar2.end());
    ASSERT_EQ(35, sum(stdv));

    ArrayRef<int> ar1 = sv;
    ar1.swap(ar2);
    ASSERT_EQ(10, sum(ar2));
    ASSERT_EQ(35, sum(ar1));

    ASSERT_EQ(ar1.begin(), ((const ArrayRef<int> *)&ar1)->begin());
    ASSERT_EQ(ar1.end(), ((const ArrayRef<int> *)&ar1)->end());

    ASSERT_EQ(ar1.size(), stdv.size());
    for (uint32_t i = 0; i < ar1.size(); ++i) {
        ASSERT_EQ(ar1[i], stdv[i]);
    }

    ASSERT_EQ(&ar1[3], &((const ArrayRef<int> *)&ar1)->operator[](3));
    sv.resize(30);
    ASSERT_EQ(sv.capacity(), 30u);

    ArrayRef<int> ar3{carray, sizeof(carray) / sizeof(carray[0])};
    ASSERT_EQ(35, sum(ar3));
}

TEST(ArrayRef, GivenEmptyContainerThenArrayRefIsEmpty) {
    StackVec<int, 5> stackVec;
    ArrayRef<int> arrayRef(stackVec);
    EXPECT_TRUE(arrayRef.empty());

    const StackVec<int, 5> &constStackVec = stackVec;
    ArrayRef<const int> constArrayRef(constStackVec);
    EXPECT_TRUE(constArrayRef.empty());
}

TEST(ArrayRef, WhenImplicitlyCovertingToConstThenResultIsCorrect) {
    int carray[] = {5, 6, 7, 8, 9};
    ArrayRef<int> arrayRef(carray);
    ArrayRef<const int> constArrayRef = arrayRef;
    EXPECT_EQ(arrayRef.begin(), constArrayRef.begin());
    EXPECT_EQ(arrayRef.end(), constArrayRef.end());
}

TEST(ArrayRef, WhenGettingIteratorThenIteratorTypeIsCorrect) {
    struct S {
    };

    using ArrayT = ArrayRef<S>;
    using iterator = ArrayT::iterator;
    using const_iterator = ArrayT::const_iterator;

    ArrayT array;
    const ArrayT constArray;

    static_assert(std::is_same<iterator, decltype(array.begin())>::value, "iterator types do not match");
    static_assert(std::is_same<iterator, decltype(array.end())>::value, "iterator types do not match");
    static_assert(std::is_same<const_iterator, decltype(constArray.begin())>::value, "iterator types do not match");
    static_assert(std::is_same<const_iterator, decltype(constArray.end())>::value, "iterator types do not match");
}

TEST(ArrayRef, GivenArraysOfDifferentSizesWhenComparingThenEqualReturnsFalse) {
    char data[] = {0, 1, 3, 4, 5};

    ArrayRef<char> longer{data, sizeof(data)};
    ArrayRef<char> shorter{data, sizeof(data) - 1};
    EXPECT_FALSE(longer == shorter);
    EXPECT_FALSE(shorter == longer);
}

TEST(ArrayRef, GivenArraysContainingDifferentDataWhenComparingThenEqualReturnsFalse) {
    char dataA[] = {0, 1, 3, 4, 5};
    char dataB[] = {0, 1, 3, 5, 4};

    ArrayRef<char> arrayA{dataA, sizeof(dataA)};
    ArrayRef<char> arrayB{dataB, sizeof(dataB)};
    EXPECT_FALSE(arrayA == arrayB);
}

TEST(ArrayRef, GivenEmptyArraysWhenComparingThenEqualReturnsTrue) {
    ArrayRef<char> arrayA;
    ArrayRef<char> arrayB;

    EXPECT_TRUE(arrayA == arrayB);
}

TEST(ArrayRef, GivenArraysContainingSameDataWhenComparingThenEqualReturnsTrue) {
    char dataA[] = {0, 1, 3, 4, 5};
    char dataB[] = {0, 1, 3, 4, 5};

    ArrayRef<char> arrayA{dataA, sizeof(dataA)};
    ArrayRef<char> arrayB{dataB, sizeof(dataB)};
    EXPECT_TRUE(arrayA == arrayB);
}

TEST(Range, GivenRangeThenValidStandardIteratorsAreAvailable) {
    int tab[10] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    Range<int> range = tab;
    const Range<int> &constantRange = range;
    Range<int> emptyRange{nullptr, 0};
    EXPECT_EQ(0U, emptyRange.size());
    EXPECT_TRUE(emptyRange.empty());
    EXPECT_EQ(10U, constantRange.size());
    EXPECT_FALSE(constantRange.empty());

    auto rangeFwdIt = range.begin();
    auto rangeFwdEnd = range.end();
    auto rangeBackIt = range.rbegin();
    auto rangeBackEnd = range.rend();

    auto constantRangeFwdIt = constantRange.begin();
    auto constantRangeFwdEnd = constantRange.end();
    auto constantRangeBackIt = constantRange.rbegin();
    auto constantRangeBackEnd = constantRange.rend();
    for (int i = 0; i < 10; ++i, ++rangeFwdIt, ++rangeBackIt, ++constantRangeFwdIt, ++constantRangeBackIt) {
        EXPECT_EQ(tab[i], *rangeFwdIt) << " it : " << i;
        EXPECT_EQ(tab[i], *constantRangeFwdIt) << " it : " << i;
        EXPECT_NE(rangeFwdEnd, rangeFwdIt) << " it : " << i;
        EXPECT_NE(constantRangeFwdEnd, constantRangeFwdIt) << " it : " << i;

        EXPECT_EQ(tab[10 - 1 - i], *rangeBackIt) << " it : " << i;
        EXPECT_EQ(tab[10 - 1 - i], *constantRangeBackIt) << " it : " << i;
        EXPECT_NE(rangeBackEnd, rangeBackIt) << " it : " << i;
        EXPECT_NE(constantRangeBackEnd, constantRangeBackIt) << " it : " << i;
    }

    EXPECT_EQ(rangeFwdEnd, rangeFwdIt);
    EXPECT_EQ(constantRangeFwdEnd, constantRangeFwdIt);
    EXPECT_EQ(rangeBackEnd, rangeBackIt);
    EXPECT_EQ(constantRangeBackEnd, constantRangeBackIt);

    std::vector<int> vec(&tab[0], &tab[10]);
    Range<int> rangeFromVec = vec;
    EXPECT_EQ(&*vec.begin(), &*rangeFromVec.begin());
    EXPECT_EQ(&*vec.rbegin(), &*rangeFromVec.rbegin());
}

TEST(ArrayRef, WhenFromAnyIsCalledThenPointerIsReinterpretedAndSizeIsAdjusted) {
    uint32_t x[2] = {};
    auto arrayRefU8 = ArrayRef<const uint8_t>::fromAny(x, 2);
    EXPECT_EQ(8U, arrayRefU8.size());
    EXPECT_EQ(reinterpret_cast<uint8_t *>(x), arrayRefU8.begin());
}

TEST(ArrayRef, WhenToArrayRefIsCalledThenPointerIsReinterpretedAndSizeIsAdjusted) {
    uint32_t x[2] = {};
    auto arrayRefU32 = ArrayRef<const uint32_t>::fromAny(x, 2);
    auto arrayRefU8 = arrayRefU32.toArrayRef<const uint8_t>();
    EXPECT_EQ(8U, arrayRefU8.size());
    EXPECT_EQ(reinterpret_cast<uint8_t *>(x), arrayRefU8.begin());
}

TEST(ArrayRef, WhenClearedThenEmpty) {
    uint32_t x[2] = {};
    auto arrayRefU32 = ArrayRef<const uint32_t>::fromAny(x, 2);
    arrayRefU32.clear();
    EXPECT_TRUE(arrayRefU32.empty());
}

TEST(LookupArrayFindGreaterEqual, WhenLookingForElementThenReturnFirstThatIsEqualOrGreater) {
    LookupArray<int, int, 3> arr({{{1, 10}, {4, 40}, {9, 90}}});
    auto res = arr.findGreaterEqual(0);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(10, res.value());

    res = arr.findGreaterEqual(1);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(10, res.value());

    res = arr.findGreaterEqual(2);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(40, res.value());

    res = arr.findGreaterEqual(3);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(40, res.value());

    res = arr.findGreaterEqual(4);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(40, res.value());

    res = arr.findGreaterEqual(9);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(90, res.value());

    res = arr.findGreaterEqual(10);
    EXPECT_FALSE(res.has_value());
}

TEST(LookupArrayLookUpGreaterEqual, WhenLookingForElementThenReturnFirstThatIsEqualOrGreater) {
    LookupArray<int, int, 3> arr({{{1, 10}, {4, 40}, {9, 90}}});
    auto res = arr.lookUpGreaterEqual(0);
    EXPECT_EQ(10, res);

    res = arr.lookUpGreaterEqual(1);
    EXPECT_EQ(10, res);

    res = arr.lookUpGreaterEqual(2);
    EXPECT_EQ(40, res);

    res = arr.lookUpGreaterEqual(3);
    EXPECT_EQ(40, res);

    res = arr.lookUpGreaterEqual(4);
    EXPECT_EQ(40, res);

    res = arr.lookUpGreaterEqual(9);
    EXPECT_EQ(90, res);

    EXPECT_THROW(res = arr.lookUpGreaterEqual(10), std::exception);
}

TEST(BitArray, WhenFfzIsCalledThenReturnsFirstZeroBitOrMinusAllBitAreSet) {
    {
        BitArray ba{3};
        ASSERT_EQ(3U, ba.length());
        auto pos = ba.ffz();
        EXPECT_EQ(0, pos);
        pos = ba.ffz();
        EXPECT_EQ(0, pos);
        ba[0] = true;
        pos = ba.ffz();
        EXPECT_EQ(1, pos);
        pos = ba.ffz();
        EXPECT_EQ(1, pos);
        ba[1] = true;
        pos = ba.ffz();
        EXPECT_EQ(2, pos);
        ba[2] = true;
        pos = ba.ffz();
        EXPECT_EQ(-1, pos);
    }
    {
        BitArray ba{64};
        ASSERT_EQ(64U, ba.length());
        for (int i = 0; i < 64; ++i) {
            ba[i] = true;
        }
        auto pos = ba.ffz();
        EXPECT_EQ(-1, pos);
    }
    {
        BitArray ba{4097};
        ASSERT_EQ(4097U, ba.length());
        for (int i = 0; i < 4095; ++i) {
            ba[i] = true;
        }
        auto pos = ba.ffz();
        EXPECT_EQ(4095, pos);
        ba[4095] = true;
        pos = ba.ffz();
        EXPECT_EQ(4096, pos);
        ba[4096] = true;
        pos = ba.ffz();
        EXPECT_EQ(-1, pos);

        ba[796] = false;
        pos = ba.ffz();
        EXPECT_EQ(796, pos);
    }
}

TEST(BitAllocator, WhenAllocateIsCalledThenAllocatesAndReturnsFirstFreeBitPositionOrMinusOneIfEmpty) {
    {
        BitAllocator ba{3};
        ASSERT_EQ(3U, ba.sizeInBits());
        auto pos = ba.allocate();
        EXPECT_EQ(0, pos);
        pos = ba.allocate();
        EXPECT_EQ(1, pos);
        pos = ba.allocate();
        EXPECT_EQ(2, pos);
        pos = ba.allocate();
        EXPECT_EQ(-1, pos);

        ba.free(1);
        pos = ba.allocate();
        EXPECT_EQ(1, pos);
        pos = ba.allocate();
        EXPECT_EQ(-1, pos);

        ba.free(0);
        pos = ba.allocate();
        EXPECT_EQ(0, pos);
        pos = ba.allocate();
        EXPECT_EQ(-1, pos);
    }
    {
        BitAllocator ba{64};
        ASSERT_EQ(64U, ba.sizeInBits());
        for (int i = 0; i < 64; ++i) {
            EXPECT_EQ(i, ba.allocate());
        }
        auto pos = ba.allocate();
        EXPECT_EQ(-1, pos);
    }
    {
        BitAllocator ba{4097};
        ASSERT_EQ(4097U, ba.sizeInBits());
        for (int i = 0; i < 4095; ++i) {
            EXPECT_EQ(i, ba.allocate());
        }
        auto pos = ba.allocate();
        EXPECT_EQ(4095, pos);
        pos = ba.allocate();
        EXPECT_EQ(4096, pos);
        pos = ba.allocate();
        EXPECT_EQ(-1, pos);

        ba.free(796);
        pos = ba.allocate();
        EXPECT_EQ(796, pos);
    }
}

TEST(OpaqueArray, WhenAllocateIsCalledThenAllocatesAndReturnsFirstFreeElementOrNullIfEmpty) {
    struct GraphicsAllocationHandleExample {
    } handle;
    double underlyingMemory[5] = {1, 2, 3, 4, 5};
    const size_t elementStride = 2 * sizeof(double);
    {
        OpaqueArrayElementAllocator<GraphicsAllocationHandleExample *> oa{&handle, underlyingMemory, elementStride, sizeof(underlyingMemory) / elementStride};
        EXPECT_EQ(underlyingMemory, oa.base());
        EXPECT_EQ(&handle, oa.handle());

        EXPECT_TRUE(oa.contains(underlyingMemory));
        EXPECT_TRUE(oa.contains(underlyingMemory + 1));
        EXPECT_TRUE(oa.contains(underlyingMemory + 2));
        EXPECT_TRUE(oa.contains(underlyingMemory + 3));
        EXPECT_FALSE(oa.contains(underlyingMemory + 4));

        auto el0 = oa.allocate();
        EXPECT_EQ(underlyingMemory, el0);
        EXPECT_EQ(el0, oa.element(0));

        auto el1 = oa.allocate();
        EXPECT_EQ(underlyingMemory + 2, el1);
        EXPECT_EQ(el1, oa.element(1));

        auto el2 = oa.allocate();
        EXPECT_EQ(nullptr, el2);

        oa.free(el0);
        el2 = oa.allocate();
        EXPECT_EQ(underlyingMemory, el2);
    }
}

TEST(OpaqueElementAllocator, WhenAllocateIsCalledThenAllocatesAndReturnsFirstFreeElementAndGrowsIfNeeded) {
    struct GraphicsAllocationHandleExample {};
    using AllocatorT = OpaqueElementAllocator<GraphicsAllocationHandleExample *>;
    using AllocationT = AllocatorT::AllocationT;

    std::set<GraphicsAllocationHandleExample *> graphicsAllocations;
    {
        const size_t chunkSize = 256;
        const size_t elementSize = 64;
        AllocatorT allocator(chunkSize, elementSize, AllocatorT::UnderlyingAllocatorT{.allocate = [&](size_t s, size_t a) -> AllocationT {
                                                                                          auto newGa = new GraphicsAllocationHandleExample;
                                                                                          graphicsAllocations.insert(newGa);
                                                                                          return {newGa, alignedMalloc(s, a)};
                                                                                      },
                                                                                      .free = [&](AllocationT alloc) {
                                                        UNRECOVERABLE_IF(1 != graphicsAllocations.count(alloc.first));
                                                        graphicsAllocations.erase(alloc.first);
                                                        delete alloc.first; 
                                                        alignedFree(alloc.second); }});

        auto elsPerChunk = chunkSize / elementSize;
        std::set<void *> allocated;
        int toAllocate = static_cast<int>(elsPerChunk * 3 + 1);
        for (int i = 0; i < toAllocate; ++i) {
            AllocationT el = allocator.allocate();
            EXPECT_NE(nullptr, el.first);
            EXPECT_NE(nullptr, el.second);
            EXPECT_EQ(1U, graphicsAllocations.count(el.first));
            EXPECT_EQ(0U, allocated.count(el.second));
            allocated.insert(el.second);

            EXPECT_TRUE(isAligned(reinterpret_cast<uintptr_t>(el.second), elementSize));
            EXPECT_EQ(alignUp(i + 1, elsPerChunk) / elsPerChunk, graphicsAllocations.size());
        }

        EXPECT_EQ(alignUp(toAllocate, elsPerChunk) / elsPerChunk, graphicsAllocations.size());
        for (auto ptr : allocated) {
            EXPECT_TRUE(allocator.contains(ptr));
        }
        int stackMem = 0;
        EXPECT_FALSE(allocator.contains(&stackMem));
        EXPECT_FALSE(allocator.free(&stackMem));

        EXPECT_EQ(alignUp(toAllocate, elsPerChunk) / elsPerChunk, graphicsAllocations.size());
        for (auto ptr : allocated) {
            EXPECT_TRUE(allocator.free(ptr));
        }

        allocated.clear();
        for (int i = 0; i < toAllocate; ++i) {
            AllocationT el = allocator.allocate();
            EXPECT_NE(nullptr, el.first);
            EXPECT_NE(nullptr, el.second);
            EXPECT_EQ(1U, graphicsAllocations.count(el.first));
            EXPECT_EQ(0U, allocated.count(el.second));
            allocated.insert(el.second);

            EXPECT_TRUE(isAligned(reinterpret_cast<uintptr_t>(el.second), elementSize));
        }
        EXPECT_EQ(alignUp(toAllocate, elsPerChunk) / elsPerChunk, graphicsAllocations.size()) << " alloc reuse failed";
    }
    EXPECT_EQ(0U, graphicsAllocations.size());
}
