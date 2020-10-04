#include "pch.h"
#include "LottieAnimation.h"
//#include "LottieAnimation.g.cpp"

#include "StringUtils.h"
#include "rlottie.h"

#include <wrl/wrappers/corewrappers.h>

#include <string>
#include <minmax.h>

#include <lz4.h>

using namespace winrt;
using namespace winrt::RLottie;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

namespace winrt::RLottie::implementation
{
	RLottie::LottieAnimation LottieAnimation::LoadFromFile(winrt::hstring filePath, bool precache, winrt::Windows::Foundation::Collections::IMapView<uint32_t, uint32_t> colorReplacement)
	{
		auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();

		long hash = 0;
		std::vector<std::pair<std::uint32_t, std::uint32_t>> colors;
		if (colorReplacement != nullptr) {
			for (auto&& elem : colorReplacement)
			{
				colors.push_back({ elem.Key(), elem.Value() });

				hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
				hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
			}
		}

		try {
			auto data = DecompressFromFile(filePath);
			auto cache = string_to_unmanaged(filePath);

			if (hash != 0) {
				cache += ".";
				cache += std::to_string(abs(hash));
			}

			info->path = filePath.data();
			info->animation = rlottie::Animation::loadFromData(data, cache, "", true, colors);
		}
		catch (...) {

		}
		//if (srcString != 0) {
		//	env->ReleaseStringUTFChars(src, srcString);
		//}
		if (info->animation == nullptr) {
			//delete info;
			return nullptr;
		}
		info->frameCount = info->animation->totalFrame();
		info->fps = (int)info->animation->frameRate();
		if (info->fps > 60 || info->frameCount > 600 || info->frameCount <= 0) {
			//delete info;
			return nullptr;
		}
		info->precache = precache;
		if (info->precache) {
			info->cacheFile = info->path;

			if (hash != 0) {
				info->cacheFile += L".";
				info->cacheFile += std::to_wstring(abs(hash));
			}

			info->cacheFile += L".cache";
			FILE* precacheFile = _wfopen(info->cacheFile.c_str(), L"r+b");
			if (precacheFile == nullptr) {
				info->createCache = true;
			}
			else {
				uint8_t temp;
				size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
				info->createCache = read != 1 || temp != CACHED_VERSION;
				if (!info->createCache) {
					info->fileOffsets = std::vector<uint32_t>(info->frameCount, 0);
					fread(&info->maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->imageSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->fileOffsets[0], sizeof(uint32_t), info->frameCount, precacheFile);
				}
				fclose(precacheFile);
			}
		}

		return info.as<RLottie::LottieAnimation>();
	}

	void LottieAnimation::CreateCache(int32_t w, int32_t h)
	{
		int stride = w * 4;

		FILE* precacheFile = _wfopen(this->cacheFile.c_str(), L"r+b");
		if (precacheFile != nullptr) {
			uint8_t temp;
			size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
			fclose(precacheFile);
			if (read == 1 && temp == CACHED_VERSION) {
				return;
			}
		}

		if (this->nextFrameIsCacheFrame && this->createCache && this->frameCount != 0 && w * 4 == stride /*&& AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0*/) {
			void* pixels = malloc(w * h * 4);
			precacheFile = _wfopen(this->cacheFile.c_str(), L"w+b");
			if (precacheFile != nullptr) {
				uint32_t offset = CACHED_HEADER_SIZE + sizeof(uint32_t) * this->frameCount;
				fseek(precacheFile, offset, SEEK_SET);

				uint32_t size;
				uint32_t firstFrameSize = 0;
				this->maxFrameSize = 0;
				int bound = LZ4_compressBound(w * h * 4);
				uint8_t* compressBuffer = new uint8_t[bound];
				rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
				int framesPerUpdate = /*this->limitFps ? this->fps < 60 ? 1 : 2 :*/ 1;
				int i = 0;
				this->fileOffsets = std::vector<uint32_t>(this->frameCount, 0);
				for (size_t a = 0; a < this->frameCount; a += framesPerUpdate) {
					this->animation->renderSync(a, surface);
					size = (uint32_t)LZ4_compress_default((const char*)pixels, (char*)compressBuffer, w * h * 4, bound);
					//totalSize += size;
					if (a == 0) {
						firstFrameSize = size;
					}
					this->maxFrameSize = max(this->maxFrameSize, size);
					this->fileOffsets[a] = offset;
					fwrite(&size, sizeof(uint32_t), 1, precacheFile);
					fwrite(compressBuffer, sizeof(uint8_t), size, precacheFile);
					offset += sizeof(uint32_t) + size;
				}
				delete[] compressBuffer;
				fseek(precacheFile, 0, SEEK_SET);
				uint8_t version = CACHED_VERSION;
				this->imageSize = (uint32_t)w * h * 4;
				fwrite(&version, sizeof(uint8_t), 1, precacheFile);
				fwrite(&this->maxFrameSize, sizeof(uint32_t), 1, precacheFile);
				fwrite(&this->imageSize, sizeof(uint32_t), 1, precacheFile);
				fwrite(&this->fileOffsets[0], sizeof(uint32_t), this->frameCount, precacheFile);

				fflush(precacheFile);
				fclose(precacheFile);
				this->createCache = false;
			}

			free(pixels);
		}
	}

	void LottieAnimation::RenderSync(WriteableBitmap bitmap, int32_t frame)
	{
		auto w = bitmap.PixelWidth();
		auto h = bitmap.PixelHeight();

		int stride = w * 4;
		//void* pixels = malloc(w * h * 4);
		//auto bitmap{ WriteableBitmap(w, h) };
		void* pixels = bitmap.PixelBuffer().data();
		bool loadedFromCache = false;
		if (this->precache && !this->createCache && w * 4 == stride && this->maxFrameSize <= w * h * 4 && this->imageSize == w * h * 4) {
			FILE* precacheFile = _wfopen(this->cacheFile.c_str(), L"rb");
			if (precacheFile != nullptr) {
				fseek(precacheFile, this->fileOffsets[frame], SEEK_SET);
				if (this->decompressBuffer == nullptr) {
					this->decompressBuffer = new uint8_t[this->maxFrameSize];
				}
				uint32_t frameSize;
				fread(&frameSize, sizeof(uint32_t), 1, precacheFile);
				if (frameSize <= this->maxFrameSize) {
					fread(this->decompressBuffer, sizeof(uint8_t), frameSize, precacheFile);
					LZ4_decompress_safe((const char*)this->decompressBuffer, (char*)pixels, frameSize, w * h * 4);
					loadedFromCache = true;
				}
				fclose(precacheFile);
				int framesPerUpdate = /*this->limitFps ? this->fps < 60 ? 1 : 2 :*/ 1;
				if (this->frameIndex + framesPerUpdate >= this->frameCount) {
					this->frameIndex = 0;
				}
				else {
					this->frameIndex += framesPerUpdate;
				}
			}
		}

		if (!loadedFromCache) {
			rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
			this->animation->renderSync((size_t)frame, surface);
			this->nextFrameIsCacheFrame = true;
		}
	}

	void LottieAnimation::RenderSync(CanvasBitmap bitmap, int32_t frame)
	{
		auto size = bitmap.SizeInPixels();
		auto w = size.Width;
		auto h = size.Height;

		int stride = w * 4;
		void* pixels = malloc(w * h * 4);
		bool loadedFromCache = false;
		if (this->precache && !this->createCache && w * 4 == stride && this->maxFrameSize <= w * h * 4 && this->imageSize == w * h * 4) {
			FILE* precacheFile = _wfopen(this->cacheFile.c_str(), L"rb");
			if (precacheFile != nullptr) {
				fseek(precacheFile, this->fileOffsets[frame], SEEK_SET);
				if (this->decompressBuffer == nullptr) {
					this->decompressBuffer = new uint8_t[this->maxFrameSize];
				}
				uint32_t frameSize;
				fread(&frameSize, sizeof(uint32_t), 1, precacheFile);
				if (frameSize <= this->maxFrameSize) {
					fread(this->decompressBuffer, sizeof(uint8_t), frameSize, precacheFile);
					LZ4_decompress_safe((const char*)this->decompressBuffer, (char*)pixels, frameSize, w * h * 4);
					loadedFromCache = true;
				}
				fclose(precacheFile);
				int framesPerUpdate = /*this->limitFps ? this->fps < 60 ? 1 : 2 :*/ 1;
				if (this->frameIndex + framesPerUpdate >= this->frameCount) {
					this->frameIndex = 0;
				}
				else {
					this->frameIndex += framesPerUpdate;
				}
			}
		}

		if (!loadedFromCache) {
			rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
			this->animation->renderSync((size_t)frame, surface);
			this->nextFrameIsCacheFrame = true;
		}

		winrt::array_view<const uint8_t> data((uint8_t*)pixels, (uint8_t*)pixels + w * h * 4);

		bitmap.SetPixelBytes(data);
		free(pixels);
	}

#pragma region Properties

	double LottieAnimation::Duration()
	{
		return animation->duration();
	}

	double LottieAnimation::FrameRate()
	{
		return animation->frameRate();
	}

	int32_t LottieAnimation::TotalFrame()
	{
		return animation->totalFrame();
	}

	winrt::Windows::Foundation::Size LottieAnimation::Size()
	{
		size_t width;
		size_t height;
		animation->size(width, height);

		return winrt::Windows::Foundation::Size(width, height);
	}

	bool LottieAnimation::ShouldCache()
	{
		return createCache;
	}

#pragma endregion

}