#pragma once
#include "cvconfig.h"

#ifdef HAVE_LIBPTHREAD

namespace cv
{

class PosixThreadManager
{
public:
    enum SchedulerType
    {
        NO_PARALLEL=0,
        SIMPLE_SCHEDULER=1
    };

    struct Parameters
    {
        SchedulerType scheduler;
        int sizeThreadPool;
        int numProcessors;
        bool shouldSetAffinityMask;

        Parameters():
            scheduler(SIMPLE_SCHEDULER),
            sizeThreadPool(cv::getNumberOfCPUs()),
            numProcessors(cv::getNumberOfCPUs()),
            shouldSetAffinityMask(false)
        {};

        Parameters(int numThreads):
            scheduler(SIMPLE_SCHEDULER),
            sizeThreadPool(numThreads),
            numProcessors(numThreads),
            shouldSetAffinityMask(false)
        {};
    };

    typedef void(*ThreadFunction)(const void* operation, const cv::Range& range);

    static Parameters getParameters();

    static bool initPool();
    static bool initPool(const Parameters& params);
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
    PosixThreadManager::run(__parallel_for_function<Body>, (const void*)&body, range);
}

}

#endif