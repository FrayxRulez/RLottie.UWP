#pragma once

#include "LottieAnimation.g.h"

#include "rlottie.h"
#include <queue>

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
using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
	class WorkQueue;

	struct LottieAnimation : LottieAnimationT<LottieAnimation>
	{
	public:
		//static CanvasBitmap LottieAnimation::CreateBitmap(ICanvasResourceCreator resourceCreator, int w, int h);

		static RLottie::LottieAnimation LoadFromFile(winrt::hstring filePath, SizeInt32 size, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier = FitzModifier::None);
		static RLottie::LottieAnimation LoadFromData(winrt::hstring jsonData, SizeInt32 size, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier = FitzModifier::None);

		LottieAnimation() = default;

		virtual ~LottieAnimation() {
			Close();
		}

		void Close() {
			if (m_decompressBuffer != nullptr) {
				delete[] m_decompressBuffer;
				m_decompressBuffer = nullptr;
			}
		}

		void RenderSync(IBuffer bitmap, int32_t width, int32_t height, int32_t frame);
		void RenderSync(WriteableBitmap bitmap, int32_t frame);
		void RenderSync(CanvasBitmap bitmap, int32_t frame);
		void RenderSync(hstring filePath, int32_t frame);

		double FrameRate();

		int32_t TotalFrame();

		winrt::Windows::Foundation::Size Size();

		bool IsCaching();
		void IsCaching(bool value);

	private:
		bool LoadLottieAnimation();
		void RenderSync(uint8_t* pixels, size_t w, size_t h, int32_t frame, bool* rendered);

		static void CompressThreadProc();

		static winrt::slim_mutex s_compressLock;
		static bool s_compressStarted;
		static std::thread s_compressWorker;
		static WorkQueue s_compressQueue;

		bool m_caching;

		static std::map<std::string, winrt::slim_mutex> s_locks;

		std::unique_ptr<rlottie::Animation> m_animation;
		size_t m_frameCount = 0;
		int32_t m_frameIndex = 0;
		int32_t m_fps = 30;
		bool m_precache = false;
		winrt::hstring m_path;
		std::wstring m_cacheFile;
		std::string m_data;
		std::string m_cacheKey;
		uint8_t* m_decompressBuffer = nullptr;
		uint32_t m_maxFrameSize = 0;
		uint32_t m_imageSize = 0;
		std::vector<uint32_t> m_fileOffsets;
		std::vector<std::pair<std::uint32_t, std::uint32_t>> m_colors;
		SizeInt32 m_size;
		rlottie::FitzModifier m_modifier;
	};

	class WorkItem {
	public:
		winrt::weak_ref<LottieAnimation> animation;
		size_t w;
		size_t h;

		WorkItem(winrt::weak_ref<LottieAnimation> animation, size_t w, size_t h)
			: animation(animation),
			w(w),
			h(h)
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

			//while (CACHE_QUEUE_SIZE < work.size())
			//{
			//	WorkItem tmp = std::move(work.front());
			//	work.pop();

			//	if (auto animation{ tmp.animation.get() }) {
			//		animation->IsCaching(false);
			//	}
			//}

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
}

namespace winrt::RLottie::factory_implementation
{
	struct LottieAnimation : LottieAnimationT<LottieAnimation, implementation::LottieAnimation>
	{
	};
}
