#pragma once

#include "rlottie.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Microsoft::Graphics::Canvas;

namespace RLottie
{
	public interface class IAnimation {
		Array<byte>^ RenderSync(int frame, int width, int height);
		CanvasBitmap^ RenderSync(ICanvasResourceCreator^ resourceCreator, int frame, int width, int height);

		property double Duration {
			double get();
		}

		property double FrameRate {
			double get();
		}

		property int TotalFrame {
			int get();
		}

		property Windows::Foundation::Size Size {
			Windows::Foundation::Size get();
		}
	};

	public ref class Animation sealed : public IAnimation
    {
    public:
		static Animation^ LoadFromData(String^ jsonData, String^ key);
		static Animation^ LoadFromFile(String^ filePath);

		virtual Array<byte>^ RenderSync(int frame, int width, int height);
		virtual CanvasBitmap^ RenderSync(ICanvasResourceCreator^ resourceCreator, int frame, int width, int height);

		virtual property double Duration {
			double get() {
				return m_animation->duration();
			}
		}

		virtual property double FrameRate {
			double get() {
				return m_animation->frameRate();
			}
		}

		virtual property int TotalFrame {
			int get() {
				return m_animation->totalFrame();
			}
		}

		virtual property Windows::Foundation::Size Size {
			Windows::Foundation::Size get() {
				size_t width;
				size_t height;
				m_animation->size(width, height);

				return Windows::Foundation::Size(width, height);
			}
		}

	private:
		Animation(std::unique_ptr<rlottie::Animation> * animation);

		std::unique_ptr<rlottie::Animation> m_animation;
    };
}
