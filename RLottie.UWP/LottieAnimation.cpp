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

// divide by 255 and round to nearest
// apply a fast variant: (X+127)/255 = ((X+127)*257+257)>>16 = ((X+128)*257)>>16
#define FAST_DIV255(x) ((((x)+128) * 257) >> 16)

#define RETURNFALSE(x) if (!x) return false;

using namespace winrt;
using namespace winrt::RLottie;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
    std::map<std::string, winrt::slim_mutex> LottieAnimation::s_locks;

    winrt::slim_mutex LottieAnimation::s_compressLock;
    bool LottieAnimation::s_compressStarted;
    std::thread LottieAnimation::s_compressWorker;
    WorkQueue LottieAnimation::s_compressQueue;

    inline bool ReadFileReturn(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead)
    {
        if (ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL))
        {
            return nNumberOfBytesToRead == *lpNumberOfBytesRead;
        }

        return false;
    }

    bool LottieAnimation::ReadHeader(HANDLE precacheFile, bool completed)
    {
        DWORD read;
        uint8_t version = 0;
        RETURNFALSE(ReadFileReturn(precacheFile, &version, sizeof(uint8_t), &read));
        if (completed ? version == CACHED_VERSION + 1 : (version == CACHED_VERSION || version == CACHED_VERSION + 1))
        {
            RETURNFALSE(ReadFileReturn(precacheFile, &m_maxFrameSize, sizeof(uint32_t), &read));
            RETURNFALSE(ReadFileReturn(precacheFile, &m_imageSize, sizeof(uint32_t), &read));
            RETURNFALSE(ReadFileReturn(precacheFile, &m_fps, sizeof(int32_t), &read));
            RETURNFALSE(ReadFileReturn(precacheFile, &m_frameCount, sizeof(size_t), &read));
            m_fileOffsets = std::vector<uint32_t>(m_frameCount, 0);
            RETURNFALSE(ReadFileReturn(precacheFile, &m_fileOffsets[0], sizeof(uint32_t) * m_frameCount, &read));

            return true;
        }

        return false;
    }

    RLottie::LottieAnimation LottieAnimation::LoadFromFile(winrt::hstring filePath, int32_t pixelWidth, int32_t pixelHeight, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier)
    {
        auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
        info->m_path = filePath;
        info->m_pixelWidth = pixelWidth;
        info->m_pixelHeight = pixelHeight;
        info->m_modifier = (rlottie::FitzModifier)modifier;

        long hash = 0;
        if (colorReplacement != nullptr)
        {
            for (auto&& elem : colorReplacement)
            {
                info->m_colors.push_back({ elem.Key(), elem.Value() });

                hash = ((hash * 20261) + 0x80000000L + elem.Key()) % 0x80000000L;
                hash = ((hash * 20261) + 0x80000000L + elem.Value()) % 0x80000000L;
            }
        }

        info->m_precache = precache;
        if (info->m_precache)
        {
            info->m_cacheFile = info->m_path;
            info->m_cacheKey = winrt::to_string(info->m_path);

            if (hash != 0)
            {
                info->m_cacheFile += L".";
                info->m_cacheFile += std::to_wstring(abs(hash));

                info->m_cacheKey += ".";
                info->m_cacheKey += std::to_string(abs(hash));
            }

            if (modifier != FitzModifier::None)
            {
                info->m_cacheFile += L".";
                info->m_cacheFile += std::to_wstring((int)modifier);

                info->m_cacheKey += ".";
                info->m_cacheKey += std::to_string((int)modifier);
            }

            info->m_cacheFile += L".";
            info->m_cacheFile += std::to_wstring(pixelWidth);
            info->m_cacheFile += L"x";
            info->m_cacheFile += std::to_wstring(pixelHeight);

            info->m_cacheKey += ".";
            info->m_cacheKey += std::to_string(pixelWidth);
            info->m_cacheKey += "x";
            info->m_cacheKey += std::to_string(pixelHeight);

            bool createCache = true;
            slim_lock_guard const guard(s_locks[info->m_cacheKey]);

            info->m_cacheFile += L".cache";

            HANDLE precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
            if (precacheFile != INVALID_HANDLE_VALUE)
            {
                if (info->ReadHeader(precacheFile, false))
                {
                    createCache = false;
                }

                CloseHandle(precacheFile);
            }

            if (createCache)
            {
                if (info->m_animation == nullptr && !info->LoadLottieAnimation())
                {
                    return nullptr;
                }

                if (info->m_fps > 60 || info->m_frameCount > 600 || info->m_frameCount <= 0)
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
                    WriteFile(precacheFile, &info->m_fileOffsets[0], sizeof(uint32_t) * info->m_frameCount, &write, NULL);

                    CloseHandle(precacheFile);
                }
            }
        }
        else
        {
            info->LoadLottieAnimation();
        }

        return info.as<RLottie::LottieAnimation>();
    }

    RLottie::LottieAnimation LottieAnimation::LoadFromData(winrt::hstring jsonData, int32_t pixelWidth, int32_t pixelHeight, winrt::hstring cacheKey, bool precache, winrt::Windows::Foundation::Collections::IMapView<int32_t, int32_t> colorReplacement, FitzModifier modifier)
    {
        auto info = winrt::make_self<winrt::RLottie::implementation::LottieAnimation>();
        info->m_data = winrt::to_string(jsonData);
        info->m_pixelWidth = pixelWidth;
        info->m_pixelHeight = pixelHeight;
        info->m_modifier = (rlottie::FitzModifier)modifier;

        long hash = 0;
        if (colorReplacement != nullptr)
        {
            for (auto&& elem : colorReplacement)
            {
                info->m_colors.push_back({ elem.Key(), elem.Value() });

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
                info->m_cacheKey += ".";
                info->m_cacheKey += std::to_string(abs(hash));
            }

            if (modifier != FitzModifier::None)
            {
                info->m_cacheKey += ".";
                info->m_cacheKey += std::to_string((int)modifier);
            }

            info->m_cacheKey += ".";
            info->m_cacheKey += std::to_string(pixelWidth);
            info->m_cacheKey += "x";
            info->m_cacheKey += std::to_string(pixelHeight);

            bool createCache = true;
            slim_lock_guard const guard(s_locks[info->m_cacheKey]);

            info->m_cacheFile = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path() + L"\\" + cacheKey;
            info->m_cacheFile += L".cache";

            HANDLE precacheFile = CreateFile2(info->m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
            if (precacheFile != INVALID_HANDLE_VALUE)
            {
                if (info->ReadHeader(precacheFile, false))
                {
                    createCache = false;
                }

                CloseHandle(precacheFile);
            }

            if (createCache)
            {
                if (info->m_animation == nullptr && !info->LoadLottieAnimation())
                {
                    return nullptr;
                }

                if (info->m_fps > 60 || info->m_frameCount > 600 || info->m_frameCount <= 0)
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
                    WriteFile(precacheFile, &info->m_fileOffsets[0], sizeof(uint32_t) * info->m_frameCount, &write, NULL);

                    CloseHandle(precacheFile);
                }
            }
        }
        else
        {
            info->LoadLottieAnimation();
        }

        return info.as<RLottie::LottieAnimation>();
    }

    bool LottieAnimation::LoadLottieAnimation()
    {
        try
        {
            auto data = m_data;
            auto cache = m_cacheKey;

            if (data.empty() && !m_path.empty())
            {
                data = DecompressFromFile(m_path);
            }

            if (data.empty())
            {
                return false;
            }

            m_animation = rlottie::Animation::loadFromData(data, std::string(), std::string(), false, m_colors, m_modifier);

            if (m_animation != nullptr)
            {
                m_frameCount = m_animation->totalFrame();
                m_fps = (int)m_animation->frameRate();
                m_fileOffsets = std::vector<uint32_t>(m_frameCount, 0);
                //m_animation->size(m_pixelWidth, m_pixelHeight);
            }

            if (m_fps > 60 || m_frameCount > 600 || m_frameCount <= 0)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return m_animation != nullptr;
    }

    void LottieAnimation::RenderSync(hstring filePath, int32_t frame)
    {
        int32_t w = m_pixelWidth;
        int32_t h = m_pixelHeight;

        uint8_t* pixels = new uint8_t[w * h * 4];
        RenderSync(pixels, frame, NULL);

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

    void LottieAnimation::RenderSync(IBuffer bitmap, int32_t frame) noexcept
    {
        uint8_t* pixels = bitmap.data();
        RenderSync(pixels, frame, NULL);
    }

    void LottieAnimation::RenderSync(uint8_t* pixels, int32_t frame, bool* rendered)
    {
        bool loadedFromCache = false;
        if (rendered)
        {
            *rendered = false;
        }

        if (m_readyToCache)
        {
            return;
        }

        if (m_precache && m_maxFrameSize <= m_pixelWidth * m_pixelHeight * 4 && m_imageSize == m_pixelWidth * m_pixelHeight * 4)
        {
            uint32_t offset = m_fileOffsets[frame];
            if (offset > 0)
            {
                slim_lock_guard const guard(s_locks[m_cacheKey]);

                HANDLE precacheFile = CreateFile2(m_cacheFile.c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);
                if (precacheFile != INVALID_HANDLE_VALUE)
                {
                    SetFilePointer(precacheFile, offset, NULL, FILE_BEGIN);
                    if (m_decompressBuffer == nullptr)
                    {
                        m_decompressBuffer = new uint8_t[m_maxFrameSize];
                    }
                    uint32_t frameSize;
                    DWORD read;
                    auto completed = ReadFileReturn(precacheFile, &frameSize, sizeof(uint32_t), &read);
                    if (completed && frameSize <= m_maxFrameSize)
                    {
                        if (ReadFileReturn(precacheFile, m_decompressBuffer, sizeof(uint8_t) * frameSize, &read))
                        {
                            LZ4_decompress_safe((const char*)m_decompressBuffer, (char*)pixels, frameSize, m_pixelWidth * m_pixelHeight * 4);
                            loadedFromCache = true;

                            if (rendered)
                            {
                                *rendered = true;
                            }
                        }
                    }
                    CloseHandle(precacheFile);
                    int framesPerUpdate = /*limitFps ? fps < 60 ? 1 : 2 :*/ 1;
                    if (m_frameIndex + framesPerUpdate >= m_frameCount)
                    {
                        m_frameIndex = 0;
                    }
                    else
                    {
                        m_frameIndex += framesPerUpdate;
                    }
                }
            }
        }

        if (!loadedFromCache && !m_caching)
        {
            if (m_animation == nullptr && !LoadLottieAnimation())
            {
                return;
            }

            rlottie::Surface surface((uint32_t*)pixels, m_pixelWidth, m_pixelHeight, m_pixelWidth * 4);
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

        if (m_color.A != 0x00)
        {
            for (int i = 0; i < m_pixelWidth * m_pixelHeight * 4; i += 4)
            {
                uint8_t alpha = pixels[i + 3];
                if (alpha != 0x00)
                {
                    pixels[i + 0] = FAST_DIV255(m_color.B * alpha);
                    pixels[i + 1] = FAST_DIV255(m_color.G * alpha);
                    pixels[i + 2] = FAST_DIV255(m_color.R * alpha);
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

            slim_lock_guard const guard(s_compressLock);

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
        while (s_compressStarted)
        {
            auto work = s_compressQueue.wait_and_pop();
            if (work == std::nullopt)
            {
                s_compressStarted = false;
                return;
            }

            auto oldW = 0;
            auto oldH = 0;

            int bound;
            uint8_t* compressBuffer = nullptr;
            uint8_t* pixels = nullptr;

            if (auto item{ work->animation.get() })
            {
                auto w = work->w;
                auto h = work->h;

                slim_lock_guard const guard(s_locks[item->m_cacheKey]);

                HANDLE precacheFile = CreateFile2(item->m_cacheFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, NULL);
                if (precacheFile != INVALID_HANDLE_VALUE)
                {
                    if (item->ReadHeader(precacheFile, true))
                    {
                        CloseHandle(precacheFile);
                        item->m_caching = false;
                        continue;
                    }

                    DWORD write;
                    size_t totalSize = SetFilePointer(precacheFile, 0, NULL, FILE_END);

                    if (w + h > oldW + oldH)
                    {
                        bound = LZ4_compressBound(w * h * 4);
                        compressBuffer = new uint8_t[bound];
                        pixels = new uint8_t[w * h * 4];
                    }

                    for (int i = 0; i < item->m_frameCount; i++)
                    {
                        item->m_fileOffsets[i] = totalSize;

                        rlottie::Surface surface((uint32_t*)pixels, w, h, w * 4);
                        item->m_animation->renderSync(i, surface);

                        uint32_t size = (uint32_t)LZ4_compress_default((const char*)pixels, (char*)compressBuffer, w * h * 4, bound);

                        if (size > item->m_maxFrameSize && item->m_decompressBuffer != nullptr)
                        {
                            delete[] item->m_decompressBuffer;
                            item->m_decompressBuffer = nullptr;
                        }

                        item->m_maxFrameSize = max(item->m_maxFrameSize, size);

                        WriteFile(precacheFile, &size, sizeof(uint32_t), &write, NULL);
                        WriteFile(precacheFile, compressBuffer, sizeof(uint8_t) * size, &write, NULL);
                        totalSize += size;
                        totalSize += 4;
                    }

                    SetFilePointer(precacheFile, 0, NULL, FILE_BEGIN);
                    uint8_t version = CACHED_VERSION + 1;
                    item->m_imageSize = (uint32_t)w * h * 4;
                    WriteFile(precacheFile, &version, sizeof(uint8_t), &write, NULL);
                    WriteFile(precacheFile, &item->m_maxFrameSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &item->m_imageSize, sizeof(uint32_t), &write, NULL);
                    WriteFile(precacheFile, &item->m_fps, sizeof(int32_t), &write, NULL);
                    WriteFile(precacheFile, &item->m_frameCount, sizeof(size_t), &write, NULL);
                    WriteFile(precacheFile, &item->m_fileOffsets[0], sizeof(uint32_t) * item->m_frameCount, &write, NULL);
                    CloseHandle(precacheFile);
                }

                item->m_caching = false;

                oldW = w;
                oldH = h;
            }

            if (compressBuffer)
            {
                delete[] compressBuffer;
            }

            if (pixels)
            {
                delete[] pixels;
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
        return m_frameCount;
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