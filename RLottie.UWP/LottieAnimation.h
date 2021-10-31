#pragma once

#include "LottieAnimation.g.h"

#include "rlottie.h"
#include "WorkQueue.h"

#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

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

namespace winrt::RLottie::implementation
{
	struct LottieAnimation : LottieAnimationT<LottieAnimation>
	{
	public:
		//static CanvasBitmap LottieAnimation::CreateBitmap(ICanvasResourceCreator resourceCreator, int w, int h);

		static RLottie::LottieAnimation LoadFromFile(winrt::hstring filePath, SizeInt32 size, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement);
		static RLottie::LottieAnimation LoadFromData(winrt::hstring jsonData, SizeInt32 size, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement);

		LottieAnimation() = default;

		virtual ~LottieAnimation() {
			Close();
		}

		void Close() {
			slim_lock_guard const guard(m_lock);

			if (m_compressWorker.joinable()) {
				m_compressWorker.join();
			}

			if (m_decompressBuffer != nullptr) {
				delete[] m_decompressBuffer;
				m_decompressBuffer = nullptr;
			}
		}

		void RenderSync(WriteableBitmap bitmap, int32_t frame);
		void RenderSync(CanvasBitmap bitmap, int32_t frame);

		double FrameRate();

		int32_t TotalFrame();

		winrt::Windows::Foundation::Size Size();

	private:
		bool LoadLottieAnimation();
		void RenderSync(std::shared_ptr<uint8_t[]> pixels, size_t w, size_t h, int32_t frame);

		void CompressThreadProc();

		bool m_compressStarted;
		std::thread m_compressWorker;
		WorkQueue m_compressQueue;

		winrt::slim_mutex m_lock;
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
	};
}

namespace winrt::RLottie::factory_implementation
{
	struct LottieAnimation : LottieAnimationT<LottieAnimation, implementation::LottieAnimation>
	{
	};
}
