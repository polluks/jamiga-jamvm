
#include "pthread.h"
#include "signal.h"
#include "../../excep.h"

struct Process * signalHandlerPtr = NULL;
extern struct Task * mainAmigaTask;
extern void realAmigaExit(int error);
struct PthreadInitData * initPthreadInitData(void * args);

#define DONTDEBUGWAIT

/**
 * pthread_kill
 * The pthread_kill() function sends the signal sig to thread, a thread
 * in the same process as the caller.  The signal is asynchronously
 * directed to thread.
 *
 * If sig is 0, then no signal is sent, but error checking is still
 * performed.
 *
 * POSIX.1-2008 recommends that if an implementation detects the use of
 * a thread ID after the end of its lifetime, pthread_kill() should
 * return the error ESRCH.  The glibc implementation returns this error
 * in the cases where an invalid thread ID can be detected.  But note
 * also that POSIX says that an attempt to use a thread ID whose
 * lifetime has ended produces undefined behavior, and an attempt to use
 * an invalid thread ID in a call to pthread_kill() can, for example,
 * cause a segmentation fault.
 *
 */
int pthread_kill(void * thread, int sig) {
    JA_TRACE("pthread_kill. Signalling thread %lx(%ld) with sig %ld, state: %ld\n", thread, ((struct Process *)thread)->pr_ProcessID, sig, ((struct Task *)thread)->tc_State);
    Signal(thread, 1 << sig);
    return 0;//ESRCH;
}

/**
 * sched_yield()
 * causes the calling thread to relinquish the CPU.  The
 * thread is moved to the end of the queue for its static priority and a
 * new thread gets to run.
 *
 * @return On success, sched_yield() returns 0.  On error, -1 is returned, and
 * errno is set appropriately.
 * In the Linux implementation, sched_yield() always succeeds.
 */
void sched_yield() {
    BYTE oldPrio = SetTaskPri(FindTask(NULL), -10);
    JA_TRACE("Sched_yield: oldprio, with delay: %d\n", oldPrio);
    Delay(10);
    SetTaskPri(FindTask(NULL), oldPrio);
}

/**
 * pthread_mutex_init
 */
void pthread_mutex_init(pthread_mutex_t *lock, void *used) {
    InitSemaphore(lock);
}

/**
 * pthread_getspecific
 */
void* pthread_getspecific(void *self) {
    struct PthreadInitData * initData = (struct PthreadInitData *)GetExitData();
    if(initData == NULL) {
        JA_TRACE("initdata was null!\n");
        return NULL;
    }
    return initData->pd_Specific;
}

/**
 * pthread_setspecific
 */
void pthread_setspecific(void *self, void *value) {
    struct PthreadInitData * initData = (struct PthreadInitData *)GetExitData();
    if(initData == NULL) {
        JA_TRACE("initdata was null!\n");
        return;
    }
    JA_TRACE("set spec %lx\n", value);
    initData->pd_Specific = value;
}

/**
 * ptherad_mutex_lock
 */
int pthread_mutex_lock(struct SignalSemaphore *lock) {
    ObtainSemaphore(lock);
    return 0;
}


/**
 * pthread_sigmask
 * pthread_sigmask, sigprocmask - examine and change blocked signals
 *
 * The pthread_sigmask() function shall examine or change (or both) the
 * calling thread's signal mask, regardless of the number of threads in
 * the process. The function shall be equivalent to sigprocmask(), without
 * the restriction that the call be made in a single-threaded process.
 *
 * In a single-threaded process, the sigprocmask() function shall examine
 * or change (or both) the signal mask of the calling thread.
 *
 * If the argument set is not a null pointer, it points to a set of signals
 * to be used to change the currently blocked set.
 *
 * The argument how indicates the way in which the set is changed, and the
 * application shall ensure it consists of one of the following values:
 *
 * SIG_BLOCK
 *     The resulting set shall be the union of the current set and the signal
 *     set pointed to by set.
 * SIG_SETMASK
 *     The resulting set shall be the signal set pointed to by set.
 * SIG_UNBLOCK
 *     The resulting set shall be the intersection of the current set and the
 *     complement of the signal set pointed to by set.
 *
 * If the argument oset is not a null pointer, the previous mask shall be
 * stored in the location pointed to by oset. If set is a null pointer, the
 * value of the argument how is not significant and the thread's signal mask
 * shall be unchanged; thus the call can be used to enquire about currently
 * blocked signals.
 *
 * If there are any pending unblocked signals after the call to sigprocmask(),
 * at least one of those signals shall be delivered before the call to
 * sigprocmask() returns.
 *
 * It is not possible to block those signals which cannot be ignored. This
 * shall be enforced by the system without causing an error to be indicated.
 *
 * If any of the SIGFPE, SIGILL, SIGSEGV, or SIGBUS signals are generated
 * while they are blocked, the result is undefined, unless the signal was
 * generated by the kill() function, the sigqueue() function, or the raise()
 * function.
 *
 * If sigprocmask() fails, the thread's signal mask shall not be changed.
 *
 * The use of the sigprocmask() function is unspecified in a multi-threaded
 * process.
 */
void pthread_sigmask(int blocktype, uint32 * mask_ptr, void *unused1) {
    uint32 old_signals, new_signals = 0, current_signals;
    //JA_TRACE("Called pthread_sigmask %s mask:%lx sigkill: %lx\n", (blocktype == SIG_UNBLOCK?"UNBLOCK (catch signals) ":"BLOCK (ignore signal)"), (*mask_ptr), (1<<SIGKILL));
    //struct PthreadInitData * initData = GetExitData();
    old_signals = SetExcept(0,0);

    switch(blocktype) {
        case SIG_BLOCK:
            JA_TRACE("SIG_BLOCK\n");
            new_signals = (*mask_ptr) & old_signals;//0
            
            break;
        case SIG_UNBLOCK:
            JA_TRACE("SIG_UNBLOCK\n");
            new_signals = old_signals | (~(*mask_ptr));//(*mask_ptr);
            break;
        
        case SIG_SETMASK:
            JA_TRACE("SIG_SETMASK\n");
            new_signals = (*mask_ptr);
            // unused by jamvm
            break;

    }
    sigaddset(&new_signals, SIGKILL);
    sigaddset(&new_signals, SIGINT);
    current_signals = SetExcept(0,0);
    JA_TRACE("CurSignals: "BYTETOBINARYPATTERN, BYTETOBINARY((*mask_ptr)));
    JA_TRACE("CurSignals: "BYTETOBINARYPATTERN, BYTETOBINARY(current_signals));
    JA_TRACE("OldSignals: "BYTETOBINARYPATTERN, BYTETOBINARY(old_signals));
    JA_TRACE("NewSignals: "BYTETOBINARYPATTERN, BYTETOBINARY(new_signals));
    JA_TRACE("SIGKILL is %s, SIGINT is %s\n", sigismember(&new_signals, SIGKILL)?"set":"not set",  sigismember(&new_signals, SIGINT)?"set":"not set");
    //initData->pd_SigMask = new_signals;
    /*
    initData->pd_SigMask = blocktype == SIG_UNBLOCK ? *mask_ptr : 0;
    SetExcept(blocktype == SIG_UNBLOCK ? *mask_ptr : ((1<<SIGINT)&(1<<SIGKILL)),blocktype == SIG_UNBLOCK ? *mask_ptr : ((1<<SIGINT)&(1<<SIGKILL)));// *mask_ptr);
    */
    SetExcept(current_signals,new_signals);
}

/**
 * pthread_mutex_unlock
 */
int pthread_mutex_unlock(struct SignalSemaphore * lock) {
    ReleaseSemaphore(lock);
    return 0;
}

/**
 * pthread_mutex_trylock
 */
int pthread_mutex_trylock(struct SignalSemaphore * lock) {
    if(AttemptSemaphore(lock)) {
        return 0;
    } else {
        return -1;
    }
}


/**
 * SignalThreads
 * Signal all threads with SIGUSR2 waiting on the condvaraible.
 */
int SignalThreads(enum CondType type, struct CondVariable * cv) {
    struct ProcessNode * procNode = NULL;

    ObtainSemaphore(&((struct CondVariable * )cv)->semaphore);
    ((struct CondVariable * )cv)->condType = type;

    for (procNode = GetHead(&((struct CondVariable * )cv)->waitingProcessList);
         procNode != NULL;
         procNode = GetSucc(procNode))
    {
        JA_TRACE("Signal w SIGUSR2 %lx %ld\n", procNode->process,procNode->process->pr_ProcessID);
        if(procNode->process->pr_ProcessID != 0) {
            Signal(procNode->process, 1 << SIGUSR2);
        } /*else {
            JA_TRACE("Nope, skipped %lx %ld",procNode->process,procNode->process->pr_ProcessID);
        }   */
    }

    /* now all tasks have been signalled */
    ReleaseSemaphore(&((struct CondVariable * )cv)->semaphore);
    return 0;
}


/**
 * pthread_cond_init
 */
void pthread_cond_init(struct CondVariable *cv, void * unused) {
    ((struct CondVariable*)cv)->condType = NONE;
    InitSemaphore((struct SignalSemaphore *)&((struct CondVariable*)cv)->semaphore);
    NewMinList(&((struct CondVariable*)cv)->waitingProcessList);
}

/**
 * pthread_cond_signal
 */
int pthread_cond_signal(struct CondVariable * cv) {
    return SignalThreads(SIGNAL, cv);
}


/**
 * pthread_cond_broadcast
 */
int pthread_cond_broadcast(struct CondVariable * cv) {
    return SignalThreads(BROADCAST, cv);
}


/**
 * CondWait
 */
int CondWait(struct CondVariable *cv, struct SignalSemaphore * lock, struct timespec * ts) {
    uint32 sigSet = 0L;
    uint32 waitSigSet = 0L;

    uint32 condType;
    struct ProcessNode procNode;
    struct TimeRequest * timerReq = NULL;
    struct MsgPort *timerMP = NULL;
    struct Process * me = (struct Process *)FindTask(NULL);
    struct PthreadInitData * initData = GetExitData();
    if(initData == NULL) {
        me->pr_ExitData = initPthreadInitData(NULL);
        initData = GetExitData();
    }
    int retValue, timerSigBit = 0;
    BYTE error, cauchtsigint = 0;
    procNode.process = me;
    JA_TRACE("CondWait: Obtaining semaphore 1\n");
    ObtainSemaphore(&cv->semaphore);

    // add this process to the waitinglist
    AddHead(&cv->waitingProcessList, &procNode);
    JA_TRACE("CondWait: added to process list\n");
    JA_TRACE("CondWait: release semaphore 1\n");
    ReleaseSemaphore(&cv->semaphore);
    JA_TRACE("CondWait: release semaphore lock\n");
    ReleaseSemaphore(lock);

    while(1) {
        if(ts != NULL) {
            timerMP = AllocSysObjectTags(ASOT_PORT, TAG_END);
            if(timerMP == NULL) {
                JA_TRACE("Couldn't alloc memory for Timer msgPort\n");
                retValue = 0;
                goto cleanReturn;
            }
            timerReq = AllocSysObjectTags(ASOT_IOREQUEST,
                ASOIOR_Size, sizeof(struct TimeRequest),
                ASOIOR_ReplyPort, timerMP,//&((struct Process *)FindTask(NULL))->pr_MsgPort,
                TAG_END);

            if(timerReq == NULL) {
                JA_TRACE("Couldn't alloc memory for Timer IO req\n");
                retValue = 0;
                goto cleanReturn;
            }
            if(error = OpenDevice(TIMERNAME, UNIT_WAITUNTIL, timerReq, 0)) {
                JA_TRACE("FATAL Couldn't open device\n");
                retValue = 0;
                goto cleanReturn;
            }
            timerReq->Request.io_Command = TR_ADDREQUEST;
                                                      // seconds from 1970-1978 (unix vs amiga epoch) no longer used!
            #ifdef _SYS_CLIB2_STDC_H
            timerReq->Time.Seconds       = ts->tv_sec;// - 252460800;
            #else
            timerReq->Time.Seconds       = ts->tv_sec- 252460800;
            #endif
            timerReq->Time.Microseconds  = ts->tv_nsec/1000;

            #ifndef DONTDEBUGWAIT
            // Get now...
            struct TimeVal now;
            GetSysTime(&now);
            struct ClockData cd;
            struct ClockData cdNow;
            Amiga2Date(timerReq->Time.Seconds, &cd);
            JA_TRACE("Wait until: %2ld-%2ld-%2ld %2ld:%2ld.%2ld %3ld, before adding TZ offsets\n", cd.year, cd.month, cd.mday, cd.hour, cd.min, cd.sec, timerReq->Time.Microseconds)
            #endif
            
            int32 totalOffset = 0;
            int32 utcOffsetSTD;
            int32 utcOffsetDST;
            BYTE timeFlag;
            GetTimezoneAttrs(NULL,
                TZA_UTCOffsetSTD, &utcOffsetSTD,
                TZA_UTCOffsetDST, &utcOffsetDST,
                TZA_TimeFlag, &timeFlag,
                TAG_END);
            // TFLG_ISSTD:     Standard time is in effect for current location
            //TFLG_ISDST:     Daylight saving time is in effect for current
            //               location
            //TFLG_UNKNOWN
            if(timeFlag == TFLG_ISSTD) {
                //JA_TRACE("UTC offset STD: %ld\n", utcOffsetSTD);
                totalOffset += utcOffsetSTD;
            } else if(timeFlag == TFLG_ISDST) {
                //JA_TRACE("UTC offset DST: %ld\n", utcOffsetDST);
                totalOffset += utcOffsetDST;
            } else {
                //JA_TRACE("TFLG_UNKNOWN! Using offset\n");
            }

            //JA_TRACE("Offset: %ld\n", totalOffset);
            //totalOffset += utcOffset;
            //JA_TRACE("Toal offset: %ld\n", (totalOffset*60));
            //JA_TRACE("Before second: %ld\n", timerReq->Time.Seconds);
            timerReq->Time.Seconds -= (totalOffset*60);
            //JA_TRACE("Aafter second: %ld\n", timerReq->Time.Seconds);

            #ifndef DONTDEBUGWAIT
            Amiga2Date(timerReq->Time.Seconds, &cd);
            Amiga2Date(now.Seconds, &cdNow);
            JA_TRACE("Wait now:   %2ld-%2ld-%2ld %2ld:%2ld.%2ld %3ld\n", cdNow.year, cdNow.month, cdNow.mday, cdNow.hour, cdNow.min, cdNow.sec, now.Microseconds);
            JA_TRACE("Wait until: %2ld-%2ld-%2ld %2ld:%2ld.%2ld %3ld\n", cd.year, cd.month, cd.mday, cd.hour, cd.min, cd.sec, timerReq->Time.Microseconds);
            #endif
            // ...for debug

            
            SendIO(timerReq);
            timerSigBit = timerMP->mp_SigBit;
            //JA_TRACE("CondWait: %ld:%ld timesigbit:%ld\n", ts->tv_sec, ts->tv_nsec/1000, timerSigBit);
            waitSigSet = (1 << SIGUSR2) | (1 << SIGINT) | (1 << timerSigBit);
            JA_TRACE("<--Wating on signal %ld %ld %ld, ignoring SIGINT in exception, allowing other\n", SIGUSR2, SIGINT, timerSigBit);

        } else {
            JA_TRACE("CondWait: Wait sig set for SIGUSR2 | SIGUSR1\n");
            waitSigSet = (1 << SIGUSR2) | (1 << SIGINT);
        }
        

        // ignore sigint from exception, since we catch it below
        uint32 ignoreSigExcept = SetExcept(0,0);
        sigdelset(&ignoreSigExcept, SIGINT);
        uint32 oldSigs = SetExcept(0, 1<<SIGINT);
        sigSet = Wait(waitSigSet);
        SetExcept(oldSigs,oldSigs);
        //JA_TRACE("-->Watied on signal, allowing SIGINT in exception, allowing other\n");
        if((sigSet & (1<<timerSigBit)) == (1<<timerSigBit)) {
            JA_TRACE("CondWait: Got %ld  %s, Ctrl-D returningn ETIMEDOUT\n", sigSet, ((sigSet & (1<<SIGUSR1)) == (1<<SIGUSR1))?"true":"false");
            /* sigusr1 means we should interrupt, set signal*/
            /* again, so we can get it in caller*/
            ObtainSemaphore(lock);
            JA_TRACE("CondWait: retval %lx\n", retValue);
            retValue = ETIMEDOUT;
            //JA_TRACE("CondWait: retval %lx\n", retValue);
            Remove(&procNode);
            goto cleanReturn;
        }

        if((sigSet & (1<<SIGINT)) == (1<<SIGINT)) {
            JA_TRACE("CondWat: got sigint\n");
            ObtainSemaphore(lock);
            retValue = SIGINT;
            Remove(&procNode);
            goto cleanReturn;
        }
        
        ObtainSemaphoreShared(&cv->semaphore);
        condType = cv->condType;
        ReleaseSemaphore(&cv->semaphore); /* release shared semaphore */

        switch(condType) {
            case SIGNAL:
                if(AttemptSemaphore(&cv->semaphore)) {
                    /* make sure only we act on the notify    */
                    cv->condType = NONE;
                    Remove(&procNode);
                    ReleaseSemaphore(&cv->semaphore);
                    ObtainSemaphore(lock);
                    retValue = 1;
                    goto cleanReturn;

                } else {
                    continue;
                }

            case BROADCAST:
                /* a broadcast, other waiting threads should      */
                /* act on this same message                       */
                ObtainSemaphore(&cv->semaphore);
                Remove(&procNode);
                ReleaseSemaphore(&cv->semaphore);
                ObtainSemaphore(lock);
                retValue = 1;
                goto cleanReturn;
            case NONE:
                /* This was just a false call (i.e. someone else  */
                /* got the semaphore before we did)               */
                continue;
        }
    }
cleanReturn:
    JA_TRACE("CondWait: Retvalue %lx\n", retValue);
    if(timerReq != NULL) {
        if (!(CheckIO(timerReq))) {
            AbortIO(timerReq);      /* Ask device to abort any pending requests */
        }
        WaitIO(timerReq);          /* Clean up */
        if(error == NULL ) {
            CloseDevice(timerReq);
        }
        
    }
    JA_TRACE("CondWait: Alloced Freed sysobjects Retvalue before return %lx\n", retValue);
    FreeSysObject(ASOT_PORT, timerMP);
    FreeSysObject(ASOT_IOREQUEST, timerReq);
    if(retValue == SIGINT) {
        JA_TRACE("Signalling ourselves w SIGINT!\n");
        SetSignal(1<<SIGINT, 1<<SIGINT);
        JA_TRACE("After sigint, SHOULD NOT HAPPEN\n");
    }
    return retValue;
}


/**
 * pthread_cond_wait
 */
int pthread_cond_wait(struct CondVariable *cv, struct SignalSemaphore * lock) {
    int ret = CondWait(cv, ((struct SignalSemaphore *)(lock)), NULL);
    //JA_TRACE("pcw: Retvalue before return %lx\n", ret);

    return ret;
}


/**
 * pthread_cond_timedwait
 */
int pthread_cond_timedwait(struct CondVariable * cv, struct SignalSemaphore * lock, struct timespec * absoluteTime) {

    int ret = CondWait(cv, ((struct SignalSemaphore *)(lock)), absoluteTime);
    //JA_TRACE("pct: Retvalue before return %lx\n", ret);
    return ret;
}


/* From thread.h */
struct thread {
    int id;
    pthread_t tid;

    void/*ExecEnv*/ *ee;
    void *stack_top;
    void *stack_base;
    void/*Monitor*/ *wait_mon;
    void/*Monitor*/ *blocked_mon;
    void/*Thread*/ *wait_prev;
    void/*Thread*/ *wait_next;
    pthread_cond_t wait_cv;
    pthread_cond_t park_cv;
    pthread_mutex_t park_lock;
    long long blocked_count;
    long long waited_count;
    void/*Thread*/ *prev, *next;
    unsigned int wait_id;
    unsigned int notify_id;
    char suspend;
    char park_state;
    char interrupted;
    char interrupting;
    char suspend_state;
//    #ifdef __amigaos4__
//    struct Process *amiga_task;
//    #endif
    // (oups:)
    //CLASSLIB_THREAD_EXTRA_FIELDS
};
#define SUSP_BLOCKING 1

void printFileHandleInfoInfo(BPTR file) {
    struct ExamineData *dat = ExamineObjectTags(EX_FileHandleInput,file,TAG_END);
    if( dat != NULL)
    {
        if( EXD_IS_FILE(dat) )
        {
            //JA_ERROR("file: %lx, filename=%s\n", file, dat->Name);
        }
        else if ( EXD_IS_DIRECTORY(dat) )
        {
           JA_TRACE("file: %lx, dirname=%s\n", file, dat->Name);
        } else if ( EXD_IS_PIPE(dat) )
        {
           JA_TRACE("file %lx, pipe=%s\n", file, dat->Name);
        } else if ( EXD_IS_SOCKET(dat) )
        {
           JA_TRACE("file: %lx is socket!\n", file);
        } else {
           JA_TRACE("file: %lx, It is: %ld\n", file, ((dat)->Type & FSO_TYPE_MASK));
        }
        //......

        FreeDosObject(DOS_EXAMINEDATA,dat); // Free data when done
    }
    else
    {
        int error = IoErr();
        switch(error) {
            case 209:
                JA_TRACE("IoErr for file %lx was 209 (ACTION_UNKNOWN)\n", file);
                break;
            case 306:
                JA_TRACE("IoErr for file %lx was 306 (ERROR_IS_PIPE)\n", file);
                break;
            default:
                JA_TRACE("IoErr for file: %lx reading examine data: %ld\n", file, error);
        }
        //PrintFault(IoErr(),NULL); // failure - why ?
    }
}


STATIC int32 ASM hookfunc1( REG(a0, struct Hook *hook) UNUSED,
                                REG(a2, uint32 *counter), /* userdata */
                                REG(a1, struct FileHandle *afh))
    {
        if( NULL != afh->fh_MsgPort )     /* exclude NIL: streams */
        {
            if(afh->fh_OpenerPID == ((struct Process*)FindTask(NULL))->pr_ProcessID) {
                JA_TRACE("Found open stream: %lx by pid: %ld %s!\n", afh, afh->fh_OpenerPID, afh->fh_Interactive ? "interactive":"not interactive");
                //JA_TRACE("Ending stream: %lx\n", afh);
                //printFileHandleInfoInfo(afh);
                //SetFileHandleAttrTags(afh, FH_EndStream, DOSTRUE, TAG_DONE);
            }
            (*counter) ++;                /* increment counter */
        }
        return(0);                        /* 0 = continue scan */
    }

/**
 * jt_proxyExceptionHandler
 */
uint32 jt_proxyExceptionHandler(struct Library * SysBase, uint32 signals, struct sigaction *exceptData) {
    struct PthreadInitData * initData = GetExitData();
    JA_TRACE("jtpxh:  Singal "BYTETOBINARYPATTERN, BYTETOBINARY(signals));
    JA_TRACE("---------------------------------------------- JT Exception handler with signal: %lx SigMask is: %lx  SIGKILL:%lx\n", signals, initData->pd_SigMask, (1<<SIGKILL));

    if((signals & (1<<SIGINT)) == (1<<SIGINT)) {
        JA_TRACE("jtpxh:  got a sigint in JT, settign itnewrrupted\n");
        threadInterrupt(threadSelf());

    } else if((signals & (1<<SIGKILL)) == (1<<SIGKILL)) {
        // SIGKILL is only sent from the JVM
        JA_TRACE("jtpxh:  got a sigkill in JT, calling suspendtask\n");
        //
        //S//uspendTask(0L, 0);
        //Wait((1<<SIGKILL));
        // Try to get a stacktrace
        //Signal(signalHandlerPtr, SIGQUIT);
        //JA_TRACE("jtpxh:  signal for stacktrace, delay 100\n");
        //Delay(100);
        JA_TRACE("jtpxh:  done delay, removing task\n");
        struct thread *t = threadSelf();
        t->suspend_state = SUSP_BLOCKING;

        struct Hook H;
        uint32 count; /* we pass a pointer to this as 'userdata' */

        count =0;                         /* set initial count to zero  */
        H.h_Entry = (APTR) &hookfunc1;    /* initialise the hook struct */
        FileHandleScan(&H, &count, 0);

        JA_TRACE("There were %lu streams open.\n", count);
        JA_TRACE("Suspending task.\n");
        // suspend here, and then remtask in scanprocess..?
        RemTask(0);
        //SuspendTask(0L,0);
        //threadInterrupt(threadSelf());
        //signalChainedExceptionName("java/lang/InterruptedException", NULL, NULL);
    } else if((signals & (1<<SIGUSR1)) == (1<<SIGUSR1)) {
        JA_TRACE("jtpxh:  got a sigusr1 in JT\n");
        JA_TRACE("jtpxh:  Calling sa_handler %lx & %lx\n", signals, initData->pd_SigMask);
        //threadInterrupt(threadSelf());
        //RemTask(0L);
        // Should I remove myself here?
        //enableSuspend(threadSelf());
        //suspendHandler(signals);
        (*exceptData->sa_handler)(signals);
        //((struct Thread*)threadSelf())->suspend_state = SUSP_NONE;
    } else {//if(signals & initData->pd_SigMask) {
        JA_TRACE("jtpxh:  Calling sa_handler %lx & %lx\n", signals, initData->pd_SigMask);
        (*exceptData->sa_handler)(signals);
    }
    return signals;
}

/**
 * proxyExceptionHandler
 */
extern int isMainAmigaTask();

//extern void signalChainedExceptionName(char *excep_name, char *message, void *cause);

uint32 proxyExceptionHandler(struct Library * SysBase, uint32 signals, struct sigaction *exceptData) {
    struct PthreadInitData * initData = GetExitData();
    JA_TRACE("pxh:    Signal "BYTETOBINARYPATTERN, BYTETOBINARY(signals));
    JA_TRACE("-------------------- Exception handler with signal: %lx\n", signals);

    if((signals & (1<<SIGINT)) == (1<<SIGINT)) {
        if(signalHandlerPtr != NULL && signalHandlerPtr != FindTask(NULL)) {
            //JA_TRACE("pxh:    SIGINT Signal our existing signal handler...\n");
            Signal(signalHandlerPtr, signals);
            //JA_TRACE("pxh:    SIGINT ...Calling sa_handler %lx & %lx\n", signals, initData->pd_SigMask);

            if(isMainAmigaTask()) {
                JA_TRACE("pxh:    SIGINT Wait 1, then calling real exit\n");
                Wait(1);
                JA_TRACE("pxh:    SIGINT Got 1, calling real exit\n");
                realAmigaExit(1);
            } else {
                JA_TRACE("pxh:    SIGINT VM thread, do nothing\n");
            }
        } else if(signalHandlerPtr == FindTask(NULL)) {
            JA_TRACE("pxh:    SIGINT we're signal ahndler settgin singal\n");
            SetSignal(signals, signals);

        } else {
            JA_TRACE("pxh:    SIGINT NO signal handler, setting signal\n");
            SetSignal(signals, signals);
        }
    } else if((signals & (1<<SIGKILL)) == (1<<SIGKILL)) {
        if(isMainAmigaTask()) {
            JA_TRACE("pxh:    SIGKILL mainamigatask, do nothing\n");
            //exitVM(0);
            //Wait(1);
            //realAmigaExit(1);
        } else {
            JA_TRACE("pxh:    SIGKILL JVM thread removing task\n");
            //freeProcessInitData(initData);
            RemTask(0l);
        }
        
    } else {//if(signals & initData->pd_SigMask) {
        JA_TRACE("pxh:    OTHER SIG Calling sa_handler %lx & %lx\n", signals, initData->pd_SigMask);
        (*exceptData->sa_handler)(signals);
        JA_TRACE("pxh:    OTHER SIG Calledg sa_handler %lx & %lx\n", signals, initData->pd_SigMask);
            
    }
    return signals;
}

/**
 *
 struct PthreadInitData {
//    void * (*pd_Func)(void*);
    void *pd_Args;
    APTR pd_ExceptCode;
    APTR pd_ExceptData;
    char pd_Dummy;
    uint32 pd_SigMask;
    void * pd_Specific;
    //struct TimeRequest *pd_timerIO;
    //struct MsgPort *pd_timerMP;

} PthreadInitData;
 */

/**
 * jt_proxyFunction
 */
int jt_proxyfunction() {

    struct PthreadInitData * initData = (struct PthreadInitData *)GetExitData();
    void * (*func)(void*) = (void * (*)(void*))GetEntryData();
    void *args = initData->pd_Args;
    uint32 allsigs = 0;

    if(AllocSignal(SIGUSR1) == -1) {
        JA_TRACE("Couldn't allocate signal SIGUSR1 %d\n", SIGUSR1);
        return;
    }
    if(AllocSignal(SIGUSR2) == -1) {
        FreeSignal(SIGUSR1);
        JA_TRACE("Couldn't allocate signal SIGUSR2 %d\n", SIGUSR2);
        return;
    }
    JA_TRACE("Allocated SIGUSR1 and SIGUSR2\n");

    //JA_TRACE("START JAVA THREAD Args: %lx %lx initdata :%lx\n", args, &args, initData);
    FindTask(NULL)->tc_ExceptCode = &jt_proxyExceptionHandler;//initData->pd_ExceptCode;
    FindTask(NULL)->tc_ExceptData = initData->pd_ExceptData;
    sigaddset(&allsigs, SIGKILL);
    sigaddset(&allsigs, SIGINT);
    // add all signals to javathread
    SetExcept(allsigs,allsigs);
    JA_TRACE("Started new process, allsigs: %lx, Args: %lx %lx, process:%lx\n", allsigs, args, &args, FindTask(NULL));
    func(args);
    // COME BACK!
    //freeProcessInitData(initData);
    JA_TRACE("EXIT javathread process:%lx\n", FindTask(NULL));
    SetExcept(allsigs,0);
    FreeSignal(SIGUSR1);
    FreeSignal(SIGUSR2);
    FindTask(NULL)->tc_ExceptCode = NULL;
    FindTask(NULL)->tc_ExceptData = NULL;
    return 0;
}

// proxyfunction for 3 std jamvm threads

/**
 * proxyfunction
 */
int proxyfunction() {
    struct PthreadInitData * initData = GetExitData();//sysMalloc(sizeof (struct PthreadInitData));
    //((struct Process *)FindTask(NULL))->pr_ExitData = initData;
    void * (*func)(void*) = GetEntryData();//*initData->pd_Func;////
    void *args = initData->pd_Args;//GetExitData();
    uint32 allsigs = 0;

    if(AllocSignal(SIGUSR1) == -1) {
        JA_TRACE("Couldn't allocate signal SIGUSR1 %d\n", SIGUSR1);
        return;
    }
    if(AllocSignal(SIGUSR2) == -1) {
        FreeSignal(SIGUSR1);
        JA_TRACE("Couldn't allocate signal SIGUSR2 %d\n", SIGUSR2);
        return;
    }
    JA_TRACE("Allocated SIGUSR1 and SIGUSR2\n");

    JA_TRACE("START JVM THREAD Args: %lx %lx initdata :%lx [%s]\n", args, &args, initData, (const char *)((char * *)args)[0]);
    FindTask(NULL)->tc_ExceptCode = &proxyExceptionHandler;//initData->pd_ExceptCode;
    FindTask(NULL)->tc_ExceptData = initData->pd_ExceptData;
    // handle signal catching differently, do not exit on sigkill
    // do not abort on sigkill...? or perhaps do that
    if(strcmp((const char *)((char * *)args)[0], "Signal Handler")!=0) {
        //JA_TRACE("SetExcept hadnler for me\n");
        sigaddset(&allsigs, SIGKILL);
        sigaddset(&allsigs, SIGINT);
        SetExcept(allsigs,allsigs);
    }
    JA_TRACE("Started new process, allsigs: %lx, Args: %lx %lx, process:%lx\n", allsigs, args, &args, FindTask(NULL));
    func(args);
    // COME BACK! Free this memory somehow.
    //freeProcessInitData(initData);
    FreeSignal(SIGUSR1);
    FreeSignal(SIGUSR2);
    JA_TRACE("EXIT JVM process:%lx\n", FindTask(NULL));
    return 0;
}


/**
 * initPthreadData
 * Inits data for use in JamVM threads
 */
struct PthreadInitData * initPthreadInitData(void * args) {
    struct PthreadInitData * initData;
    initData = sysMalloc(sizeof (struct PthreadInitData));

    //if(args!= NULL){
    //    JA_TRACE("Args: %lx %lx\n", args, &args);
    //}

    initData->pd_Args = args;
    initData->pd_ExceptCode = FindTask(NULL)->tc_ExceptCode;
    initData->pd_ExceptData = FindTask(NULL)->tc_ExceptData;
    initData->pd_Dummy = 'M';
    // Could each thread has its own already opened Timer?
    //initData->pd_timerMP = AllocSysObjectTags(ASOT_PORT, TAG_END);
    //if(initData->pd_timerMP == NULL) {
    //    JA_TRACE("pthread-create: Couldn't alloc memory for Timer msgPort\n");
    //    freeProcessInitData(initData);
    //    return NULL;
    //}
    //initData->pd_timerIO = AllocSysObjectTags(ASOT_IOREQUEST,
    //            ASOIOR_Size, sizeof(struct TimeRequest),
    //            ASOIOR_ReplyPort, ((struct Process *)FindTask(NULL))->pr_MsgPort,
    //            TAG_END);

    //if(initData->pd_timerIO == NULL) {
    //    JA_TRACE("CondWait: Couldn't alloc memory for Timer IO req\n");
    //    freeProcessInitData(initData);
    //    return NULL;
    //}
    return initData;
}


/**
 * Free thrad init data
 */
void freeProcessInitData(struct PthreadInitData * initData) {
    if(initData == NULL) {
        return;
    }
    /*JA_TRACE("Freeing process init data\n");
    if(initData->pd_timerIO != NULL) {
        if (!(CheckIO(initData->pd_timerIO))) {
            AbortIO(initData->pd_timerIO);
        }
        WaitIO(initData->pd_timerIO);
        CloseDevice((struct IORequest *)initData->pd_timerIO);
    }
    FreeSysObject(ASOT_PORT, initData->pd_timerMP);
    FreeSysObject(ASOT_IOREQUEST, initData->pd_timerIO);

    initData->pd_timerMP = NULL;
    initData->pd_timerIO = NULL;
    */
    sysFree(initData);
}

// COME BACK!
// make pthread_cr3eate behave more as one function

/**
 * pthread_create
 */
int pthread_create(pthread_t * tid, void * attributes, void * (*func)(void*), void * args) {
    struct PthreadInitData * initData = initPthreadInitData(args);
    (*(tid) = CreateNewProcTags(
            NP_Entry, &jt_proxyfunction,
            NP_EntryData, func,
            NP_Name, "Java Thread",
            NP_Child, TRUE,
            NP_ExitData, initData,
            NP_FinalData, MAGIC_SECRET_PROCESSDATA,
            //NP_Priority, -20,
            TAG_END));
    JA_TRACE("Started JAVA THREAD: %lx\n", *tid);
    return (*tid)!=NULL?0:1;
//    return pthread_create2(tid, attributes, func, args, "Java thread");
}

/**
 * pthread_create2
 */
int pthread_create2(pthread_t * tid, void * attributes, void * (*func)(void*), void ** args, char * name) {
    struct PthreadInitData * initData = initPthreadInitData(args);
    
    (*(tid) = CreateNewProcTags(
            NP_Entry, &proxyfunction,
            NP_EntryData, func,
            NP_Name, name,
            NP_Child, TRUE,
            NP_ExitData, initData,
            NP_FinalData, MAGIC_SECRET_JAVATHREADDATA,
            NP_Priority, 20,
            TAG_END));
    if(strcmp(name, "Signal Handler")==0) {
        JA_TRACE("Started our signal handler: %lx\n", *tid);
        signalHandlerPtr = *tid;
    }

    return (*tid)!=NULL?0:1;
}
