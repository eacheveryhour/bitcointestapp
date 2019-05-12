#ifndef IRUNNABLE_H
#define IRUNNABLE_H

#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <thread>
#include <chrono>

enum ThreadPriority
{
    TPri_Normal,
    TPri_AboveNormal,
    TPri_BelowNormal,
    TPri_Highest,
    TPri_Lowest,
    TPri_SlightlyBelowNormal,
    TPri_TimeCritical
};

typedef void *(*PthreadEntryPoint)(void *arg);

class IRunnable
{
public:

    inline bool Start()
    {
        if (IsStarted)
        {
            return true;
        }

        if (CreateThread(&Thread, &IsStarted, &IRunnable::_ThreadProc))
        {
            return IsStarted;
        }

        return false;

    }

    inline bool Stop()
    {
        if (!IsStarted)
        {
            printf("Thread already stopped");
            return false;
        }

        printf("Cancelling thread...");
        IsStarted = false;

        if (pthread_cancel(Thread) == 0)
        {
            printf("Thread succesfully canceled.");
            return true;
        }

    return false;

    }

    inline void WaitForCompletion()
    {
        // Block until this thread exits
        pthread_join(Thread, nullptr);
    }

protected:

    /**
     * Allows subclass to setup anything needed on the thread before running the Run function
     */
    virtual void PreRun() {}

    virtual void Run() = 0;

    /**
     * Allows a subclass to teardown anything needed on the thread after running the Run function
     */
    virtual void PostRun() {}

    inline void SetThreadPriority(pthread_t InThread, ThreadPriority NewPriority)
    {
        struct sched_param Sched;
        memset(&Sched, 0, sizeof(struct sched_param));
        int Policy = SCHED_RR;

        // Read the current policy
        pthread_getschedparam(InThread, &Policy, &Sched);

        // set the priority appropriately
        Sched.sched_priority = TranslatThreadPriority(NewPriority);
        pthread_setschedparam(InThread, Policy, &Sched);
    }

    virtual ~IRunnable()
    {
        Stop();
    }

    virtual PthreadEntryPoint GetThreadEntryPoint()
    {
        return _ThreadProc;
    }

private:

    /**
     * Converts an ThreadPriority to a value that can be used in pthread_setschedparam.
     */
    virtual int TranslatThreadPriority(ThreadPriority Priority)
    {
        // these are some default priorities
        switch (Priority)
        {
            // 0 is the lowest, 31 is the highest possible priority for pthread
            case TPri_Highest: case TPri_TimeCritical: return 30;
            case TPri_AboveNormal: return 25;
            case TPri_Normal: return 15;
            case TPri_BelowNormal: return 5;
            case TPri_Lowest: return 1;
            case TPri_SlightlyBelowNormal: return 14;
        }
    }

    /**
     * The thread entry point. Simply forwards the call on to the right
     * thread main function
     */
    static void * _ThreadProc(void *pThis)
    {
        assert(pThis);

        IRunnable* ThisThread = reinterpret_cast<IRunnable*>(pThis);

        // run the thread!
        ThisThread->PreRun();
        ThisThread->Run();
        ThisThread->PostRun();

        pthread_exit(nullptr);
    }

    bool CreateThread(pthread_t* HandlePtr, bool* OutThreadCreated, PthreadEntryPoint Proc, uint32_t InStackSize = 2097152 /* Default Linux/x86-32 stack size 2mb*/, void *Arg = nullptr)
    {
        *OutThreadCreated = false;
        pthread_attr_t *AttrPtr = nullptr;
        pthread_attr_t StackAttr;

        if (InStackSize != 0)
        {
            if (pthread_attr_init(&StackAttr) == 0)
            {
                // we'll use this the attribute if this succeeds, otherwise, we'll wing it without it.
                const size_t StackSize = (size_t) InStackSize;
                if (pthread_attr_setstacksize(&StackAttr, StackSize) == 0)
                {
                    AttrPtr = &StackAttr;
                }
            }

            if (AttrPtr == nullptr)
            {
                printf("Failed to change pthread stack size to %d bytes", InStackSize);
            }
        }

        IRunnable *This = this;
        const int ThreadErrno = pthread_create(HandlePtr, AttrPtr, Proc, This);
        *OutThreadCreated = (ThreadErrno == 0);

        if (AttrPtr != nullptr)
        {
            pthread_attr_destroy(AttrPtr);
        }

        if (!*OutThreadCreated)
        {
            printf("Failed to create thread! (err=%d, %s)", ThreadErrno, (strerror(ThreadErrno)));
        }

        return *OutThreadCreated;
    }

    /**
     * The thread handle for the thread
     */
    pthread_t Thread;

    bool IsStarted = false;
};

#endif // IRUNNABLE_H
