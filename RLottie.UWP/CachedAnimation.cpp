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
using namespace Microsoft::Graphics::Canvas;

CachedAnimation::CachedAnimation() {

}

CachedAnimation^ CachedAnimation::LoadFromFile(String^ filePath, /*jintArray data,*/ bool precache/*, jintArray colorReplacement*/, bool limitFps) {
	CachedAnimation^ info = ref new CachedAnimation();

	//std::map<int32_t, int32_t> colors;
	//if (colorReplacement != nullptr) {
	//	jint* arr = env->GetIntArrayElements(colorReplacement, 0);
	//	if (arr != nullptr) {
	//		jsize len = env->GetArrayLength(colorReplacement);
	//		for (int32_t a = 0; a < len / 2; a++) {
	//			colors[arr[a * 2]] = arr[a * 2 + 1];
	//		}
	//		env->ReleaseIntArrayElements(colorReplacement, arr, 0);
	//	}
	//}

	auto srcString = string_to_unmanaged(filePath);
	auto data = DecompressFromFile(filePath);
	info->path = srcString;

	try {
		info->animation = rlottie::Animation::loadFromData(data, srcString/*, colors*/);
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
		info->cacheFile += ".cache";
		FILE* precacheFile = fopen(info->cacheFile.c_str(), "r+");
		if (precacheFile == nullptr) {
			info->createCache = true;
		}
		else {
			uint8_t temp;
			size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
			info->createCache = read != 1 || temp == 0;
			if (!info->createCache) {
				fread(&(info->maxFrameSize), sizeof(uint32_t), 1, precacheFile);
				fread(&(info->imageSize), sizeof(uint32_t), 1, precacheFile);
				info->fileOffset = 9;
			}
			fclose(precacheFile);
		}
	}

	return info;
}

void CachedAnimation::CreateCache(int w, int h) {
	int stride = w * 4;
	CachedAnimation^ info = this;

	FILE* precacheFile = fopen(info->cacheFile.c_str(), "r+");
	if (precacheFile != nullptr) {
		uint8_t temp;
		size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
		fclose(precacheFile);
		if (read == 1 && temp != 0) {
			return;
		}
	}

	if (info->nextFrameIsCacheFrame && info->createCache && info->frameCount != 0 && w * 4 == stride /*&& AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0*/) {
		void* pixels;
		pixels = static_cast<std::uint32_t*> (malloc(w * h * 4));
		precacheFile = fopen(info->cacheFile.c_str(), "w+");
		if (precacheFile != nullptr) {
			fseek(precacheFile, info->fileOffset = 9, SEEK_SET);

			uint32_t size;
			uint32_t firstFrameSize = 0;
			info->maxFrameSize = 0;
			int bound = LZ4_compressBound(w * h * 4);
			uint8_t* compressBuffer = new uint8_t[bound];
			rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
			//int64_t time = ConnectionsManager::getInstance(0).getCurrentTimeMillis();
			//int totalSize = 0;
			int framesPerUpdate = info->limitFps ? info->fps < 60 ? 1 : 2 : 1;
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
			//DEBUG_D("total size %s = %d, time = %lld ms", info->path.c_str(), totalSize, (ConnectionsManager::getInstance(0).getCurrentTimeMillis() - time));
			fseek(precacheFile, 0, SEEK_SET);
			uint8_t byte = 1;
			info->imageSize = (uint32_t)w * h * 4;
			fwrite(&byte, sizeof(uint8_t), 1, precacheFile);
			fwrite(&info->maxFrameSize, sizeof(uint32_t), 1, precacheFile);
			fwrite(&info->imageSize, sizeof(uint32_t), 1, precacheFile);
			fflush(precacheFile);
			//int fd = fileno(precacheFile);
			//fsync(fd);
			fflush(precacheFile);
			info->createCache = false;
			info->fileOffset = 9 + sizeof(uint32_t) + firstFrameSize;
			fclose(precacheFile);
		}
		//AndroidBitmap_unlockPixels(env, bitmap);
		delete[] pixels;
	}
}

Array<byte>^ CachedAnimation::RenderSync(int frame, int w, int h) {
	int stride = w * 4;
	CachedAnimation^ info = this;
	void* pixels;
	pixels = static_cast<std::uint32_t*> (malloc(w * h * 4));
	//if (AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0) {
	bool loadedFromCache = false;
	if (info->precache && !info->createCache && w * 4 == stride && info->maxFrameSize <= w * h * 4 && info->imageSize == w * h * 4) {
		FILE* precacheFile = fopen(info->cacheFile.c_str(), "r");
		if (precacheFile != nullptr) {
			fseek(precacheFile, info->fileOffset, SEEK_SET);
			if (info->decompressBuffer == nullptr) {
				info->decompressBuffer = new uint8_t[info->maxFrameSize];
			}
			uint32_t frameSize;
			fread(&frameSize, sizeof(uint32_t), 1, precacheFile);
			if (frameSize <= info->maxFrameSize) {
				fread(info->decompressBuffer, sizeof(uint8_t), frameSize, precacheFile);
				info->fileOffset += 4 + frameSize;
				LZ4_decompress_safe((const char*)info->decompressBuffer, (char*)pixels, frameSize, w * h * 4);
				loadedFromCache = true;
			}
			fclose(precacheFile);
			int framesPerUpdate = info->limitFps ? info->fps < 60 ? 1 : 2 : 1;
			if (frame + framesPerUpdate >= info->frameCount) {
				info->fileOffset = 9;
			}
		}
	}
	if (!loadedFromCache) {
		rlottie::Surface surface((uint32_t*)pixels, (size_t)w, (size_t)h, (size_t)stride);
		info->animation->renderSync((size_t)frame, surface);
		info->nextFrameIsCacheFrame = true;
	}

	//	AndroidBitmap_unlockPixels(env, bitmap);
	//}
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