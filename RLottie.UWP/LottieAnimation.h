#pragma once

#include "LottieAnimation.g.h"

#include "rlottie.h"
#include <stack>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <memory>
#include <map>

#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

#define CACHE_QUEUE_SIZE 15

#define CACHED_VERSION 4
#define CACHED_HEADER_SIZE sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t) + sizeof(size_t)
// Header
// uint8_t  -> version
// uint32_t -> maxFrameSize
// uint32_t -> imageSize
// int32_t -> frameRate
// size_t -> totalFrame

using namespace concurrency;
using namespace winrt;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
    class WorkQueue;

    struct LottieAnimation : LottieAnimationT<LottieAnimation>
    {
    public:
        static RLottie::LottieAnimation LoadFromFile(winrt::hstring filePath, int32_t pixelWidth, int32_t pixelHeight, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier = FitzModifier::None);
        static RLottie::LottieAnimation LoadFromData(winrt::hstring jsonData, int32_t pixelWidth, int32_t pixelHeight, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier = FitzModifier::None);

        LottieAnimation() = default;

        virtual ~LottieAnimation()
        {
            Close();
        }

        void Close()
        {
            if (m_decompressBuffer != nullptr)
            {
                delete[] m_decompressBuffer;
                m_decompressBuffer = nullptr;
            }
        }

        void SetColor(Color color)
        {
            m_color = color;
        }

        void RenderSync(IBuffer bitmap, int32_t frame) noexcept;
        void RenderSync(CanvasBitmap bitmap, int32_t frame);
        void RenderSync(hstring filePath, int32_t frame);

        void Cache() noexcept;

        double FrameRate() noexcept;

        int32_t TotalFrame() noexcept;

        int32_t PixelWidth() noexcept;
        int32_t PixelHeight() noexcept;

        bool IsCaching() noexcept;
        bool IsReadyToCache() noexcept;

    private:
        bool LoadLottieAnimation();
        void RenderSync(uint8_t* pixels, int32_t frame, bool* rendered);

        bool ReadHeader(HANDLE precacheFile, bool completed);

        static void CompressThreadProc();

        // Performance-optimized lock management
        static winrt::slim_mutex& GetLockForKey(const std::string& key);

        static std::mutex s_init_mutex;
        static std::map<std::string, std::unique_ptr<winrt::slim_mutex>> s_locks;

        static winrt::slim_mutex s_compressLock;
        static bool s_compressStarted;
        static std::thread s_compressWorker;
        static WorkQueue s_compressQueue;

        bool m_caching = false;
        bool m_readyToCache = false;

        std::unique_ptr<rlottie::Animation> m_animation;
        size_t m_frameCount = 0;
        int32_t m_frameIndex = 0;
        int32_t m_fps = 30;
        bool m_precache = false;
        winrt::hstring m_path;
        std::wstring m_cacheFile;
        std::string m_data;
        std::string m_cacheKey;
        uint8_t* m_decompressBuffer = nullptr;  // Raw pointer for performance
        uint32_t m_maxFrameSize = 0;
        uint32_t m_imageSize = 0;
        std::vector<uint32_t> m_fileOffsets;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> m_colors;
        rlottie::FitzModifier m_modifier = rlottie::FitzModifier::None;
        size_t m_pixelWidth = 0;
        size_t m_pixelHeight = 0;

        Color m_color = {};
    };

    class WorkItem
    {
    public:
        winrt::weak_ref<LottieAnimation> animation;
        size_t w;
        size_t h;

        WorkItem(winrt::weak_ref<LottieAnimation> animation, size_t w, size_t h)
            : animation(animation), w(w), h(h)
        {
        }
    };

    class WorkQueue
    {
        std::condition_variable work_available;
        std::mutex work_mutex;
        std::stack<WorkItem> work;

    public:
        void push_work(WorkItem item)
        {
            std::unique_lock<std::mutex> lock(work_mutex);

            bool was_empty = work.empty();
            work.push(std::move(item));

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
                if (work_available.wait_for(lock, timeout) == std::cv_status::timeout)
                {
                    return std::nullopt;
                }
            }

            WorkItem tmp = std::move(work.top());
            work.pop();
            return std::make_optional<WorkItem>(std::move(tmp));
        }
    };
}

namespace winrt::RLottie::factory_implementation
{
    struct LottieAnimation : LottieAnimationT<LottieAnimation, implementation::LottieAnimation>
    {
    };
}
