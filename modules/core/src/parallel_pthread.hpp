#pragma once
#include "cvconfig.h"

#ifdef HAVE_LIBPTHREAD

namespace tf
{

class ThreadManager
{
public:
    enum ThreadManagerScheduler
    {
        NO_PARALLEL=0,
        SIMPLE_SCHEDULER=1
    };

    struct ThreadManagerParameters
    {
        ThreadManagerScheduler sched;
        int sizeThreadPool;
        int numProcessors;
        bool shouldSetAffinityMask;

        ThreadManagerParameters():
            sched(SIMPLE_SCHEDULER),
            sizeThreadPool(4),
            numProcessors(4),
            shouldSetAffinityMask(false)
        {};
    };

    typedef void(*ThreadFunction)(const void* operation, const cv::Range& range);

    static ThreadManagerParameters getThreadManagerParameters();

    static bool initPool();
    static bool initPool(const ThreadManagerParameters& params);
    static bool clearPool();
    static void run(ThreadFunction function, const void* operation, const cv::Range& range);
    static void doLog(bool shouldLog);
};

template<typename Body>
static void __parallel_for_function(const void* operation, const cv::Range& range)
{
    const Body& body=*((Body*)operation);
    body(range);
}

template<typename Body>
static void parallel_for( const Body& body, const cv::Range& range )
{
    ThreadManager::run(__parallel_for_function<Body>, (const void*)&body, range);
}

}

#endif