#pragma once

#include <string>
#include <zlib.h>

inline bool IsGzipCompressed(const char* data, size_t length)
{
    if (length < 10) return false;
    return (static_cast<unsigned char>(data[0]) == 0x1f &&
        static_cast<unsigned char>(data[1]) == 0x8b);
}

inline std::string DecompressFromFile(winrt::hstring filePath)
{
    FILE* file;
    _wfopen_s(&file, filePath.c_str(), L"rb");
    if (file == NULL)
    {
        return "";
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(length);
    fread(buffer, 1, length, file);
    fclose(file);

    if (!buffer || length == 0)
    {
        free(buffer);
        return "";
    }

    if (!IsGzipCompressed(buffer, length))
    {
        free(buffer);
        return std::string(buffer, length);
    }

    z_stream stream = {};

    if (inflateInit2(&stream, 15 + 16) != Z_OK)
    {
        free(buffer);
        return "";
    }

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(buffer));
    stream.avail_in = static_cast<uInt>(length);

    std::string decompressed;
    const size_t CHUNK_SIZE = 32768;

    int ret;
    do
    {
        std::vector<char> chunk(CHUNK_SIZE);
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            inflateEnd(&stream);
            free(buffer);
            return "";
        }

        size_t decompressedSize = chunk.size() - stream.avail_out;
        decompressed.append(chunk.data(), decompressedSize);

    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
    free(buffer);
    return decompressed;
}