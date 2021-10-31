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

using namespace winrt;
using namespace winrt::RLottie;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

namespace winrt::RLottie::implementation
{
	std::map<std::string, winrt::slim_mutex> LottieAnimation::s_locks;

	RLottie::LottieAnimation LottieAnimation::LoadFromFile(winrt::hstring filePath, SizeInt32 size, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement)
	{
		auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
		info->m_path = filePath;

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
				if (read == 1 && temp == CACHED_VERSION) {
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

	RLottie::LottieAnimation LottieAnimation::LoadFromData(winrt::hstring jsonData, SizeInt32 size, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement)
	{
		auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
		info->m_data = string_to_unmanaged(jsonData);

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
				if (read == 1 && temp == CACHED_VERSION) {
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

			m_animation = rlottie::Animation::loadFromData(data, std::string(), std::string(), false, m_colors);

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

	void LottieAnimation::RenderSync(WriteableBitmap bitmap, int32_t frame)
	{
		auto w = bitmap.PixelWidth();
		auto h = bitmap.PixelHeight();

		uint8_t* pixels = bitmap.PixelBuffer().data();
		auto unique = std::shared_ptr<uint8_t[]>(pixels, [](uint8_t* p) {});
		RenderSync(unique, w, h, frame);
	}

	void LottieAnimation::RenderSync(CanvasBitmap bitmap, int32_t frame)
	{
		auto size = bitmap.SizeInPixels();
		auto w = size.Width;
		auto h = size.Height;

		uint8_t* pixels = new uint8_t[w * h * 4];
		auto unique = std::shared_ptr<uint8_t[]>(pixels, [](uint8_t* p) { delete[] p; });
		RenderSync(unique, w, h, frame);

		bitmap.SetPixelBytes(winrt::array_view(pixels, w * h * 4));
	}

	void LottieAnimation::RenderSync(std::shared_ptr<uint8_t[]> pixels, size_t w, size_t h, int32_t frame)
	{
		bool loadedFromCache = false;
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
						LZ4_decompress_safe((const char*)m_decompressBuffer, (char*)pixels.get(), frameSize, w * h * 4);
						loadedFromCache = true;
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

		if (!loadedFromCache) {
			if (m_animation == nullptr && !LoadLottieAnimation()) {
				return;
			}

			rlottie::Surface surface((uint32_t*)pixels.get(), w, h, w * 4);
			m_animation->renderSync(frame, surface);

			if (m_precache) {
				m_compressQueue.push_work(WorkItem(frame, w, h, pixels));

				if (!m_compressStarted) {
					if (m_compressWorker.joinable()) {
						m_compressWorker.join();
					}

					m_compressStarted = true;
					m_compressWorker = std::thread(&LottieAnimation::CompressThreadProc, this);
				}
			}
		}
	}

	void LottieAnimation::CompressThreadProc() {
		while (m_compressStarted) {
			auto work = m_compressQueue.wait_and_pop();
			if (work == std::nullopt) {
				m_compressStarted = false;
				return;
			}

			auto w = work->w;
			auto h = work->h;

			slim_lock_guard const guard(s_locks[m_cacheKey]);

			FILE* precacheFile = _wfopen(m_cacheFile.c_str(), L"r+b");
			if (precacheFile != nullptr) {
				fseek(precacheFile, 0, SEEK_END);
				m_fileOffsets[work->frame] = ftell(precacheFile);

				int bound = LZ4_compressBound(w * h * 4);
				uint8_t* compressBuffer = new uint8_t[bound];
				uint32_t size = (uint32_t)LZ4_compress_default((const char*)work->pixels.get(), (char*)compressBuffer, w * h * 4, bound);
				//totalSize += size;

				if (size > m_maxFrameSize && m_decompressBuffer != nullptr) {
					delete[] m_decompressBuffer;
					m_decompressBuffer = nullptr;
				}

				m_maxFrameSize = max(m_maxFrameSize, size);

				fwrite(&size, sizeof(uint32_t), 1, precacheFile);
				fwrite(compressBuffer, sizeof(uint8_t), size, precacheFile);

				delete[] compressBuffer;

				fseek(precacheFile, 0, SEEK_SET);
				uint8_t version = CACHED_VERSION;
				m_imageSize = (uint32_t)w * h * 4;
				fwrite(&version, sizeof(uint8_t), 1, precacheFile);
				fwrite(&m_maxFrameSize, sizeof(uint32_t), 1, precacheFile);
				fwrite(&m_imageSize, sizeof(uint32_t), 1, precacheFile);
				fwrite(&m_fps, sizeof(int32_t), 1, precacheFile);
				fwrite(&m_frameCount, sizeof(size_t), 1, precacheFile);
				fwrite(&m_fileOffsets[0], sizeof(uint32_t), m_frameCount, precacheFile);

				fflush(precacheFile);
				fclose(precacheFile);
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
		m_animation->size(width, height);

		return winrt::Windows::Foundation::Size(width, height);
	}

#pragma endregion

}