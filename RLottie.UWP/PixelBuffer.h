#pragma once

#include "PixelBuffer.g.h"

#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

using namespace winrt::Windows::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::RLottie::implementation
{
    struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
    };

    struct PixelBuffer : PixelBufferT<PixelBuffer, IBufferByteAccess>
    {
        PixelBuffer(WriteableBitmap bitmap)
            : m_bitmap(bitmap)
        {
            auto buffer = bitmap.PixelBuffer();
            m_pixels = buffer.data();
            m_capacity = buffer.Capacity();
            m_length = buffer.Length();

            m_bitmapWidth = bitmap.PixelWidth();
            m_bitmapHeight = bitmap.PixelHeight();
        }

        uint32_t Capacity() {
            return m_capacity;
        }

        uint32_t Length() {
            return m_length;
        }

        void Length(uint32_t value) {

        }

        HRESULT __stdcall Buffer(uint8_t** value)
        {
            *value = m_pixels;
            return S_OK;
        }



        int32_t PixelWidth() {
            return m_bitmapWidth;
        }

        int32_t PixelHeight() {
            return m_bitmapHeight;
        }

        WriteableBitmap Bitmap() {
            return m_bitmap;
        }

    private:
        WriteableBitmap m_bitmap;
        uint8_t* m_pixels;
        int32_t m_bitmapWidth;
        int32_t m_bitmapHeight;

        uint32_t m_capacity;
        uint32_t m_length;
    };
}

namespace winrt::RLottie::factory_implementation
{
    struct PixelBuffer : PixelBufferT<PixelBuffer, implementation::PixelBuffer>
    {
    };
}
