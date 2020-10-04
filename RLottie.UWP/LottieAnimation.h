#pragma once

#include "LottieAnimation.g.h"

#include "rlottie.h"

#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

#define CACHED_VERSION 3
#define CACHED_HEADER_SIZE sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t)

using namespace winrt;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

namespace winrt::RLottie::implementation
{
	struct LottieAnimation : LottieAnimationT<LottieAnimation>
	{
	public:
		static RLottie::LottieAnimation LoadFromFile(winrt::hstring filePath, bool precache, winrt::Windows::Foundation::Collections::IMapView<uint32_t, uint32_t> colorReplacement);

		LottieAnimation() = default;

		virtual ~LottieAnimation() {
			if (decompressBuffer != nullptr) {
				delete[] decompressBuffer;
				decompressBuffer = nullptr;
			}
		}

		void Close() {
			if (decompressBuffer != nullptr) {
				delete[] decompressBuffer;
				decompressBuffer = nullptr;
			}
		}

		void CreateCache(int32_t width, int32_t height);

		void RenderSync(WriteableBitmap bitmap, int32_t frame);
		void RenderSync(CanvasBitmap bitmap, int32_t frame);

		double Duration();

		double FrameRate();

		int32_t TotalFrame();

		winrt::Windows::Foundation::Size Size();

		bool ShouldCache();

	private:
		//LottieAnimation();

		void FilterColor(float& r, float& g, float& b);

		winrt::Windows::Foundation::Collections::IMapView<uint32_t, uint32_t> m_colorReplacement;

		std::unique_ptr<rlottie::Animation> animation;
		size_t frameCount = 0;
		int32_t frameIndex = 0;
		int32_t fps = 30;
		bool precache = false;
		bool createCache = false;
		std::wstring path;
		std::wstring cacheFile;
		uint8_t* decompressBuffer = nullptr;
		uint32_t maxFrameSize = 0;
		uint32_t imageSize = 0;
		std::vector<uint32_t> fileOffsets;
		bool nextFrameIsCacheFrame = false;
	};
}

namespace winrt::RLottie::factory_implementation
{
	struct LottieAnimation : LottieAnimationT<LottieAnimation, implementation::LottieAnimation>
	{
	};
}
