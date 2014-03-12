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
        #define LOG_TAG "cv::PosixThreadingManager"
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
    #define LOG_TAG1 "cv::PosixThreadingManager!"
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

                pthread_cond_wait(&condTaskIsReady, &taskMutex);
            }

            pthread_mutex_unlock(&taskMutex);
        }

        void die()
        {
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

                pthread_cond_wait(&condTaskIsReady, &taskMutex);
            }

            pthread_mutex_unlock(&taskMutex);
        }

        ThreadTask()
            :id(-1), affinityMask(-1),
            shouldSetAffinityMask(true),
            state(STATE_UNINITIALIZED)
        {}

        virtual ~ThreadTask()
        {
            //TODO: we cannot kill the thread, pthread_cancel is not implemented on Android
            //-- how to stop it quicker and decrease the risk of crash?
            //The destructor should be called VERY ACCURATELY
            state=STATE_DESTROYED;
            pthread_cond_destroy(&condTaskIsReady);
            pthread_cond_destroy(&condTaskRun);
            pthread_mutex_destroy(&taskMutex);
        }

    private:
        ThreadTask(const ThreadTask&)
        {};

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

        friend class cv::PosixThreadManager;
        friend void* workcycleThreadTask(void*);
        friend void initThreadManagerPoolInternals();

        void init(int _id, int _affinityMask, bool _shouldSetAffinityMask)
        {
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
            if ((state==STATE_UNINITIALIZED) || (state==STATE_DESTROYED))
            {
                LOGE("ERROR: ThreadTask::workcycle() was called in invalid state");
                return;
            }

            if (shouldSetAffinityMask)
                setCurrentThreadAffinityMask(affinityMask);

            for(;;)
            {
                if (state==STATE_DESTROYED)
                {
                    LOGE("ERROR: the thread %d is destroyed while the ThreadTask::workcycle is not finished", id);
                    break;
                }
                pthread_mutex_lock(&taskMutex);
                {
                    if (state==STATE_DYING)
                    {
                        pthread_mutex_unlock(&taskMutex);
                        break;
                    }
                    if (state == STATE_SLEEPING)
                    {//the main thread could run this thread just after starting
                        pthread_cond_wait(&condTaskRun, &taskMutex);
                    }
                    if (state != STATE_RUNNING)
                    {
                        if (state != STATE_DESTROYED)
                            pthread_mutex_unlock(&taskMutex);
                        continue;
                    }

                    //state == STATE_RUNNING
                    if (!data.isValid())
                    {
                        LOGE("ERROR: in ThreadTask::workcycle() for thread id %d: waked up with invalid data", id);
                        pthread_mutex_unlock(&taskMutex);
                        break;
                    }
                }
                pthread_mutex_unlock(&taskMutex);

                (*data.function)(data.operation, data.range);

                pthread_mutex_lock(&taskMutex);
                {
                    state=STATE_SLEEPING;
                    data.clear();
                    pthread_cond_signal(&condTaskIsReady);
                }
                pthread_mutex_unlock(&taskMutex);
            }

            if (state!=STATE_DESTROYED)
            {
                LOGI("Sending the signal that the ThreadTask::workcycle is finished");
                pthread_mutex_lock(&taskMutex);
                pthread_cond_signal(&condTaskIsReady);
                pthread_mutex_unlock(&taskMutex);
            }
        }
};

static void* workcycleThreadTask(void* p)
{
    ((ThreadTask*)p)->workcycle();
    return NULL;
}

static std::vector<ThreadTask*> tasks;
static PosixThreadManager::Parameters params;
static bool isPoolInitialized=false;
static pthread_mutex_t* pThreadManagerMutex=0;

enum ThreadManagerState
{
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

PosixThreadManager::Parameters PosixThreadManager::getParameters()
{
    lockThreadManagerMutex();
    PosixThreadManager::Parameters res=params;
    unlockThreadManagerMutex();
    return res;
}

static void initThreadManagerPoolInternals();
bool PosixThreadManager::initPool(const Parameters& _params)
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
    params = _params;
    initThreadManagerPoolInternals();

    unlockThreadManagerMutex();
    return true;
}

bool PosixThreadManager::initPool()
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

// Attention: the function initThreadManagerPoolInternals should be called ALWAYS
// inside lockThreadManagerMutex() ... unlockThreadManagerMutex()
static void initThreadManagerPoolInternals()
{
    if (isPoolInitialized)
        return;

    isPoolInitialized=true;

    if (params.scheduler == PosixThreadManager::NO_PARALLEL)
        return;

    tasks.resize(params.sizeThreadPool);

    for(size_t i=0; i < tasks.size(); i++)
    {
        int proc=i % params.numProcessors;
        tasks[i]=new ThreadTask();
        tasks[i]->init(i, 1 << proc, params.shouldSetAffinityMask);
    }
}

bool PosixThreadManager::clearPool()
{
    LOGD("PosixThreadManager::clearPool -- start");
    lockThreadManagerMutex();
    if (! isPoolInitialized)
    {
        unlockThreadManagerMutex();
        LOGD("PosixThreadManager::clearPool -- pool is not initialized -- end");
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
        tasks[i]->die();
        delete tasks[i];
    }

    tasks.clear();
    isPoolInitialized=false;
    unlockThreadManagerMutex();

    return true;
}

void PosixThreadManager::run(ThreadFunction function, const void* operation, const cv::Range& range)
{
    LOGD("PosixThreadManager::run : function=%x, operation=%x, range=[%d,%d):%d",
            (int)(intptr_t)function, (int)(intptr_t)operation, range.start, range.stop);


    lockThreadManagerMutex();
    if ( (params.scheduler == NO_PARALLEL) || (stateThreadManager != STATE_THREAD_MANAGER_SLEEP))
    {
        unlockThreadManagerMutex();
        LOGD("PosixThreadManager::run -- scheduler mode NO_PARALLEL%s", ((params.scheduler == NO_PARALLEL) ? "" :
                                                                     " (since stateThreadManager != STATE_THREAD_MANAGER_SLEEP)"));
        (*function)(operation, range);

        return;
    }

    initThreadManagerPoolInternals();

    stateThreadManager = STATE_THREAD_MANAGER_RUN;

    unlockThreadManagerMutex();

    int begin = range.start;
    int end = range.end;

    int numGrains = (end-begin);
    if (begin + numGrains < end)
        numGrains++;

    int numThreadsToUse=params.sizeThreadPool;
    int grainsStep = numGrains / numThreadsToUse;
    int grainsRemainder = numGrains - grainsStep * numThreadsToUse;

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

    LOGD("PosixThreadManager::run -- before joining cycle");

    for(int i=0; i < numThreadsToUse; i++)
        tasks[i]->join();

    LOGD("PosixThreadManager::run -- after joining cycle");

    lockThreadManagerMutex();
    if (stateThreadManager == STATE_THREAD_MANAGER_RUN)
        stateThreadManager = STATE_THREAD_MANAGER_SLEEP;
    else
        LOGE("WARNING: very strange: after joining the variable stateThreadManager has value %d instead of STATE_THREAD_MANAGER_RUN==%d",
                (int)stateThreadManager, (int)STATE_THREAD_MANAGER_RUN);

    unlockThreadManagerMutex();
}

void PosixThreadManager::doLog(bool _shouldLog)
{
    shouldLog=_shouldLog;
}

#endif
