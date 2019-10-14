#pragma once

#include "rlottie.h"
#include "Animation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Microsoft::Graphics::Canvas;

namespace RLottie
{
	public ref class CachedAnimation sealed : public IAnimation
	{
	public:
		static CachedAnimation^ LoadFromFile(String^ filePath, bool precache, bool limitFps);

		virtual ~CachedAnimation() {
			if (decompressBuffer != nullptr) {
				delete[] decompressBuffer;
				decompressBuffer = nullptr;
			}
		}

		void CreateCache(int width, int height);

		virtual Array<byte>^ RenderSync(int frame, int width, int height);
		virtual CanvasBitmap^ RenderSync(ICanvasResourceCreator^ resourceCreator, int frame, int width, int height);

		virtual property double Duration {
			double get() {
				return animation->duration();
			}
		}

		virtual property double FrameRate {
			double get() {
				return animation->frameRate();
			}
		}

		virtual property int TotalFrame {
			int get() {
				return animation->totalFrame();
			}
		}

		virtual property Windows::Foundation::Size Size {
			Windows::Foundation::Size get() {
				size_t width;
				size_t height;
				animation->size(width, height);

				return Windows::Foundation::Size(width, height);
			}
		}



		property bool IsCached {
			bool get() {
				return !createCache;
			}
		}

	private:
		CachedAnimation();

		std::unique_ptr<rlottie::Animation> animation;
		size_t frameCount = 0;
		int32_t fps = 30;
		bool precache = false;
		bool createCache = false;
		bool limitFps = false;
		std::string path;
		std::string cacheFile;
		uint8_t* decompressBuffer = nullptr;
		uint32_t maxFrameSize = 0;
		uint32_t imageSize = 0;
		uint32_t fileOffset = 0;
		bool nextFrameIsCacheFrame = false;
	};
}
