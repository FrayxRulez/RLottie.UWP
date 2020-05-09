#include "pch.h"
#include "CachedAnimation.h"
#include "StringUtils.h"
#include "rlottie.h"

#include <string>
#include <minmax.h>

#include <lz4.h>

using namespace RLottie;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Graphics::Canvas;

CachedAnimation::CachedAnimation() {

}

CachedAnimation^ CachedAnimation::LoadFromFile(String^ filePath, bool precache, bool limitFps, IMapView<uint32, uint32>^ colorReplacement) {
	CachedAnimation^ info = ref new CachedAnimation();

	long hash = 0;
	std::vector<std::pair<std::uint32_t, std::uint32_t>> colors;
	if (colorReplacement != nullptr) {
		for (auto&& elem : colorReplacement)
		{
			colors.push_back({ elem->Key, elem->Value });

			hash = ((hash * 20261) + 0x80000000L + elem->Key) % 0x80000000L;
			hash = ((hash * 20261) + 0x80000000L + elem->Value) % 0x80000000L;
		}
	}

	try {
		auto data = DecompressFromFile(filePath);
		auto cache = string_to_unmanaged(filePath);

		if (hash != 0) {
			cache += ".";
			cache += std::to_string(abs(hash));
		}

		info->path = filePath->Data();
		info->animation = rlottie::Animation::loadFromData(data, cache, "", true, colors);
	}
	catch (...) {

	}
	//if (srcString != 0) {
	//	env->ReleaseStringUTFChars(src, srcString);
	//}
	if (info->animation == nullptr) {
		delete info;
		return nullptr;
	}
	info->frameCount = info->animation->totalFrame();
	info->fps = (int)info->animation->frameRate();
	if (info->fps > 60 || info->frameCount > 600 || info->frameCount <= 0) {
		delete info;
		return nullptr;
	}
	info->limitFps = limitFps;
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
				fread(&(info->maxFrameSize), sizeof(uint32_t), 1, precacheFile);
				fread(&(info->imageSize), sizeof(uint32_t), 1, precacheFile);
				info->fileOffset = CACHED_HEADER_SIZE;
			}
			fclose(precacheFile);
		}
	}

	return info;
}

void CachedAnimation::CreateCache(int w, int h) {
	int stride = w * 4;
	CachedAnimation^ info = this;

	FILE* precacheFile = _wfopen(info->cacheFile.c_str(), L"r+b");
	if (precacheFile != nullptr) {
		uint8_t temp;
		size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
		fclose(precacheFile);
		if (read == 1 && temp == CACHED_VERSION) {
			return;
		}
	}

	if (info->nextFrameIsCacheFrame && info->createCache && info->frameCount != 0 && w * 4 == stride /*&& AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0*/) {
		void* pixels = malloc(w * h * 4);
		precacheFile = _wfopen(info->cacheFile.c_str(), L"w+b");
		if (precacheFile != nullptr) {
			fseek(precacheFile, info->fileOffset = CACHED_HEADER_SIZE, SEEK_SET);

			uint32_t size;
			uint32_t firstFrameSize = 0;
			info->maxFrameSize = 0;
			int bound = LZ4_compressBound(w * h * 4);
			uint8_t* compressBuffer = new uint8_t[bound];
			rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
			int framesPerUpdate = info->limitFps ? info->fps < 60 ? 1 : 2 : 1;
			int i = 0;
			for (size_t a = 0; a < info->frameCount; a += framesPerUpdate) {
				info->animation->renderSync(a, surface);
				size = (uint32_t)LZ4_compress_default((const char*)pixels, (char*)compressBuffer, w * h * 4, bound);
				//totalSize += size;
				if (a == 0) {
					firstFrameSize = size;
				}
				info->maxFrameSize = max(info->maxFrameSize, size);
				fwrite(&size, sizeof(uint32_t), 1, precacheFile);
				fwrite(compressBuffer, sizeof(uint8_t), size, precacheFile);
			}
			delete[] compressBuffer;
			fseek(precacheFile, 0, SEEK_SET);
			uint8_t version = CACHED_VERSION;
			info->imageSize = (uint32_t)w * h * 4;
			fwrite(&version, sizeof(uint8_t), 1, precacheFile);
			fwrite(&info->maxFrameSize, sizeof(uint32_t), 1, precacheFile);
			fwrite(&info->imageSize, sizeof(uint32_t), 1, precacheFile);
			fflush(precacheFile);
			//int fd = fileno(precacheFile);
			//fsync(fd);
			info->createCache = false;
			info->fileOffset = CACHED_HEADER_SIZE + sizeof(uint32_t) + firstFrameSize;
			fclose(precacheFile);
		}

		delete[] pixels;
	}
}

Array<byte>^ CachedAnimation::RenderSync(int frame, int w, int h) {
	int stride = w * 4;
	CachedAnimation^ info = this;
	void* pixels = malloc(w * h * 4);
	bool loadedFromCache = false;
	if (info->precache && !info->createCache && w * 4 == stride && info->maxFrameSize <= w * h * 4 && info->imageSize == w * h * 4) {
		FILE* precacheFile = _wfopen(info->cacheFile.c_str(), L"rb");
		if (precacheFile != nullptr) {
			fseek(precacheFile, info->fileOffset, SEEK_SET);
			if (info->decompressBuffer == nullptr) {
				info->decompressBuffer = new uint8_t[info->maxFrameSize];
			}
			uint32_t frameSize;
			fread(&frameSize, sizeof(uint32_t), 1, precacheFile);
			if (frameSize <= info->maxFrameSize) {
				fread(info->decompressBuffer, sizeof(uint8_t), frameSize, precacheFile);
				info->fileOffset += sizeof(uint32_t) + frameSize;
				LZ4_decompress_safe((const char*)info->decompressBuffer, (char*)pixels, frameSize, w * h * 4);
				loadedFromCache = true;
			}
			fclose(precacheFile);
			int framesPerUpdate = info->limitFps ? info->fps < 60 ? 1 : 2 : 1;
			if (info->frameIndex + framesPerUpdate >= info->frameCount) {
				info->fileOffset = CACHED_HEADER_SIZE;
				info->frameIndex = 0;
			}
			else {
				info->frameIndex += framesPerUpdate;
			}
		}
	}

	if (!loadedFromCache) {
		rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
		info->animation->renderSync((size_t)frame, surface);
		info->nextFrameIsCacheFrame = true;
	}

	Array<byte>^ res = ref new Array<byte>((uint8_t*)pixels, w * h * 4);
	delete[] pixels;
	return res;
}

CanvasBitmap^ CachedAnimation::RenderSync(ICanvasResourceCreator^ resourceCreator, int frame, int width, int height) {
	auto bytes = RenderSync(frame, width, height);

	// Would probably make sense to use ABI here, but I have no idea about how to
	auto bitmap = CanvasBitmap::CreateFromBytes(resourceCreator, bytes, width, height, Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized);
	delete[] bytes;

	return bitmap;
}