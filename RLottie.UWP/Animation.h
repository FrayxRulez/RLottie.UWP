#pragma once

#include "rlottie.h"

using namespace Platform;
using namespace Windows::Foundation;

namespace RLottie
{
    public ref class Animation sealed
    {
    public:
		static Animation^ LoadFromData(String^ jsonData);
		static Animation^ LoadFromFile(String^ filePath);

		Array<byte>^ RenderSync(int frameNo);

		property double Duration {
			double get() {
				return m_animation->duration();
			}
		}

		property double FrameRate {
			double get() {
				return m_animation->frameRate();
			}
		}

		property int TotalFrame {
			int get() {
				return m_animation->totalFrame();
			}
		}

		property Windows::Foundation::Size Size {
			Windows::Foundation::Size get() {
				size_t width;
				size_t height;
				m_animation->size(width, height);

				return Windows::Foundation::Size(width, height);
			}
		}

	private:
		Animation(std::string jsonData);

		std::unique_ptr<rlottie::Animation> m_animation;
    };
}
