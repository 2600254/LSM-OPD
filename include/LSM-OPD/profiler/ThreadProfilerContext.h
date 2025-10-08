#pragma once
#include "LSM-OPD/profiler/profiler.h"


namespace LSMOPD {
    class ThreadProfilerContext {
    public:
        static void SetCurrent(ThreadProfiler* profiler);
        static ThreadProfiler* GetCurrent();

    private:
        static thread_local ThreadProfiler* current_profiler_;
    };

}