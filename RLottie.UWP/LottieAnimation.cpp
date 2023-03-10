#include "pch.h"
#include "LottieAnimation.h"
#if __has_include("LottieAnimation.g.cpp")
#include "LottieAnimation.g.cpp"
#endif

#include "StringUtils.h"
#include "rlottie.h"

#include <winrt/Windows.Storage.h>

#include <wrl/wrappers/corewrappers.h>

#include <string>
#include <minmax.h>

#include <lz4.h>

#include <wincodec.h>

using namespace winrt;
using namespace winrt::RLottie;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
	std::map<std::string, winrt::slim_mutex> LottieAnimation::s_locks;

	winrt::slim_mutex LottieAnimation::s_compressLock;
	bool LottieAnimation::s_compressStarted;
	std::thread LottieAnimation::s_compressWorker;
	WorkQueue LottieAnimation::s_compressQueue;

	RLottie::LottieAnimation LottieAnimation::LoadFromFile(winrt::hstring filePath, SizeInt32 size, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier)
	{
		auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
		info->m_path = filePath;
		info->m_size = size;
		info->m_modifier = (rlottie::FitzModifier)modifier;

		long hash = 0;
		if (colorReplacement != nullptr) {
			for (auto&& elem : colorReplacement)
			{
				info->m_colors.push_back({ elem.Key(), elem.Value() });

				hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
				hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
			}
		}

		info->m_precache = precache;
		if (info->m_precache) {
			info->m_cacheFile = info->m_path;
			info->m_cacheKey = string_to_unmanaged(info->m_path);

			if (hash != 0) {
				info->m_cacheFile += L".";
				info->m_cacheFile += std::to_wstring(abs(hash));

				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string(abs(hash));
			}

			if (modifier != FitzModifier::None) {
				info->m_cacheFile += L".";
				info->m_cacheFile += std::to_wstring((int)modifier);

				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string((int)modifier);
			}

			if (size.Width != 256 || size.Height != 256) {
				info->m_cacheFile += L".";
				info->m_cacheFile += std::to_wstring(size.Width);
				info->m_cacheFile += L"x";
				info->m_cacheFile += std::to_wstring(size.Width);

				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string(size.Width);
				info->m_cacheKey += "x";
				info->m_cacheKey += std::to_string(size.Width);
			}

			bool createCache = true;
			slim_lock_guard const guard(s_locks[info->m_cacheKey]);

			info->m_cacheFile += L".cache";

			FILE* precacheFile = _wfopen(info->m_cacheFile.c_str(), L"r+b");
			if (precacheFile != nullptr) {
				uint8_t temp;
				size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
				if (read == 1 && (temp == CACHED_VERSION || temp == CACHED_VERSION + 1)) {
					fread(&info->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->m_imageSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->m_fps, sizeof(int32_t), 1, precacheFile);
					fread(&info->m_frameCount, sizeof(size_t), 1, precacheFile);
					info->m_fileOffsets = std::vector<uint32_t>(info->m_frameCount, 0);
					fread(&info->m_fileOffsets[0], sizeof(uint32_t), info->m_frameCount, precacheFile);

					createCache = false;
				}

				fclose(precacheFile);
			}

			if (createCache) {
				if (info->m_animation == nullptr && !info->LoadLottieAnimation()) {
					return nullptr;
				}

				if (info->m_fps > 60 || info->m_frameCount > 600 || info->m_frameCount <= 0) {
					return nullptr;
				}

				precacheFile = _wfopen(info->m_cacheFile.c_str(), L"w+b");
				if (precacheFile != nullptr) {
					uint8_t version = CACHED_VERSION;
					fseek(precacheFile, 0, SEEK_SET);
					fwrite(&version, sizeof(uint8_t), 1, precacheFile);
					fwrite(&info->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&info->m_imageSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&info->m_fps, sizeof(int32_t), 1, precacheFile);
					fwrite(&info->m_frameCount, sizeof(size_t), 1, precacheFile);
					fwrite(&info->m_fileOffsets[0], sizeof(uint32_t), info->m_frameCount, precacheFile);

					fflush(precacheFile);
					fclose(precacheFile);
				}
			}
		}
		else {
			info->LoadLottieAnimation();
		}

		return info.as<RLottie::LottieAnimation>();
	}

	RLottie::LottieAnimation LottieAnimation::LoadFromData(winrt::hstring jsonData, SizeInt32 size, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier)
	{
		auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
		info->m_data = string_to_unmanaged(jsonData);
		info->m_size = size;
		info->m_modifier = (rlottie::FitzModifier)modifier;

		long hash = 0;
		if (colorReplacement != nullptr) {
			for (auto&& elem : colorReplacement)
			{
				info->m_colors.push_back({ elem.Key(), elem.Value() });

				hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
				hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
			}
		}

		info->m_precache = precache;
		if (info->m_precache) {
			info->m_cacheKey = string_to_unmanaged(cacheKey);

			if (hash != 0) {
				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string(abs(hash));
			}

			if (modifier != FitzModifier::None) {
				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string((int)modifier);
			}

			if (size.Width != 256 || size.Height != 256) {
				info->m_cacheKey += ".";
				info->m_cacheKey += std::to_string(size.Width);
				info->m_cacheKey += "x";
				info->m_cacheKey += std::to_string(size.Width);
			}

			bool createCache = true;
			slim_lock_guard const guard(s_locks[info->m_cacheKey]);

			info->m_cacheFile = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path() + L"\\" + cacheKey;
			info->m_cacheFile += L".cache";

			FILE* precacheFile = _wfopen(info->m_cacheFile.c_str(), L"r+b");
			if (precacheFile != nullptr) {
				uint8_t temp;
				size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
				if (read == 1 && (temp == CACHED_VERSION || temp == CACHED_VERSION + 1)) {
					fread(&info->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->m_imageSize, sizeof(uint32_t), 1, precacheFile);
					fread(&info->m_fps, sizeof(int32_t), 1, precacheFile);
					fread(&info->m_frameCount, sizeof(size_t), 1, precacheFile);
					info->m_fileOffsets = std::vector<uint32_t>(info->m_frameCount, 0);
					fread(&info->m_fileOffsets[0], sizeof(uint32_t), info->m_frameCount, precacheFile);

					createCache = false;
				}

				fclose(precacheFile);
			}

			if (createCache) {
				if (info->m_animation == nullptr && !info->LoadLottieAnimation()) {
					return nullptr;
				}

				if (info->m_fps > 60 || info->m_frameCount > 600 || info->m_frameCount <= 0) {
					return nullptr;
				}

				precacheFile = _wfopen(info->m_cacheFile.c_str(), L"w+b");
				if (precacheFile != nullptr) {
					uint8_t version = CACHED_VERSION;
					fseek(precacheFile, 0, SEEK_SET);
					fwrite(&version, sizeof(uint8_t), 1, precacheFile);
					fwrite(&info->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&info->m_imageSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&info->m_fps, sizeof(int32_t), 1, precacheFile);
					fwrite(&info->m_frameCount, sizeof(size_t), 1, precacheFile);
					fwrite(&info->m_fileOffsets[0], sizeof(uint32_t), info->m_frameCount, precacheFile);

					fflush(precacheFile);
					fclose(precacheFile);
				}
			}
		}
		else {
			info->LoadLottieAnimation();
		}

		return info.as<RLottie::LottieAnimation>();
	}

	bool LottieAnimation::LoadLottieAnimation() {
		try {
			auto data = m_data;
			auto cache = m_cacheKey;

			if (data.empty() && !m_path.empty()) {
				data = DecompressFromFile(m_path);
			}

			if (data.empty()) {
				return false;
			}

			m_animation = rlottie::Animation::loadFromData(data, std::string(), std::string(), false, m_colors, m_modifier);

			if (m_animation != nullptr) {
				m_frameCount = m_animation->totalFrame();
				m_fps = (int)m_animation->frameRate();
				m_fileOffsets = std::vector<uint32_t>(m_frameCount, 0);
			}

			if (m_fps > 60 || m_frameCount > 600 || m_frameCount <= 0) {
				return false;
			}
		}
		catch (...) {
			return false;
		}

		return m_animation != nullptr;
	}

	void LottieAnimation::RenderSync(hstring filePath, int32_t frame)
	{
		int32_t w = m_size.Width;
		int32_t h = m_size.Height;

		uint8_t* pixels = new uint8_t[w * h * 4];
		RenderSync(pixels, w, h, frame, NULL);

		IWICImagingFactory2* piFactory = NULL;
		IWICBitmapEncoder* piEncoder = NULL;
		IWICBitmapFrameEncode* piBitmapFrame = NULL;
		IWICStream* piStream = NULL;

		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&piFactory));

		if (SUCCEEDED(hr))
		{
			hr = piFactory->CreateStream(&piStream);
		}

		if (SUCCEEDED(hr))
		{
			hr = piStream->InitializeFromFilename(filePath.data(), GENERIC_WRITE);
		}

		if (SUCCEEDED(hr))
		{
			hr = piFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &piEncoder);
		}

		if (SUCCEEDED(hr))
		{
			hr = piEncoder->Initialize(piStream, WICBitmapEncoderNoCache);
		}

		if (SUCCEEDED(hr))
		{
			hr = piEncoder->CreateNewFrame(&piBitmapFrame, nullptr);
		}

		if (SUCCEEDED(hr))
		{
			hr = piBitmapFrame->Initialize(nullptr);
		}

		if (SUCCEEDED(hr))
		{
			hr = piBitmapFrame->SetSize(w, h);
		}

		WICPixelFormatGUID formatGUID = GUID_WICPixelFormat32bppBGRA;
		if (SUCCEEDED(hr))
		{
			hr = piBitmapFrame->SetPixelFormat(&formatGUID);
		}

		if (SUCCEEDED(hr))
		{
			UINT cbStride = (w * 32 + 7) / 8/***WICGetStride***/;
			UINT cbBufferSize = w * h * 4;

			hr = piBitmapFrame->WritePixels(h, cbStride, cbBufferSize, pixels);
		}

		if (SUCCEEDED(hr))
		{
			hr = piBitmapFrame->Commit();
		}

		if (SUCCEEDED(hr))
		{
			hr = piEncoder->Commit();
		}

		if (piFactory)
			piFactory->Release();

		if (piEncoder)
			piEncoder->Release();

		if (piBitmapFrame)
			piBitmapFrame->Release();

		if (piStream)
			piStream->Release();

		delete[] pixels;
	}

	void LottieAnimation::SetBitmap(WriteableBitmap bitmap)
	{
		if (bitmap)
		{
			m_target = bitmap;
			m_bitmap = std::make_unique<uint8_t*>(bitmap.PixelBuffer().data());
			m_bitmapWidth = bitmap.PixelWidth();
			m_bitmapHeight = bitmap.PixelHeight();
		}
		else
		{
			m_target = nullptr;
			m_bitmap = nullptr;
		}
	}

	void LottieAnimation::RenderSync(int32_t frame)
	{
		if (m_bitmap == nullptr) {
			return;
		}

		slim_lock_guard const guard(m_bitmapLock);

		uint8_t* pixels = *m_bitmap.get();
		bool rendered;
		RenderSync(pixels, m_bitmapWidth, m_bitmapHeight, frame, NULL);
	}

	void LottieAnimation::RenderSync(IBuffer bitmap, int32_t width, int32_t height, int32_t frame)
	{
		auto w = width;
		auto h = height;

		uint8_t* pixels = bitmap.data();
		RenderSync(pixels, w, h, frame, NULL);
	}

	void LottieAnimation::RenderSync(WriteableBitmap bitmap, int32_t frame)
	{
		auto w = bitmap.PixelWidth();
		auto h = bitmap.PixelHeight();

		uint8_t* pixels = bitmap.PixelBuffer().data();
		RenderSync(pixels, w, h, frame, NULL);
	}

	void LottieAnimation::RenderSync(CanvasBitmap bitmap, int32_t frame)
	{
		auto size = bitmap.SizeInPixels();
		auto w = size.Width;
		auto h = size.Height;

		uint8_t* pixels = new uint8_t[w * h * 4];
		bool rendered;
		RenderSync(pixels, w, h, frame, &rendered);

		if (rendered) {
			bitmap.SetPixelBytes(winrt::array_view(pixels, w * h * 4));
		}

		delete[] pixels;
	}

	void LottieAnimation::RenderSync(uint8_t* pixels, size_t w, size_t h, int32_t frame, bool* rendered)
	{
		bool loadedFromCache = false;
		if (rendered) {
			*rendered = false;
		}

		if (m_precache && m_maxFrameSize <= w * h * 4 && m_imageSize == w * h * 4) {
			uint32_t offset = m_fileOffsets[frame];
			if (offset > 0) {
				slim_lock_guard const guard(s_locks[m_cacheKey]);

				FILE* precacheFile = _wfopen(m_cacheFile.c_str(), L"rb");
				if (precacheFile != nullptr) {
					fseek(precacheFile, offset, SEEK_SET);
					if (m_decompressBuffer == nullptr) {
						m_decompressBuffer = new uint8_t[m_maxFrameSize];
					}
					uint32_t frameSize;
					fread(&frameSize, sizeof(uint32_t), 1, precacheFile);
					if (frameSize <= m_maxFrameSize) {
						fread(m_decompressBuffer, sizeof(uint8_t), frameSize, precacheFile);
						LZ4_decompress_safe((const char*)m_decompressBuffer, (char*)pixels, frameSize, w * h * 4);
						loadedFromCache = true;

						if (rendered) {
							*rendered = true;
						}
					}
					fclose(precacheFile);
					int framesPerUpdate = /*limitFps ? fps < 60 ? 1 : 2 :*/ 1;
					if (m_frameIndex + framesPerUpdate >= m_frameCount) {
						m_frameIndex = 0;
					}
					else {
						m_frameIndex += framesPerUpdate;
					}
				}
			}
		}

		if (!loadedFromCache && !m_caching) {
			if (m_animation == nullptr && !LoadLottieAnimation()) {
				return;
			}

			rlottie::Surface surface((uint32_t*)pixels, w, h, w * 4);
			m_animation->renderSync(frame, surface);

			if (rendered) {
				*rendered = true;
			}

			if (m_precache) {
				m_caching = true;
				s_compressQueue.push_work(WorkItem(get_weak(), w, h));

				slim_lock_guard const guard(s_compressLock);

				if (!s_compressStarted) {
					if (s_compressWorker.joinable()) {
						s_compressWorker.join();
					}

					s_compressStarted = true;
					s_compressWorker = std::thread(&LottieAnimation::CompressThreadProc);
				}
			}
		}
	}

	void LottieAnimation::CompressThreadProc() {
		while (s_compressStarted) {
			auto work = s_compressQueue.wait_and_pop();
			if (work == std::nullopt) {
				s_compressStarted = false;
				return;
			}

			auto oldW = 0;
			auto oldH = 0;

			int bound;
			uint8_t* compressBuffer = nullptr;
			uint8_t* pixels = nullptr;

			if (auto item{ work->animation.get() }) {
				auto w = work->w;
				auto h = work->h;

				slim_lock_guard const guard(s_locks[item->m_cacheKey]);

				FILE* precacheFile = _wfopen(item->m_cacheFile.c_str(), L"r+b");
				if (precacheFile != nullptr) {
					uint8_t temp;
					size_t read = fread(&temp, sizeof(uint8_t), 1, precacheFile);
					if (read == 1 && temp == CACHED_VERSION + 1) {
						fread(&item->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
						fread(&item->m_imageSize, sizeof(uint32_t), 1, precacheFile);
						fread(&item->m_fps, sizeof(int32_t), 1, precacheFile);
						fread(&item->m_frameCount, sizeof(size_t), 1, precacheFile);
						item->m_fileOffsets = std::vector<uint32_t>(item->m_frameCount, 0);
						fread(&item->m_fileOffsets[0], sizeof(uint32_t), item->m_frameCount, precacheFile);
						fclose(precacheFile);

						item->m_caching = false;
						continue;
					}

					fseek(precacheFile, 0, SEEK_END);
					size_t totalSize = ftell(precacheFile);

					if (w + h > oldW + oldH) {
						bound = LZ4_compressBound(w * h * 4);
						compressBuffer = new uint8_t[bound];
						pixels = new uint8_t[w * h * 4];
					}

					for (int i = 0; i < item->m_frameCount; i++) {
						item->m_fileOffsets[i] = totalSize;

						rlottie::Surface surface((uint32_t*)pixels, w, h, w * 4);
						item->m_animation->renderSync(i, surface);

						uint32_t size = (uint32_t)LZ4_compress_default((const char*)pixels, (char*)compressBuffer, w * h * 4, bound);

						if (size > item->m_maxFrameSize && item->m_decompressBuffer != nullptr) {
							delete[] item->m_decompressBuffer;
							item->m_decompressBuffer = nullptr;
						}

						item->m_maxFrameSize = max(item->m_maxFrameSize, size);

						fwrite(&size, sizeof(uint32_t), 1, precacheFile);
						fwrite(compressBuffer, sizeof(uint8_t), size, precacheFile);
						totalSize += size;
						totalSize += 4;
					}

					fseek(precacheFile, 0, SEEK_SET);
					uint8_t version = CACHED_VERSION + 1;
					item->m_imageSize = (uint32_t)w * h * 4;
					fwrite(&version, sizeof(uint8_t), 1, precacheFile);
					fwrite(&item->m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&item->m_imageSize, sizeof(uint32_t), 1, precacheFile);
					fwrite(&item->m_fps, sizeof(int32_t), 1, precacheFile);
					fwrite(&item->m_frameCount, sizeof(size_t), 1, precacheFile);
					fwrite(&item->m_fileOffsets[0], sizeof(uint32_t), item->m_frameCount, precacheFile);

					fflush(precacheFile);
					fclose(precacheFile);
				}

				item->m_caching = false;

				oldW = w;
				oldH = h;
			}

			if (compressBuffer) {
				delete[] compressBuffer;
			}

			if (pixels) {
				delete[] pixels;
			}
		}
	}

#pragma region Properties

	double LottieAnimation::FrameRate()
	{
		return m_fps;
	}

	int32_t LottieAnimation::TotalFrame()
	{
		return m_frameCount;
	}

	winrt::Windows::Foundation::Size LottieAnimation::Size()
	{
		size_t width;
		size_t height;

		if (m_animation) {
			m_animation->size(width, height);
		}
		else {
			width = 0;
			height = 0;
		}

		return winrt::Windows::Foundation::Size(width, height);
	}

	bool LottieAnimation::IsCaching() {
		return m_caching;
	}

	void LottieAnimation::IsCaching(bool value) {
		m_caching = value;
	}

#pragma endregion

}