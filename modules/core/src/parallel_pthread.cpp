#ifdef HAVE_LIBPTHREAD
#if ANDROID
    #include <android/log.h>
#else
    #include <errno.h>
    #include <sys/types.h>
    #define gettid() syscall(__NR_gettid)
    #include <string.h>
#endif

#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <stdio.h>


#include <vector>
#include <typeinfo>
#include <pthread.h>

#include "parallel_pthread.hpp"

#define THREADING_FRAMEWORK_LOG 0
static bool shouldLog=false;
#if THREADING_FRAMEWORK_LOG
    #if ANDROID && !defined(ANDROID_CONSOLE)
        #define LOG_TAG "PTHREAD_FRAMEWORK"
        #define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
        #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
        #define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
        #define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
    #else
        #define LOGD(_str, ...) do{if(shouldLog){printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);}} while(0)
        #define LOGI(_str, ...) do{if(shouldLog){printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);}} while(0)
        #define LOGW(_str, ...) do{if(shouldLog){printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);}} while(0)
        #define LOGE(_str, ...) do{if(shouldLog){printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);}} while(0)
    #endif
#else
    #define LOGD(...) do {} while(0)
    #define LOGI(...) do {} while(0)
    #define LOGW(...) do {} while(0)
    #define LOGE(...) do {} while(0)
#endif

#if ANDROID
    #define LOG_TAG1 "PTHREAD_FRAMEWORK!"
    #define LOGD1(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG1, __VA_ARGS__))
    #define LOGI1(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG1, __VA_ARGS__))
    #define LOGW1(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG1, __VA_ARGS__))
    #define LOGE1(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG1, __VA_ARGS__))
#else
    #define LOGD1(...) do {} while(0)
    #define LOGI1(...) do {} while(0)
    #define LOGW1(...) do {} while(0)
    #define LOGE1(...) do {} while(0)
#endif

using namespace std;
using namespace cv;
using namespace tf;

static void setCurrentThreadAffinityMask(int mask)
{
    pid_t pid=gettid();
    int syscallres=syscall(__NR_sched_setaffinity, pid, sizeof(mask), &mask);
    if (syscallres)
        LOGE("Error in the syscall setaffinity: mask=%d=0x%x err=%d=0x%x", mask, mask, errno, errno);
}

static void initThreadManagerPoolInternals();
static void* workcycleThreadTask(void*);

class ThreadTask
{
    public:
        typedef void(*ThreadFunction)(const void* operation, const cv::Range& range);

        struct TaskData
        {
            ThreadFunction function;
            const void* operation;
            cv::Range range;

            TaskData(): function(NULL), operation(NULL) {};

            TaskData(ThreadFunction f,  const void* op, const cv::Range& r)
                : function(f), operation(op), range(r) {};

            bool isValid() const
            {
                return (function != NULL) && (operation != NULL) ;
            };

            void clear()
            {
                function=NULL;
                operation=NULL;
            };

            void log() const
            {
                LOGD("TaskData: function=0x%x, operation=0x%x, range=[%d, %d):%d",
                     (int)(intptr_t)function, (int)(intptr_t)operation, range.start, range.end );
            }
        };

        enum ThreadTaskState
        {
            STATE_UNINITIALIZED=0,
            STATE_SLEEPING,
            STATE_RUNNING,
            STATE_DYING,
            STATE_DESTROYED=1024
        };

        void run(const TaskData& td)
        {
            LOGD("ThreadTask::run is called for thread %d -- start", id);
            td.log();
            if (state==STATE_DESTROYED)
            {
                LOGE("ERROR: call of method ThreadTask::run for crashed object");
                return;
            }
            pthread_mutex_lock(&taskMutex);
            {
                if (state==STATE_RUNNING)
                {
                    LOGE("WARNING: the current thread id %d is running when the method ThreadTask::run is called --- sleeping", id);
                    pthread_cond_wait(&condTaskIsReady, &taskMutex);
                    LOGI("Method ThreadTask::run for the thread id %d is waked up by the signal condTaskIsReady", id);
                }

                if (state != STATE_SLEEPING)
                {
                    LOGE("ERROR: the current thread id %d is not in SLEEPING state when the method ThreadTask::run is passing data to it", id);
                    pthread_mutex_unlock(&taskMutex);
                    throw(std::exception());
                }
                state=STATE_RUNNING;
                data=td;
                LOGI("sending the signal condTaskRun to the thread id=%d", id);
                pthread_cond_signal(&condTaskRun);
            }
            pthread_mutex_unlock(&taskMutex);
            LOGD("ThreadTask::run is called for thread %d -- end", id);
        }

        void join()
        {
            pthread_mutex_lock(&taskMutex);
            {
                if (state != STATE_RUNNING)
                {
                    pthread_mutex_unlock(&taskMutex);
                    LOGI("Warning: called the method ThreadTask::join but the thread is not in running state "
                            "(probably job is done), "
                            "thread id=%d", id);
                    return;
                }

                LOGD("the current thread id %d is running when the method ThreadTask::join is called --- sleeping", id);
                pthread_cond_wait(&condTaskIsReady, &taskMutex);
                LOGD("Method ThreadTask::join for the thread id %d is waked up by the signal condTaskIsReady", id);
            }

            pthread_mutex_unlock(&taskMutex);
        }

        void die()
        {
            LOGD("ThreadTask::die is called for the thread id=%d --- start", id);
            if (state==STATE_DESTROYED)
            {
                LOGE("ERROR: call of method ThreadTask::makeDie for crashed object");
                return;
            }

            pthread_mutex_lock(&taskMutex);
            {
                if (state==STATE_RUNNING)
                {
                    LOGE("WARNING: the current thread id %d is running when the method ThreadTask::makeDie is called --- sleeping", id);
                    pthread_cond_wait(&condTaskIsReady, &taskMutex);
                    LOGI("Method ThreadTask::makeDie for the thread id %d is waked up by the signal condTaskIsReady", id);
                }

                state=STATE_DYING;
                LOGI("sending the signal condTaskRun to the thread id=%d", id);
                pthread_cond_signal(&condTaskRun);

                LOGE("waiting the response from the  thread id=%d that it is died --- sleeping", id);
                pthread_cond_wait(&condTaskIsReady, &taskMutex);
                LOGI("Method ThreadTask::makeDie for the thread id %d is waked up by the signal condTaskIsReady --- workthread is died", id);
            }

            pthread_mutex_unlock(&taskMutex);
            LOGD("ThreadTask::die is called for the thread id=%d --- end", id);
        }

        ThreadTask()
            :id(-1), affinityMask(-1),
            shouldSetAffinityMask(true),
            state(STATE_UNINITIALIZED)
        {
            LOGD("A ThreadTask is created");
        }

        virtual ~ThreadTask()
        {
            //TODO: we cannot kill the thread, pthread_cancel is not implemented on Android
            //-- how to stop it quicker and decrease the risk of crash?
            //The destructor should be called VERY ACCURATELY
            LOGI("DESTRUCTOR START ThreadTask id=%d, state=%d", id, (int)state);
            state=STATE_DESTROYED;
            pthread_cond_destroy(&condTaskIsReady);
            pthread_cond_destroy(&condTaskRun);
            pthread_mutex_destroy(&taskMutex);
            LOGI("DESTRUCTOR END ThreadTask id=%d, state=%d", id, (int)state);
        }

    private:
        ThreadTask(const ThreadTask&)
        {
            LOGE("Copying ThreadTask object is prohibited");
        };

    protected:
        int id;
        int affinityMask;
        bool shouldSetAffinityMask;

        ThreadTaskState state;
        TaskData data;

        pthread_t currentThread;
        pthread_mutex_t taskMutex;
        pthread_cond_t condTaskIsReady;
        pthread_cond_t condTaskRun;

        friend class tf::ThreadManager;
        friend void* workcycleThreadTask(void*);
        friend void initThreadManagerPoolInternals();

        void init(int _id, int _affinityMask, bool _shouldSetAffinityMask)
        {
            LOGD("ThreadTask::init for id=%d is called", _id);
            if (state!=STATE_UNINITIALIZED)
            {
                LOGE("ERROR: the method ThreadTask::init() is called for the task, which has been initialized");
                throw(std::exception());//TODO: make exception
            }

            int res=0;
            res=pthread_mutex_init(&taskMutex, NULL);//TODO: should be attributes?
            if (res)
            {
                LOGE("ERROR: error in pthread_mutex_init(&taskMutex, NULL) is %d", res);
                state=STATE_DESTROYED;
                return;
            }

            res=pthread_cond_init (&condTaskRun, NULL);//TODO: should be attributes?
            if (res)
            {
                LOGE("ERROR: error pthread_cond_init (&condTaskRun, NULL) is %d", res);
                pthread_mutex_destroy(&taskMutex);
                state=STATE_DESTROYED;
                return;
            }

            res=pthread_cond_init (&condTaskIsReady, NULL);//TODO: should be attributes?
            if (res)
            {
                LOGE("ERROR: error pthread_cond_init (&condTaskRun, NULL) is %d", res);
                pthread_cond_destroy(&condTaskRun);
                pthread_mutex_destroy(&taskMutex);
                state=STATE_DESTROYED;
                return;
            }

            id=_id;
            affinityMask=_affinityMask;
            shouldSetAffinityMask=_shouldSetAffinityMask;
            state=STATE_SLEEPING;

            pthread_create(&currentThread, NULL, workcycleThreadTask, (void*)this); //TODO: should be attributes?
            //TODO: should it be inside mutex?

            LOGD("A ThreadTask id=%d is started", id);
        }

        void workcycle()
        {
            if (state==STATE_UNINITIALIZED)
            {
                LOGE("%%%%[%d] ERROR: the thread has not been initialized, but the ThreadTask::workcycle is called", id);
                return;
            }
            if (state==STATE_DESTROYED)
            {
                LOGE("%%%%[%d] ERROR: the thread is destroyed but the ThreadTask::workcycle is called", id);
                return;
            }

            //pthread_detach(pthread_self()); //ATTENTION: the threads should be joinable
            if (shouldSetAffinityMask)
            {
                setCurrentThreadAffinityMask(affinityMask);
                LOGD("%%%%[%d] affinityMask 0x%x is set for the thread %d", id, affinityMask, id);
            } else {
                LOGD("%%%%[%d] affinityMask is NOT set for the thread %d", id, id);
            }

            LOGE1("%%%%[%d] ThreadTask --- GOING INTO THE MAIN CYCLE", id);

            for(;;)
            {
                LOGD("%%%%[%d] Started the next iteration of the main workcycle in the thread id=%d", id, id);
                if (state==STATE_DESTROYED)
                {
                    LOGE("%%%%[%d] ERROR: the thread is destroyed while the ThreadTask::workcycle is not finished", id);
                    break;
                }
                pthread_mutex_lock(&taskMutex);
                {
                    if (state==STATE_DYING)
                    {
                        LOGW("%%%%[%d] The state of the thread %d is turned out to be STATE_DYING -- exiting the main workcycle", id, id);
                        pthread_mutex_unlock(&taskMutex);
                        break;
                    }
                    if (state == STATE_SLEEPING)
                    {//the main thread could run this thread just after starting
                        LOGW("%%%%[%d] Sleeping the thread %d to wait the signal condTaskRun in the main workcycle", id, id);
                        pthread_cond_wait(&condTaskRun, &taskMutex);
                        LOGW("%%%%[%d] Waking up the thread %d after receiving the signal condTaskRun in the main workcycle", id, id);
                    }
                    if (state != STATE_RUNNING)
                    {
                        LOGW("%%%%[%d] WARNING: in ThreadTask::workcycle() for thread id %d: waked up after pthread_cond_wait(&condTaskRun), but state is %d",
                                id,
                                id, (int)state);

                        if (state != STATE_DESTROYED)
                            pthread_mutex_unlock(&taskMutex);
                        continue;
                    }

                    //state == STATE_RUNNING
                    if (!data.isValid())
                    {
                        LOGE("%%%%[%d] ERROR: in ThreadTask::workcycle() for thread id %d: waked up after pthread_cond_wait(&condTaskRun), but data is not valid",
                                id,
                                id);
                        pthread_mutex_unlock(&taskMutex);
                        break;
                    }
                }
                pthread_mutex_unlock(&taskMutex);

                LOGD("%%%%[%d] in the main workcycle of the thread %d --- before the calling the work function", id, id);
                (*data.function)(data.operation, data.range);
                LOGD("%%%%[%d] in the main workcycle of the thread %d --- after the calling the work function", id, id);

                pthread_mutex_lock(&taskMutex);
                {
                    state=STATE_SLEEPING;
                    data.clear();
                    LOGD("%%%%[%d] In the main workcycle of the thread %d --- sending the signal condTaskIsReady", id, id);
                    pthread_cond_signal(&condTaskIsReady);
                }
                pthread_mutex_unlock(&taskMutex);
            }

            LOGE("%%%%[%d] The main cycle in ThreadTask::workcycle is finished --- dying", id);
            if (state!=STATE_DESTROYED)
            {
                LOGI("%%%%[%d] Sending the signal that the ThreadTask::workcycle is finished", id);
                pthread_mutex_lock(&taskMutex);
                pthread_cond_signal(&condTaskIsReady);
                pthread_mutex_unlock(&taskMutex);
            }
            else
                LOGE("%%%%[%d] ERROR: state==STATE_DESTROYED during dying", id);

            LOGE("%%%%[%d] The ThreadTask::workcycle is finished, state=%d", id, state);
        }
};

static void* workcycleThreadTask(void* p)
{
    ((ThreadTask*)p)->workcycle();
    return NULL;
}

static std::vector<ThreadTask*> tasks;
static ThreadManager::ThreadManagerParameters params;
static bool isPoolInitialized=false;
static pthread_mutex_t* pThreadManagerMutex=0;

enum ThreadManagerState {
    STATE_THREAD_MANAGER_SLEEP=0,
    STATE_THREAD_MANAGER_RUN=1
};

static ThreadManagerState stateThreadManager=STATE_THREAD_MANAGER_SLEEP;

static void initThreadManagerMutex()
{
    if (pThreadManagerMutex)
        return;

    pThreadManagerMutex=new pthread_mutex_t;

    int res=pthread_mutex_init(pThreadManagerMutex, NULL);
    if (res)
    {
        delete pThreadManagerMutex;
        pThreadManagerMutex=0;
        throw std::exception();
    }
}

static void lockThreadManagerMutex()
{
    initThreadManagerMutex();
    pthread_mutex_lock(pThreadManagerMutex);
}

static void unlockThreadManagerMutex()
{
    if (pThreadManagerMutex==0)
    {
        throw std::exception();
    }
    pthread_mutex_unlock(pThreadManagerMutex);
}

ThreadManager::ThreadManagerParameters ThreadManager::getThreadManagerParameters()
{
    lockThreadManagerMutex();
    ThreadManager::ThreadManagerParameters res=params;
    unlockThreadManagerMutex();
    return res;
}

static void initThreadManagerPoolInternals();
bool ThreadManager::initPool(const ThreadManagerParameters& params1)
{
    lockThreadManagerMutex();

    if (stateThreadManager != STATE_THREAD_MANAGER_SLEEP)
    {
        unlockThreadManagerMutex();
        LOGE("ERROR: initPool is called while stateThreadManager != STATE_THREAD_MANAGER_SLEEP");
        return false;
    }
    if (isPoolInitialized)
    {
        unlockThreadManagerMutex();
        LOGE("ERROR: initPool is called, but it has been initialized");
        return false;
    }
    params=params1;
    initThreadManagerPoolInternals();

    unlockThreadManagerMutex();
    return true;
}

bool ThreadManager::initPool()
{
    lockThreadManagerMutex();
    if (stateThreadManager != STATE_THREAD_MANAGER_SLEEP)
    {
        unlockThreadManagerMutex();
        LOGE("WARNING: initPool is called while stateThreadManager != STATE_THREAD_MANAGER_SLEEP");
        return false;
    }
    if (isPoolInitialized)
    {
        unlockThreadManagerMutex();
        LOGD("initPool is called, but it has been initialized");
        return true;
    }
    initThreadManagerPoolInternals();

    unlockThreadManagerMutex();
    return true;
}

//Attention: the function initThreadManagerPoolInternals should be called ALWAYS inside lockThreadManagerMutex() ... unlockThreadManagerMutex()
static void initThreadManagerPoolInternals()
{
    LOGD("ThreadManager::init -- start");
    if (isPoolInitialized)
    {
        LOGD("ThreadManager::init -- pool is initialized -- return");
        return;
    }

    isPoolInitialized=true;

    if (params.sched == ThreadManager::NO_PARALLEL)
    {
        LOGD("ThreadManager::initPool: params.sched == NO_PARALLEL --- return");
        return;
    }


    LOGD("ThreadManager::init -- before resizing tasks");
    tasks.resize(params.sizeThreadPool);
    LOGD("ThreadManager::init -- after resizing tasks");

    for(size_t i=0; i < tasks.size(); i++)
    {
        int proc=i % params.numProcessors;
        LOGD("ThreadManager::init -- before initializing task %d", (int)i);
        tasks[i]=new ThreadTask();
        tasks[i]->init(i, 1 << proc, params.shouldSetAffinityMask);
        LOGD("ThreadManager::init -- after initializing task %d", (int)i);
    }
    LOGD("ThreadManager::init -- end");
}

bool ThreadManager::clearPool()
{
    LOGD("ThreadManager::clearPool -- start");
    lockThreadManagerMutex();
    if (! isPoolInitialized)
    {
        unlockThreadManagerMutex();
        LOGD("ThreadManager::clearPool -- pool is not initialized -- end");
        return false;
    }

    if (stateThreadManager != STATE_THREAD_MANAGER_SLEEP)
    {
        unlockThreadManagerMutex();
        LOGE("ERROR: clearPool is called while stateThreadManager != STATE_THREAD_MANAGER_SLEEP");
        return false;
    }

    for(size_t i=0; i < tasks.size(); i++)
    {
        LOGD("ThreadManager::clear -- before making task %d to die", (int)i);
        tasks[i]->die();
        LOGD("ThreadManager::clear -- after making task %d to die", (int)i);
        LOGD("ThreadManager::clear -- before deleting task %d", (int)i);
        delete tasks[i];
        LOGD("ThreadManager::clear -- after deleting task %d", (int)i);
    }

    LOGD("ThreadManager::clear -- before clearing tasks");
    tasks.clear();
    LOGD("ThreadManager::clear -- after clearing tasks");
    isPoolInitialized=false;
    LOGD("ThreadManager::clearPool -- end");
    unlockThreadManagerMutex();
    return true;
}

void ThreadManager::run(ThreadFunction function, const void* operation, const cv::Range& range)
{
    LOGD("ThreadManager::run -- start");
    LOGD("ThreadManager::run : function=%x, operation=%x, range=[%d,%d):%d",
            (int)(intptr_t)function, (int)(intptr_t)operation, range.start, range.stop);


    lockThreadManagerMutex();
    if ( (params.sched == NO_PARALLEL) || (stateThreadManager != STATE_THREAD_MANAGER_SLEEP))
    {
        unlockThreadManagerMutex();
        LOGD("ThreadManager::run -- scheduler mode NO_PARALLEL%s", ((params.sched == NO_PARALLEL)?"":" (since stateThreadManager != STATE_THREAD_MANAGER_SLEEP)"));
        LOGD("ThreadManager::run -- before calling the operation function");
        (*function)(operation, range);
        LOGD("ThreadManager::run -- after calling the operation function");
        LOGD("ThreadManager::run -- return");
        return;
    }

    LOGD("ThreadManager::run -- before initializing");
    initThreadManagerPoolInternals();
    LOGD("ThreadManager::run -- after initializing");

    stateThreadManager = STATE_THREAD_MANAGER_RUN;

    unlockThreadManagerMutex();

    LOGD("ThreadManager::run -- before setting variables");
    int begin = range.start;
    int end = range.end;
    LOGD("ThreadManager::run: beign=%d, end=%d", begin, end);

    int numGrains = (end-begin);
    if (begin + numGrains < end)
        numGrains++;
    LOGD("ThreadManager::run: numGrains=%d", numGrains);

    int numThreadsToUse=params.sizeThreadPool;
    LOGD("ThreadManager::run: numThreadsToUse=%d", numThreadsToUse);

    int grainsStep = numGrains / numThreadsToUse;
    LOGD("ThreadManager::run: grainsStep=%d", grainsStep);
    int grainsRemainder = numGrains - grainsStep * numThreadsToUse;
    LOGD("ThreadManager::run: grainsRemainder=%d", grainsRemainder);

    LOGD("ThreadManager::run -- after setting variables");
    LOGD("ThreadManager::run -- before running cycle");

    int currentBegin=begin;
    for(int i=0; i < numThreadsToUse; i++)
    {
        if (currentBegin >= end) // operator '>=' is used, since [x; x) is empty for any x
            break;

        int curStepInGrains = grainsStep;
        if (grainsRemainder > 0)
        {
            curStepInGrains+=1;
            grainsRemainder--;
        }

        if (curStepInGrains == 0)
        {}

        int curStep = curStepInGrains;

        int currentEnd=std::min(currentBegin+curStep, end);// "-1" is not required

        ThreadTask::TaskData data(function, operation, cv::Range(currentBegin, currentEnd));

        tasks[i]->run(data);

        currentBegin=currentEnd;//"+1" is not required
    }

    LOGD("ThreadManager::run -- after running cycle: last handled element is %d", currentBegin - 1);
    LOGD("ThreadManager::run -- before joining cycle");

    for(int i=0; i < numThreadsToUse; i++)
        tasks[i]->join();

    LOGD("ThreadManager::run -- after joining cycle");
    lockThreadManagerMutex();
    if (stateThreadManager == STATE_THREAD_MANAGER_RUN)
        stateThreadManager = STATE_THREAD_MANAGER_SLEEP;
    else
        LOGE("WARNING: very strange: after joining the variable stateThreadManager has value %d instead of STATE_THREAD_MANAGER_RUN==%d",
                (int)stateThreadManager, (int)STATE_THREAD_MANAGER_RUN);

    unlockThreadManagerMutex();
    LOGD("ThreadManager::run -- end");
}

void ThreadManager::doLog(bool _shouldLog)
{
    shouldLog=_shouldLog;
}

#endif
