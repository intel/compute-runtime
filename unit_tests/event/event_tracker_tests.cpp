/*
 * Copyright (c) 2018, Intel Corporation
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

#include "event_fixture.h"
#include "runtime/event/event_tracker.h"
#include "runtime/event/event.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

#include <array>
#include <functional>

TEST(EventsTracker, whenCallingGetEventsTrackerThenGetGlobalEventsTrackerInstance) {
    auto &evTracker1 = EventsTracker::getEventsTracker();
    auto &evTracker2 = EventsTracker::getEventsTracker();

    EXPECT_EQ(&evTracker1, &evTracker2);

    EventsTracker::shutdownGlobalEvTracker();
}

TEST(EventsTracker, whenCallLabelFunctionThenGetStringWithProperEventId) {
    UserEvent uEvent;

    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;

    EXPECT_STREQ("e0", EventsTracker::label(&uEvent, map).c_str());
}

TEST(EventsTracker, whenCallLabelFunctionWhenEventIsNotInMapThenGetStringWithoutId) {
    UserEvent uEvent;

    std::unordered_map<Event *, int64_t> map;

    EXPECT_STREQ("e", EventsTracker::label(&uEvent, map).c_str());
}

TEST(EventsTracker, whenCallLabelFunctionThenGetStringWithProperCmdqId) {
    CommandQueue cmdq;

    std::string expect = "cq" + std::to_string(reinterpret_cast<uintptr_t>(&cmdq));

    EXPECT_STREQ(expect.c_str(), EventsTracker::label(&cmdq).c_str());
}

TEST(EventsTracker, givenNullptrCmdqThenNotDumping) {
    CommandQueue *cmdq_ptr = nullptr;

    std::stringstream stream;
    std::set<CommandQueue *> dumped;

    EventsTracker::dumpQueue(cmdq_ptr, stream, dumped);

    EXPECT_STREQ("", stream.str().c_str());
}

TEST(EventsTracker, givenAlreadyDumpedCmdqThenNotDumping) {
    CommandQueue cmdq;

    std::stringstream stream;
    std::set<CommandQueue *> dumped;
    dumped.insert(&cmdq);

    EventsTracker::dumpQueue(&cmdq, stream, dumped);

    EXPECT_STREQ("", stream.str().c_str());
}

TEST(EventsTracker, givenCmqdWithTaskCountAndLevelNotReadyThenDumpingCmdqWithNotReadyLabels) {
    CommandQueue cmdq;
    cmdq.taskCount = Event::eventNotReady;
    cmdq.taskLevel = Event::eventNotReady;

    std::stringstream stream;
    std::set<CommandQueue *> dumped;

    EventsTracker::dumpQueue(&cmdq, stream, dumped);

    std::stringstream expected;
    expected << EventsTracker::label(&cmdq) << "[label=\"{------CmdQueue, ptr=" << &cmdq << "------|task count=NOT_READY, level=NOT_READY}\",color=blue];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, whenCallDumpQueueThenDumpingCmdqWithProperCountTaskAndLevelValues) {
    CommandQueue cmdq;
    cmdq.taskCount = 3;
    cmdq.taskLevel = 1;

    std::stringstream stream;
    std::set<CommandQueue *> dumped;

    EventsTracker::dumpQueue(&cmdq, stream, dumped);

    std::stringstream expected;
    expected << EventsTracker::label(&cmdq) << "[label=\"{------CmdQueue, ptr=" << &cmdq << "------|task count=3, level=1}\",color=blue];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, whenCallDumpEdgeThenGetStringWithProperLabelOfDumpedEdge) {
    UserEvent uEvent1;
    UserEvent uEvent2;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent1] = 0;
    map[&uEvent2] = 1;

    EventsTracker::dumpEdge(&uEvent1, &uEvent2, stream, map);

    EXPECT_STREQ("e0->e1;\n", stream.str().c_str());
}

TEST(EventsTracker, givenEventWithTaskLevelAndCountNotReadyThenDumpingNodeWithNotReadyLabels) {
    UserEvent uEvent;
    uEvent.taskLevel = Event::eventNotReady;
    uEvent.updateTaskCount(Event::eventNotReady);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;

    EventsTracker::dumpNode(&uEvent, stream, map);

    std::stringstream expected;
    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, whenCallDumpNodeFunctionThenDumpingNodeWithProperTaskLevelAndCountValues) {
    UserEvent uEvent;
    uEvent.taskLevel = 1;
    uEvent.updateTaskCount(1);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;

    EventsTracker::dumpNode(&uEvent, stream, map);

    std::stringstream expected;
    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_QUEUED|task count=1, level=1|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenNullptrEventThenNotDumpingNode) {
    UserEvent *uEvent = nullptr;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[uEvent] = 0;

    EventsTracker::dumpNode(uEvent, stream, map);

    EXPECT_STREQ("eNULL[label=\"{ptr=nullptr}\",color=red];\n", stream.str().c_str());
}

TEST(EventsTracker, givenEventAndUserEventThenDumpingNodeWithProperLabels) {
    UserEvent uEvent;
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    map[&event] = 1;

    EventsTracker::dumpNode(&uEvent, stream, map);

    std::stringstream expecteduEvent;
    expecteduEvent << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expecteduEvent.str().c_str(), stream.str().c_str());

    stream.str(std::string());
    EventsTracker::dumpNode(&event, stream, map);

    std::stringstream expectedEvent;
    expectedEvent << "e1[label=\"{-----------EVENT  ptr=" << &event
                  << "------|CL_COMMAND_NDRANGE_KERNEL|CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expectedEvent.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenCmdqAndItsVirtualEventThenDumpingWithProperLabels) {
    MockContext ctx;
    CommandQueue cmdq;
    VirtualEvent vEvent(&cmdq, &ctx);
    vEvent.setCurrentCmdQVirtualEvent(true);
    vEvent.updateTaskCount(1);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&vEvent] = 0;

    EventsTracker::dumpNode(&vEvent, stream, map);

    std::stringstream expected;
    expected << "e0[label=\"{---------V_EVENT  ptr=" << &vEvent << "------|CMD_UNKNOWN:" << (cl_command_type)-1
             << "|CL_QUEUED|task count=1, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n"
             << EventsTracker::label(&cmdq) << "->e0[label=\"VIRTUAL_EVENT\"];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenEventWithCallbackThenDumpingWithProperLabel) {
    Event::Callback::ClbFuncT func = [](cl_event ev, cl_int i, void *data) {};
    UserEvent uEvent;
    uEvent.addCallback(func, 0, nullptr);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;

    EventsTracker::dumpNode(&uEvent, stream, map);

    std::stringstream expected;
    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=TRUE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenSubmittedEventThenDumpingWithProperLabel) {
    Event event(nullptr, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&event] = 0;
    std::stringstream expected;

    event.setStatus(CL_SUBMITTED);
    EventsTracker::dumpNode(&event, stream, map);

    expected << "e0[label=\"{-----------EVENT  ptr=" << &event
             << "------|CL_COMMAND_NDRANGE_KERNEL|CL_SUBMITTED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=yellow];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenSubmittedUserEventThenDumpingWithProperLabel) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::stringstream expected;

    uEvent.setStatus(CL_SUBMITTED);
    EventsTracker::dumpNode(&uEvent, stream, map);

    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent
             << "------||CL_SUBMITTED|task count=NOT_READY, level=0|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenUserEventWithUnproperStatusThenDumpingWithProperLabel) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::stringstream expected;

    uEvent.setStatus(-1);
    EventsTracker::dumpNode(&uEvent, stream, map);

    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent
             << "------||ABORTED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=green];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenRunningEventThenDumpingWithProperLabel) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::stringstream expected;

    uEvent.setStatus(CL_RUNNING);
    EventsTracker::dumpNode(&uEvent, stream, map);

    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_RUNNING|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}
TEST(EventsTracker, givenQueuedEventThenDumpingWithProperLabel) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::stringstream expected;

    uEvent.setStatus(CL_QUEUED);
    EventsTracker::dumpNode(&uEvent, stream, map);

    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}
TEST(EventsTracker, givenCompleteEventThenDumpingWithProperLabel) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::stringstream expected;

    uEvent.setStatus(CL_COMPLETE);
    EventsTracker::dumpNode(&uEvent, stream, map);
    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent << "------||CL_COMPLETE|task count=NOT_READY, level=0|CALLBACKS=FALSE}\",color=green];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenNullptrEventThenNotDumpingGraph) {
    Event *ev = nullptr;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[ev] = 0;
    std::set<CommandQueue *> dumpedCmdQs;
    std::set<Event *> dumpedEvents;

    EventsTracker::dumpGraph(ev, stream, dumpedCmdQs, dumpedEvents, map);

    EXPECT_STREQ("", stream.str().c_str());
}

TEST(EventsTracker, givenAlreadyDumpedEventThenNotDumpingGraph) {
    UserEvent uEvent;

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    std::set<CommandQueue *> dumpedCmdQs;
    std::set<Event *> dumpedEvents;

    dumpedEvents.insert(&uEvent);
    EventsTracker::dumpGraph(&uEvent, stream, dumpedCmdQs, dumpedEvents, map);

    EXPECT_STREQ("", stream.str().c_str());
}

TEST(EventsTracker, givenCmdqAndItsVirtualEventThenDumpingProperGraph) {
    MockContext ctx;
    CommandQueue cmdq;
    VirtualEvent vEvent(&cmdq, &ctx);
    vEvent.setCurrentCmdQVirtualEvent(true);
    vEvent.updateTaskCount(1);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&vEvent] = 0;
    std::set<CommandQueue *> dumpedCmdQs;
    std::set<Event *> dumpedEvents;

    EventsTracker::dumpGraph(&vEvent, stream, dumpedCmdQs, dumpedEvents, map);
    std::stringstream expected;
    expected << EventsTracker::label(&cmdq) << "[label=\"{------CmdQueue, ptr=" << &cmdq << "------|task count=0, level=0}\",color=blue];\ne0[label=\"{---------V_EVENT  ptr=" << &vEvent
             << "------|CMD_UNKNOWN:4294967295|CL_QUEUED|task count=1, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n"
             << EventsTracker::label(&cmdq) << "->e0[label=\"VIRTUAL_EVENT\"];\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());
}

TEST(EventsTracker, givenTwoEventsWithCommonParentEventThenDumpingProperGraph) {
    UserEvent uEvent, uEventChild1, uEventChild2;
    uEvent.addChild(uEventChild1);
    uEvent.addChild(uEventChild2);

    std::stringstream stream;
    std::unordered_map<Event *, int64_t> map;
    map[&uEvent] = 0;
    map[&uEventChild1] = 1;
    map[&uEventChild2] = 2;
    std::set<CommandQueue *> dumpedCmdQs;
    std::set<Event *> dumpedEvents;

    EventsTracker::dumpGraph(&uEvent, stream, dumpedCmdQs, dumpedEvents, map);
    std::stringstream expected;
    expected << "e0[label=\"{------USER_EVENT ptr=" << &uEvent
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne2[label=\"{------USER_EVENT ptr=" << &uEventChild2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e2;\ne1[label=\"{------USER_EVENT ptr=" << &uEventChild1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e1;\n";

    EXPECT_STREQ(expected.str().c_str(), stream.str().c_str());

    uEvent.setStatus(0);
}

TEST(EventsTracker, whenCalingCreateDumpStreamThenGettingValidFstreamInstance) {
    std::string testFileName("test_files\\EventsTracker_testfile.gv");
    std::shared_ptr<std::ostream> stream = EventsTracker::getEventsTracker().createDumpStream(testFileName);

    EXPECT_TRUE(stream->good());

    static_cast<std::fstream *>(stream.get())->close();
    remove(testFileName.c_str());
    EventsTracker::shutdownGlobalEvTracker();
}

class EventsTrackerMock : public EventsTracker {
  public:
    std::shared_ptr<std::ostream> createDumpStream(const std::string &filename) override {
        streamMock.reset();
        std::shared_ptr<std::stringstream> out{new std::stringstream()};
        streamMock = out;
        return out;
    }
    void overrideGlobal() {
        originGlobal.swap(EventsTracker::globalEvTracker);
        EventsTracker::globalEvTracker = std::unique_ptr<EventsTracker>{new EventsTrackerMock()};
    }
    void restoreGlobal() {
        EventsTracker::shutdownGlobalEvTracker();
        EventsTracker::globalEvTracker.swap(originGlobal);
    }
    std::shared_ptr<std::stringstream> streamMock;
    std::unique_ptr<EventsTracker> originGlobal;
};

TEST(EventsTracker, whenDeletingEventTwoTimesThenDeletingIsProper) {
    UserEvent uEvent1;
    EventsTrackerMock evTrackerMock;

    std::stringstream expected;

    evTrackerMock.getList()->pushFrontOne(*new TrackedEvent{&uEvent1, 1});
    evTrackerMock.getList()->pushFrontOne(*new TrackedEvent{&uEvent1, -2});
    evTrackerMock.getList()->pushFrontOne(*new TrackedEvent{&uEvent1, -3});
    evTrackerMock.dump();

    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());
}

TEST(EventsTracker, givenTwoEventsWithSamePtrWhenFirstOneIsDeletedThenDumpingFirstProperly) {
    UserEvent uEvent;
    EventsTrackerMock evTrackerMock;

    std::stringstream expected;

    evTrackerMock.getList()->pushFrontOne(*new TrackedEvent{&uEvent, 2});
    evTrackerMock.getList()->pushFrontOne(*new TrackedEvent{&uEvent, -1});
    evTrackerMock.dump();

    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne2[label=\"{------USER_EVENT ptr="
             << &uEvent << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());
}

TEST(EventsTracker, whenNotifyCreationOfEventThenEventIsDumped) {
    Event event(nullptr, CL_COMMAND_USER, Event::eventNotReady, Event::eventNotReady);
    EventsTrackerMock evTrackerMock;

    std::stringstream expected;

    evTrackerMock.notifyCreation(&event);

    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());
}

TEST(EventsTracker, whenNotifyTransitionedExecutionStatusOfEventThenEventIsDumpedWithProperDescription) {
    UserEvent uEvent;
    EventsTrackerMock evTrackerMock;

    evTrackerMock.notifyCreation(&uEvent);
    evTrackerMock.notifyTransitionedExecutionStatus();

    std::stringstream expected;
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne0[label=\"{------USER_EVENT ptr=" << &uEvent
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());
}

TEST(EventsTracker, whenNotifyDestructionOfEventThenEventIsDumped) {
    UserEvent *uEvent = new UserEvent();
    EventsTrackerMock evTrackerMock;

    evTrackerMock.notifyCreation(uEvent);
    evTrackerMock.notifyDestruction(uEvent);
    delete uEvent;

    std::stringstream stream;
    stream << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(stream.str().c_str(), evTrackerMock.streamMock->str().c_str());
}

TEST(EventsTracker, givenSeveralEventsWhenOneIsCompleteThenDumpingWithProperLabels) {
    UserEvent *uEvent1 = new UserEvent();
    UserEvent *uEvent2 = new UserEvent();
    UserEvent *uEvent3 = new UserEvent();
    EventsTrackerMock evTrackerMock;

    evTrackerMock.notifyCreation(uEvent1);
    evTrackerMock.notifyCreation(uEvent2);
    evTrackerMock.notifyCreation(uEvent3);
    uEvent2->setStatus(CL_COMPLETE);
    evTrackerMock.notifyTransitionedExecutionStatus();
    evTrackerMock.notifyDestruction(uEvent2);
    delete uEvent2;

    std::stringstream stream;
    stream << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne2[label=\"{------USER_EVENT ptr=" << uEvent3
           << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0[label=\"{------USER_EVENT ptr=" << uEvent1
           << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(stream.str().c_str(), evTrackerMock.streamMock->str().c_str());
    delete uEvent1;
    delete uEvent3;
}

TEST(EventsTracker, givenEventsWithDependenciesBetweenThemThenDumpingProperGraph) {
    EventsTrackerMock evTrackerMock;

    UserEvent uEvent1;
    evTrackerMock.notifyCreation(&uEvent1);
    evTrackerMock.dump();

    std::stringstream expected;
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne0[label=\"{------USER_EVENT ptr=" << &uEvent1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());

    UserEvent uEvent2;
    evTrackerMock.notifyCreation(&uEvent2);
    evTrackerMock.dump();

    expected.str(std::string());
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne1[label=\"{------USER_EVENT ptr=" << &uEvent2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0[label=\"{------USER_EVENT ptr=" << &uEvent1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());

    UserEvent uEventChild1;
    evTrackerMock.notifyCreation(&uEventChild1);
    uEvent1.addChild(uEventChild1);
    evTrackerMock.dump();

    expected.str(std::string());
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne1[label=\"{------USER_EVENT ptr=" << &uEvent2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0[label=\"{------USER_EVENT ptr=" << &uEvent1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne2[label=\"{------USER_EVENT ptr=" << &uEventChild1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e2;\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());

    UserEvent uEventChild2;
    evTrackerMock.notifyCreation(&uEventChild2);
    uEvent1.addChild(uEventChild2);
    evTrackerMock.dump();

    expected.str(std::string());
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne1[label=\"{------USER_EVENT ptr=" << &uEvent2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0[label=\"{------USER_EVENT ptr=" << &uEvent1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne3[label=\"{------USER_EVENT ptr=" << &uEventChild2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e3;\ne2[label=\"{------USER_EVENT ptr=" << &uEventChild1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e2;\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());

    uEvent2.addChild(uEvent1);
    evTrackerMock.dump();

    expected.str(std::string());
    expected << "digraph events_registry_" << &evTrackerMock << " {\nnode [shape=record]\n//pragma: somePragmaData\ne1[label=\"{------USER_EVENT ptr=" << &uEvent2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0[label=\"{------USER_EVENT ptr=" << &uEvent1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne3[label=\"{------USER_EVENT ptr=" << &uEventChild2
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e3;\ne2[label=\"{------USER_EVENT ptr=" << &uEventChild1
             << "------||CL_QUEUED|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\ne0->e2;\ne1->e0;\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMock.streamMock->str().c_str());

    uEvent2.setStatus(0);
    uEvent1.setStatus(0);
}

TEST(EventsTracker, whenEventsDebugEnableFlagIsTrueAndCreateOrChangeStatusOrDestroyEventThenDumpingGraph) {
    DebugManagerStateRestore dbRestore;
    DebugManager.flags.EventsTrackerEnable.set(true);

    EventsTrackerMock evTrackerMock;
    evTrackerMock.overrideGlobal();

    Event *ev = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, Event::eventNotReady);

    std::stringstream expected;
    expected << "digraph events_registry_" << &EventsTracker::getEventsTracker() << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), static_cast<EventsTrackerMock *>(&EventsTracker::getEventsTracker())->streamMock->str().c_str());

    ev->setStatus(1);

    expected.str(std::string());
    expected << "digraph events_registry_" << &EventsTracker::getEventsTracker() << " {\nnode [shape=record]\n//pragma: somePragmaData\ne0[label=\"{-----------EVENT  ptr=" << ev
             << "------|CL_COMMAND_NDRANGE_KERNEL|CL_RUNNING|task count=NOT_READY, level=NOT_READY|CALLBACKS=FALSE}\",color=red];\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), static_cast<EventsTrackerMock *>(&EventsTracker::getEventsTracker())->streamMock->str().c_str());

    delete ev;

    expected.str(std::string());
    expected << "digraph events_registry_" << &EventsTracker::getEventsTracker() << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), static_cast<EventsTrackerMock *>(&EventsTracker::getEventsTracker())->streamMock->str().c_str());

    evTrackerMock.restoreGlobal();
}

class EventsTrackerMockMT : public EventsTrackerMock {
  public:
    TrackedEvent *getNodes() override {
        auto TrackedEventsMock = std::shared_ptr<IFList<TrackedEvent, true, true>>{new IFList<TrackedEvent, true, true>};
        return TrackedEventsMock->detachNodes();
    }
    std::shared_ptr<IFList<TrackedEvent, true, true>> *TrackedEventsMock;
};

TEST(EventsTracker, givenEventsFromDifferentThreadsThenDumpingProperly) {
    auto evTrackerMockMT = std::shared_ptr<EventsTrackerMockMT>{new EventsTrackerMockMT()};
    UserEvent uEvent1;
    UserEvent uEvent2;

    evTrackerMockMT->getList()->pushFrontOne(*new TrackedEvent{&uEvent1, 2});
    evTrackerMockMT->getList()->pushFrontOne(*new TrackedEvent{&uEvent2, 3});
    evTrackerMockMT->dump();

    std::stringstream expected;
    expected << "digraph events_registry_" << evTrackerMockMT
             << " {\nnode [shape=record]\n//pragma: somePragmaData\n\n}\n";

    EXPECT_STREQ(expected.str().c_str(), evTrackerMockMT->streamMock->str().c_str());
}
