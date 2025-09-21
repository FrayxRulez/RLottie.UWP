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
#include <algorithm>
#include <lz4.h>
#include <wincodec.h>

// divide by 255 and round to nearest
// apply a fast variant: (X+127)/255 = ((X+127)*257+257)>>16 = ((X+128)*257)>>16
#define FAST_DIV255(x) ((((x)+128) * 257) >> 16)

#define RETURNFALSE(x) if (!x) return false;

// Conservative limits - only prevent obvious attacks
constexpr size_t MAX_FRAME_COUNT = 2000;
constexpr size_t MAX_FRAME_SIZE = 64 * 1024 * 1024; // 64MB
constexpr int MAX_FPS = 200;

using namespace winrt;
using namespace winrt::RLottie;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
    // Thread-safe initialization of locks map
    std::mutex LottieAnimation::s_init_mutex;
    std::map<std::string, std::unique_ptr<winrt::slim_mutex>> LottieAnimation::s_locks;

    winrt::slim_mutex LottieAnimation::s_compressLock;
    bool LottieAnimation::s_compressStarted = false;
    std::thread LottieAnimation::s_compressWorker;
    WorkQueue LottieAnimation::s_compressQueue;

    // Fast lock access after initialization
    winrt::slim_mutex& LottieAnimation::GetLockForKey(const std::string& key)
    {
        std::lock_guard<std::mutex> guard(s_init_mutex);
        auto it = s_locks.find(key);
        if (it != s_locks.end())
        {
            return *it->second;
        }

        // Create new lock
        s_locks[key] = std::make_unique<winrt::slim_mutex>();
        return *s_locks[key];
    }

    inline bool ReadFileReturn(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead)
    {
        return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL) &&
            nNumberOfBytesToRead == *lpNumberOfBytesRead;
    }

    bool LottieAnimation::ReadHeader(HANDLE precacheFile, bool completed)
    {
        DWORD read;
        uint8_t version = 0;

        RETURNFALSE(ReadFileReturn(precacheFile, &version, sizeof(uint8_t), &read));

        bool validVersion = completed ?
            (version == CACHED_VERSION + 1) :
            (version == CACHED_VERSION || version == CACHED_VERSION + 1);

        if (!validVersion)
        {
            return false;
        }

        RETURNFALSE(ReadFileReturn(precacheFile, &m_maxFrameSize, sizeof(uint32_t), &read));
        RETURNFALSE(ReadFileReturn(precacheFile, &m_imageSize, sizeof(uint32_t), &read));
        RETURNFALSE(ReadFileReturn(precacheFile, &m_fps, sizeof(int32_t), &read));
        RETURNFALSE(ReadFileReturn(precacheFile, &m_frameCount, sizeof(size_t), &read));

        // Basic validation - only check obvious corruption
        if (m_frameCount == 0 || m_frameCount > MAX_FRAME_COUNT ||
            m_fps <= 0 || m_fps > MAX_FPS ||
            m_maxFrameSize > MAX_FRAME_SIZE)
        {
            return false;
        }

        m_fileOffsets.resize(m_frameCount);
        RETURNFALSE(ReadFileReturn(precacheFile, m_fileOffsets.data(),
            sizeof(uint32_t) * m_frameCount, &read));

        return true;
    }

    RLottie::LottieAnimation LottieAnimation::LoadFromFile(
        winrt::hstring filePath, int32_t pixelWidth, int32_t pixelHeight, bool precache,
        winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement,
        FitzModifier modifier)
    {
        auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();

        // Basic dimension validation
        if (pixelWidth <= 0 || pixelHeight <= 0)
        {
            return nullptr;
        }

        info->m_path = filePath;
        info->m_pixelWidth = pixelWidth;
        info->m_pixelHeight = pixelHeight;
        info->m_modifier = static_cast<rlottie::FitzModifier>(modifier);

        // Build color replacement vector and hash
        long hash = 0;
        if (colorReplacement != nullptr)
        {
            for (auto&& elem : colorReplacement)
            {
                info->m_colors.push_back({ static_cast<uint32_t>(elem.Key()), static_cast<uint32_t>(elem.Value()) });

                hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
                hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
            }
        }

        info->m_precache = precache;

        if (info->m_precache)
        {
            // Build cache key efficiently
            info->m_cacheFile = info->m_path;
            info->m_cacheKey = winrt::to_string(info->m_path);

            if (hash != 0)
            {
                auto hashStr = std::to_string(abs(hash));
                info->m_cacheFile += L"." + winrt::to_hstring(hashStr);
                info->m_cacheKey += "." + hashStr;
            }

            if (modifier != FitzModifier::None)
            {
                auto modStr = std::to_string(static_cast<int>(modifier));
                info->m_cacheFile += L"." + winrt::to_hstring(modStr);
                info->m_cacheKey += "." + modStr;
            }

            auto dimStr = std::to_string(pixelWidth) + "x" + std::to_string(pixelHeight);
            info->m_cacheFile += L"." + winrt::to_hstring(dimStr) + L".cache";
            info->m_cacheKey += "." + dimStr;

            bool createCache = true;
            winrt::slim_lock_guard const guard(GetLockForKey(info->m_cacheKey));

            HANDLE precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
            if (precacheFile != INVALID_HANDLE_VALUE)
            {
                bool headerValid = info->ReadHeader(precacheFile, false);
                CloseHandle(precacheFile);

                if (headerValid)
                {
                    createCache = false;
                }
            }

            if (createCache)
            {
                if (info->m_animation == nullptr && !info->LoadLottieAnimation())
                {
                    return nullptr;
                }

                // Basic validation
                if (info->m_fps > MAX_FPS || info->m_frameCount > MAX_FRAME_COUNT || info->m_frameCount <= 0)
                {
                    return nullptr;
                }

                precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
                if (precacheFile != INVALID_HANDLE_VALUE)
                {
                    DWORD write;
                    uint8_t version = CACHED_VERSION;
                    WriteFile(precacheFile, &version, sizeof(uint8_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_maxFrameSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_imageSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_fps, sizeof(int32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_frameCount, sizeof(size_t), &write, NULL);
                    if (!info->m_fileOffsets.empty())
                    {
                        WriteFile(precacheFile, info->m_fileOffsets.data(), sizeof(uint32_t) * info->m_frameCount, &write, NULL);
                    }

                    CloseHandle(precacheFile);
                }
            }
        }
        else
        {
            if (!info->LoadLottieAnimation())
            {
                return nullptr;
            }
        }

        return info.as<RLottie::LottieAnimation>();
    }

    RLottie::LottieAnimation LottieAnimation::LoadFromData(
        winrt::hstring jsonData, int32_t pixelWidth, int32_t pixelHeight, winrt::hstring cacheKey, bool precache,
        winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement,
        FitzModifier modifier)
    {
        auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();

        // Basic validation
        if (pixelWidth <= 0 || pixelHeight <= 0 || jsonData.empty())
        {
            return nullptr;
        }

        info->m_data = winrt::to_string(jsonData);
        info->m_pixelWidth = pixelWidth;
        info->m_pixelHeight = pixelHeight;
        info->m_modifier = static_cast<rlottie::FitzModifier>(modifier);

        // Build color replacement vector and hash
        long hash = 0;
        if (colorReplacement != nullptr)
        {
            for (auto&& elem : colorReplacement)
            {
                info->m_colors.push_back({ static_cast<uint32_t>(elem.Key()), static_cast<uint32_t>(elem.Value()) });

                hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
                hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
            }
        }

        info->m_precache = precache;

        if (info->m_precache)
        {
            info->m_cacheKey = winrt::to_string(cacheKey);

            if (hash != 0)
            {
                info->m_cacheKey += "." + std::to_string(abs(hash));
            }

            if (modifier != FitzModifier::None)
            {
                info->m_cacheKey += "." + std::to_string(static_cast<int>(modifier));
            }

            info->m_cacheKey += "." + std::to_string(pixelWidth) + "x" + std::to_string(pixelHeight);

            bool createCache = true;
            winrt::slim_lock_guard const guard(GetLockForKey(info->m_cacheKey));

            auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
            info->m_cacheFile = localFolder + L"\\" + cacheKey + L".cache";

            HANDLE precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
            if (precacheFile != INVALID_HANDLE_VALUE)
            {
                bool headerValid = info->ReadHeader(precacheFile, false);
                CloseHandle(precacheFile);

                if (headerValid)
                {
                    createCache = false;
                }
            }

            if (createCache)
            {
                if (info->m_animation == nullptr && !info->LoadLottieAnimation())
                {
                    return nullptr;
                }

                // Basic validation
                if (info->m_fps > MAX_FPS || info->m_frameCount > MAX_FRAME_COUNT || info->m_frameCount <= 0)
                {
                    return nullptr;
                }

                precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
                if (precacheFile != INVALID_HANDLE_VALUE)
                {
                    uint8_t version = CACHED_VERSION;
                    DWORD write;
                    WriteFile(precacheFile, &version, sizeof(uint8_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_maxFrameSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_imageSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_fps, sizeof(int32_t), &write, NULL);
                    WriteFile(precacheFile, &info->m_frameCount, sizeof(size_t), &write, NULL);
                    if (!info->m_fileOffsets.empty())
                    {
                        WriteFile(precacheFile, info->m_fileOffsets.data(), sizeof(uint32_t) * info->m_frameCount, &write, NULL);
                    }

                    CloseHandle(precacheFile);
                }
            }
        }
        else
        {
            if (!info->LoadLottieAnimation())
            {
                return nullptr;
            }
        }

        return info.as<RLottie::LottieAnimation>();
    }

    bool LottieAnimation::LoadLottieAnimation()
    {
        auto data = m_data;

        if (data.empty() && !m_path.empty())
        {
            data = DecompressFromFile(m_path);
        }

        if (data.empty())
        {
            return false;
        }

        // No try/catch in performance path - let exceptions propagate
        m_animation = rlottie::Animation::loadFromData(data, std::string(), std::string(), false, m_colors, m_modifier);

        if (m_animation != nullptr)
        {
            m_frameCount = m_animation->totalFrame();
            m_fps = static_cast<int32_t>(m_animation->frameRate());

            m_fileOffsets.resize(m_frameCount);
            m_imageSize = static_cast<uint32_t>(m_pixelWidth) * m_pixelHeight * 4;
        }

        // Basic validation
        if (m_fps > MAX_FPS || m_frameCount > MAX_FRAME_COUNT || m_frameCount <= 0)
        {
            return false;
        }

        return m_animation != nullptr;
    }

    void LottieAnimation::RenderSync(hstring filePath, int32_t frame)
    {
        int32_t w = m_pixelWidth;
        int32_t h = m_pixelHeight;

        // Basic overflow check
        if (static_cast<uint64_t>(w) * h > MAX_FRAME_SIZE / 4)
        {
            return;
        }

        uint8_t* pixels = new uint8_t[w * h * 4];
        RenderSync(pixels, frame, nullptr);

        // Simple COM resource management
        IWICImagingFactory2* piFactory = nullptr;
        IWICBitmapEncoder* piEncoder = nullptr;
        IWICBitmapFrameEncode* piBitmapFrame = nullptr;
        IWICStream* piStream = nullptr;

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&piFactory));

        if (SUCCEEDED(hr))
        {
            hr = piFactory->CreateStream(&piStream);
        }

        if (SUCCEEDED(hr))
        {
            hr = piStream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
        }

        if (SUCCEEDED(hr))
        {
            hr = piFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &piEncoder);
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
            UINT cbStride = (w * 32 + 7) / 8;
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

        // Cleanup
        if (piFactory) piFactory->Release();
        if (piEncoder) piEncoder->Release();
        if (piBitmapFrame) piBitmapFrame->Release();
        if (piStream) piStream->Release();

        delete[] pixels;
    }

    void LottieAnimation::RenderSync(IBuffer bitmap, int32_t frame) noexcept
    {
        if (bitmap && bitmap.data())
        {
            RenderSync(bitmap.data(), frame, nullptr);
        }
    }

    void LottieAnimation::RenderSync(CanvasBitmap bitmap, int32_t frame)
    {
        auto size = bitmap.SizeInPixels();
        auto w = size.Width;
        auto h = size.Height;

        uint8_t* pixels = new uint8_t[w * h * 4];
        bool rendered;
        RenderSync(pixels, frame, &rendered);

        if (rendered)
        {
            bitmap.SetPixelBytes(winrt::array_view(pixels, w * h * 4));
        }

        delete[] pixels;
    }

    void LottieAnimation::RenderSync(uint8_t* pixels, int32_t frame, bool* rendered)
    {
        // Fast early exits
        if (!pixels)
        {
            return;
        }

        if (rendered)
        {
            *rendered = false;
        }

        if (m_readyToCache)
        {
            return;
        }

        bool loadedFromCache = false;

        // Optimized cache path - minimize branches in hot path
        if (m_precache && m_maxFrameSize <= static_cast<uint32_t>(m_pixelWidth) * m_pixelHeight * 4 &&
            m_imageSize == static_cast<uint32_t>(m_pixelWidth) * m_pixelHeight * 4) [[likely]]
        {
            if (frame >= 0 && static_cast<size_t>(frame) < m_fileOffsets.size()) [[likely]]
            {
                uint32_t offset = m_fileOffsets[frame];
                if (offset > 0) [[likely]]
                {
                    winrt::slim_lock_guard const guard(GetLockForKey(m_cacheKey));

                    HANDLE precacheFile = CreateFile2(m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
                    if (precacheFile != INVALID_HANDLE_VALUE)
                    {
                        if (SetFilePointer(precacheFile, offset, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
                        {
                            // Lazy buffer allocation
                            if (m_decompressBuffer == nullptr && m_maxFrameSize > 0)
                            {
                                m_decompressBuffer = new uint8_t[m_maxFrameSize];
                            }

                            if (m_decompressBuffer)
                            {
                                uint32_t frameSize;
                                DWORD read;
                                if (ReadFileReturn(precacheFile, &frameSize, sizeof(uint32_t), &read) &&
                                    frameSize <= m_maxFrameSize && frameSize > 0)
                                {
                                    if (ReadFileReturn(precacheFile, m_decompressBuffer, frameSize, &read))
                                    {
                                        // CRITICAL: Validate LZ4 result
                                        int result = LZ4_decompress_safe(
                                            (const char*)m_decompressBuffer,
                                            (char*)pixels,
                                            frameSize,
                                            m_pixelWidth * m_pixelHeight * 4);

                                        if (result > 0) [[likely]]
                                        {
                                            loadedFromCache = true;
                                            if (rendered)
                                            {
                                                *rendered = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        CloseHandle(precacheFile);
                    }
                }
            }
        }

        // Direct rendering fallback
        if (!loadedFromCache && !m_caching) [[likely]]
        {
            if (m_animation == nullptr && !LoadLottieAnimation())
            {
                return;
            }

            // No exception handling in hot path - let it crash if needed for performance
            rlottie::Surface surface(reinterpret_cast<uint32_t*>(pixels), m_pixelWidth, m_pixelHeight, m_pixelWidth * 4);
            m_animation->renderSync(frame, surface);

            if (rendered)
            {
                *rendered = true;
            }

            if (m_precache)
            {
                m_readyToCache = true;
            }
        }

        // Apply color overlay if specified - optimize for common case (no overlay)
        if (m_color.A != 0x00) [[unlikely]]
        {
            size_t pixelCount = static_cast<size_t>(m_pixelWidth) * m_pixelHeight;
            for (size_t i = 0; i < pixelCount; ++i)
            {
                size_t idx = i * 4;
                uint8_t alpha = pixels[idx + 3];
                if (alpha != 0x00)
                {
                    pixels[idx + 0] = FAST_DIV255(m_color.B * alpha);
                    pixels[idx + 1] = FAST_DIV255(m_color.G * alpha);
                    pixels[idx + 2] = FAST_DIV255(m_color.R * alpha);
                }
            }
        }
    }

    void LottieAnimation::Cache() noexcept
    {
        if (m_animation == nullptr && !LoadLottieAnimation())
        {
            return;
        }

        if (m_precache)
        {
            m_readyToCache = false;
            m_caching = true;
            s_compressQueue.push_work(WorkItem(get_weak(), m_pixelWidth, m_pixelHeight));

            winrt::slim_lock_guard const guard(s_compressLock);

            if (!s_compressStarted)
            {
                if (s_compressWorker.joinable())
                {
                    s_compressWorker.join();
                }

                s_compressStarted = true;
                s_compressWorker = std::thread(&LottieAnimation::CompressThreadProc);
            }
        }
    }

    void LottieAnimation::CompressThreadProc()
    {
        // CRITICAL FIX: Thread-local static buffers to prevent memory leaks
        static thread_local uint8_t* compressBuffer = nullptr;
        static thread_local uint8_t* pixels = nullptr;
        static thread_local size_t currentCompressBound = 0;
        static thread_local size_t currentPixelSize = 0;

        while (s_compressStarted)
        {
            auto work = s_compressQueue.wait_and_pop();
            if (work == std::nullopt)
            {
                s_compressStarted = false;
                // Cleanup on exit
                delete[] compressBuffer;
                delete[] pixels;
                compressBuffer = nullptr;
                pixels = nullptr;
                return;
            }

            if (auto item{ work->animation.get() })
            {
                auto w = work->w;
                auto h = work->h;

                // Basic overflow check
                if (w <= 0 || h <= 0 || static_cast<uint64_t>(w) * h > MAX_FRAME_SIZE / 4)
                {
                    item->m_caching = false;
                    continue;
                }

                size_t imageSize = static_cast<size_t>(w) * h * 4;
                size_t neededBound = LZ4_compressBound(static_cast<int>(imageSize));

                // Efficient buffer reallocation
                if (neededBound > currentCompressBound)
                {
                    delete[] compressBuffer;
                    compressBuffer = new uint8_t[neededBound];
                    currentCompressBound = neededBound;
                }

                if (imageSize > currentPixelSize)
                {
                    delete[] pixels;
                    pixels = new uint8_t[imageSize];
                    currentPixelSize = imageSize;
                }

                winrt::slim_lock_guard const guard(GetLockForKey(item->m_cacheKey));

                HANDLE precacheFile = CreateFile2(item->m_cacheFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, NULL);
                if (precacheFile != INVALID_HANDLE_VALUE)
                {
                    // Quick check if already cached
                    if (item->ReadHeader(precacheFile, true))
                    {
                        CloseHandle(precacheFile);
                        item->m_caching = false;
                        continue;
                    }

                    DWORD write;
                    DWORD totalSize = SetFilePointer(precacheFile, 0, NULL, FILE_END);
                    if (totalSize == INVALID_SET_FILE_POINTER)
                    {
                        CloseHandle(precacheFile);
                        item->m_caching = false;
                        continue;
                    }

                    uint32_t maxFrameSize = 0;

                    // Cache all frames
                    for (size_t i = 0; i < item->m_frameCount && i < MAX_FRAME_COUNT; ++i)
                    {
                        item->m_fileOffsets[i] = totalSize;

                        // No exception handling - let it fail fast
                        rlottie::Surface surface(reinterpret_cast<uint32_t*>(pixels), w, h, w * 4);
                        item->m_animation->renderSync(static_cast<int>(i), surface);

                        int compressedSize = LZ4_compress_default(
                            (const char*)pixels,
                            (char*)compressBuffer,
                            static_cast<int>(imageSize),
                            static_cast<int>(neededBound));

                        if (compressedSize <= 0)
                        {
                            break;
                        }

                        uint32_t frameSize = static_cast<uint32_t>(compressedSize);
                        maxFrameSize = max(maxFrameSize, frameSize);

                        if (!WriteFile(precacheFile, &frameSize, sizeof(uint32_t), &write, NULL) ||
                            !WriteFile(precacheFile, compressBuffer, frameSize, &write, NULL))
                        {
                            break;
                        }

                        totalSize += frameSize + sizeof(uint32_t);
                    }

                    // Write header information
                    if (SetFilePointer(precacheFile, 0, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
                    {
                        uint8_t version = CACHED_VERSION + 1;
                        item->m_imageSize = static_cast<uint32_t>(imageSize);
                        item->m_maxFrameSize = maxFrameSize;

                        WriteFile(precacheFile, &version, sizeof(uint8_t), &write, NULL);
                        WriteFile(precacheFile, &item->m_maxFrameSize, sizeof(uint32_t), &write, NULL);
                        WriteFile(precacheFile, &item->m_imageSize, sizeof(uint32_t), &write, NULL);
                        WriteFile(precacheFile, &item->m_fps, sizeof(int32_t), &write, NULL);
                        WriteFile(precacheFile, &item->m_frameCount, sizeof(size_t), &write, NULL);
                        if (!item->m_fileOffsets.empty())
                        {
                            WriteFile(precacheFile, item->m_fileOffsets.data(), sizeof(uint32_t) * item->m_frameCount, &write, NULL);
                        }
                    }

                    CloseHandle(precacheFile);
                }

                item->m_caching = false;
            }
        }
    }

#pragma region Properties

    double LottieAnimation::FrameRate() noexcept
    {
        return m_fps;
    }

    int32_t LottieAnimation::TotalFrame() noexcept
    {
        return static_cast<int32_t>(m_frameCount);
    }

    int32_t LottieAnimation::PixelWidth() noexcept
    {
        return m_pixelWidth;
    }

    int32_t LottieAnimation::PixelHeight() noexcept
    {
        return m_pixelHeight;
    }

    bool LottieAnimation::IsCaching() noexcept
    {
        return m_caching;
    }

    bool LottieAnimation::IsReadyToCache() noexcept
    {
        return m_readyToCache;
    }

#pragma endregion

}
