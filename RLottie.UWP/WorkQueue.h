#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

#include "LottieAnimation.h"

class WorkItem {
public:
    int frame;
    size_t w;
    size_t h;
    std::shared_ptr<uint8_t[]> pixels;

    WorkItem(int frame, size_t w, size_t h, std::shared_ptr<uint8_t[]> pixels)
    : frame(frame),
      w(w),
      h(h),
      pixels(pixels)
    {
        
    }
};

class WorkQueue
{
    std::condition_variable work_available;
    std::mutex work_mutex;
    std::queue<WorkItem> work;

public:
    void push_work(WorkItem item)
    {
        std::unique_lock<std::mutex> lock(work_mutex);

        bool was_empty = work.empty();
        work.push(item);

        lock.unlock();

        if (was_empty)
        {
            work_available.notify_one();
        }
    }

    std::optional<WorkItem> wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(work_mutex);
        while (work.empty())
        {
            const std::chrono::milliseconds timeout(3000);
            if (work_available.wait_for(lock, timeout) == std::cv_status::timeout) {
                return std::nullopt;
            }
        }

        WorkItem tmp = std::move(work.front());
        work.pop();
        return std::make_optional<WorkItem>(tmp);
    }
};
