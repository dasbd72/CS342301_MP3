// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

/* clang-format off */
#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"
/* clang-format on */

// MP3
//----------------------------------------------------------------------
// compSJF
//  Comparator of preemptive SJF (remaining burst time) scheduling policy
//----------------------------------------------------------------------

static int compSJF(Thread *x, Thread *y) {
    // Compare using remaining burst time since running thread may become ready
    if (x->getRemainBurstTick() > y->getRemainBurstTick()) {
        return 1;
    } else if (x->getRemainBurstTick() < y->getRemainBurstTick()) {
        return -1;
    } else {
        return 0;
    }
}

//----------------------------------------------------------------------
// compPriority
//  Comparator of non-preemptive priority scheduling
//----------------------------------------------------------------------

static int compPriority(Thread *x, Thread *y) {
    if (x->getPriority() < y->getPriority()) {
        return 1;
    } else if (x->getPriority() > y->getPriority()) {
        return -1;
    } else {
        return 0;
    }
}
// MP3 end

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
    // MP3
    readyList1 = new SortedList<Thread *>(compSJF);
    readyList2 = new SortedList<Thread *>(compPriority);
    readyList3 = new List<Thread *>();
    preemtingThread = NULL;
    // MP3 end
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
    // MP3
    delete readyList1;
    delete readyList2;
    delete readyList3;
    // MP3 end
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    // MP3
    ASSERT(thread->getPriority() >= 0 && thread->getPriority() < 150);
    thread->enterReady();
    if (thread->getPriority() >= 100) {
        // L1: [100, 149]
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[1]");
        readyList1->Insert(thread);
    } else if (thread->getPriority() >= 50) {
        // L2: [50, 99]
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[2]");
        readyList2->Insert(thread);
    } else {
        // L3: [0, 49]
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[3]");
        readyList3->Append(thread);
    }
    // MP3 end
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun() {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    // MP3
    Thread *currentThread = kernel->currentThread;
    Thread *nextThread = NULL;
    if (preemtingThread != NULL) {
        nextThread = preemtingThread;
        preemtingThread = NULL;
    } else if (currentThread->getStatus() != RUNNING) {
        if (!readyList1->IsEmpty()) {
            nextThread = readyList1->RemoveFront();
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is removed from queue L[1]");
        } else if (!readyList2->IsEmpty()) {
            nextThread = readyList2->RemoveFront();
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is removed from queue L[2]");
        } else if (!readyList3->IsEmpty()) {
            nextThread = readyList3->RemoveFront();
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is removed from queue L[3]");
        }
    }
    return nextThread;
    // MP3 end
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing) {
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {  // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) {  // if this thread is a user program,
        oldThread->SaveUserState();  // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();  // check if the old thread
                                 // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    // MP3
    nextThread->ReadyToRunning();
    DEBUG(dbgSchedule, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
                                    << "] is now selected for execution, thread [" << oldThread->getID()
                                    << "] is replaced, and it has executed [" << oldThread->getBurstTick() << "] ticks");
    // MP3
    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();  // check if thread we were running
                           // before this one has finished
                           // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();  // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed() {
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
    // MP3
    cout << "Ready list 1 contents:\n";
    readyList1->Apply(ThreadPrint);
    cout << "Ready list 2 contents:\n";
    readyList2->Apply(ThreadPrint);
    cout << "Ready list 3 contents:\n";
    readyList3->Apply(ThreadPrint);
    // MP3 end
}

// MP3
//----------------------------------------------------------------------
// Scheduler::Aging
// 	Check if needs to age, and move from list to list
//----------------------------------------------------------------------
void Scheduler::Aging() {
    readyList1->Apply(ThreadAge);
    readyList2->Apply(ThreadAge);
    readyList3->Apply(ThreadAge);
    while (!readyList2->IsEmpty() && readyList2->Front()->getPriority() >= 100) {
        // L2 to L1
        DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << readyList2->Front()->getID() << "] is removed from queue L[2]");
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << readyList2->Front()->getID() << "] is inserted into queue L[1]");
        readyList1->Insert(readyList2->RemoveFront());
    }
    List<Thread *> *tmpList3 = readyList3;
    readyList3 = new List<Thread *>();
    while (!tmpList3->IsEmpty()) {
        if (tmpList3->Front()->getPriority() >= 50) {
            // L3 to L2
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << tmpList3->Front()->getID() << "] is removed from queue L[3]");
            DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << tmpList3->Front()->getID() << "] is inserted into queue L[2]");
            readyList2->Insert(tmpList3->RemoveFront());
        } else {
            readyList3->Append(tmpList3->RemoveFront());
        }
    }
    delete tmpList3;
}

bool Scheduler::Preempt() {
    Thread *currentThread = kernel->currentThread;
    if (currentThread->getStatus() == RUNNING) {
        currentThread->updateBurstTick();
        if (currentThread->getPriority() >= 100) {
            // L1
            if (!readyList1->IsEmpty() && currentThread->getRemainBurstTick() > readyList1->Front()->getRemainBurstTick()) {
                // L1 preempts L1 if current L1 thread's remaining burst time is larger.
                preemtingThread = readyList1->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << preemtingThread->getID() << "] is removed from queue L[1]");
            }
        } else if (currentThread->getPriority() >= 50) {
            // L2
            if (!readyList1->IsEmpty()) {
                // only L1 preempts L2
                preemtingThread = readyList1->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << preemtingThread->getID() << "] is removed from queue L[1]");
            }
        } else {
            // L3
            if (!readyList1->IsEmpty()) {
                // L1 preempts L3
                preemtingThread = readyList1->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << preemtingThread->getID() << "] is removed from queue L[1]");
            } else if (!readyList2->IsEmpty()) {
                // L2 preempts L3
                preemtingThread = readyList2->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << preemtingThread->getID() << "] is removed from queue L[2]");
            } else if (currentThread->getTimeQuantum() >= 100 && !readyList3->IsEmpty()) {
                // L3 round-robin
                preemtingThread = readyList3->RemoveFront();
                DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << preemtingThread->getID() << "] is removed from queue L[3]");
            }
        }
    }
    if (preemtingThread == NULL)
        return false;
    else
        return true;
}
// MP3 end
