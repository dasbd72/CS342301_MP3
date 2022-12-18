// scheduler.h
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

/* clang-format off */
#include "copyright.h"
#include "list.h"
#include "thread.h"
/* clang-format on */

// The following class defines the scheduler/dispatcher abstraction --
// the data structures and operations needed to keep track of which
// thread is running, and which threads are ready but not running.

class Scheduler {
   public:
    Scheduler();   // Initialize list of ready threads
    ~Scheduler();  // De-allocate ready list

    void ReadyToRun(Thread* thread);
    // Thread can be dispatched.
    Thread* FindNextToRun();  // Dequeue first thread on the ready
                              // list, if any, and return thread.
    void Run(Thread* nextThread, bool finishing);
    // Cause nextThread to start running
    void CheckToBeDestroyed();  // Check if thread that had been
                                // running needs to be deleted
    void Print();               // Print contents of ready list
    // MP3
    void Aging();
    bool Preempt();
    // MP3 end

    // SelfTest for scheduler is implemented in class Thread

   private:
    // MP3
    SortedList<Thread*>* readyList1;  // L1
    SortedList<Thread*>* readyList2;  // L2
    List<Thread*>* readyList3;        // L3
    Thread* preemtingThread;
    // MP3 end
    Thread* toBeDestroyed;  // finishing thread to be destroyed
                            // by the next thread that runs
};

#endif  // SCHEDULER_H
