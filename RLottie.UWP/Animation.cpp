#include "pch.h"
#include "Animation.h"
#include "rlottie.h"

#include <string>
#include <fstream>
#include <streambuf>

#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>

using namespace RLottie;
using namespace Platform;
using namespace Platform::Collections;

inline std::string from_wstring(const wchar_t* begin, size_t size) {
	size_t result_len = 0;
	for (size_t i = 0; i < size; i++) {
		unsigned int cur = begin[i];
		if ((cur & 0xF800) == 0xD800) {
			if (i < size) {
				unsigned int next = begin[++i];
				if ((next & 0xFC00) == 0xDC00 && (cur & 0x400) == 0) {
					result_len += 4;
					continue;
				}
			}

			return nullptr;
		}
		result_len += 1 + (cur >= 0x80) + (cur >= 0x800);
	}

	std::string result(result_len, '\0');
	if (result_len) {
		char* res = &result[0];
		for (size_t i = 0; i < size; i++) {
			unsigned int cur = begin[i];
			// TODO conversion unsigned int -> signed char is implementation defined
			if (cur <= 0x7f) {
				*res++ = static_cast<char>(cur);
			}
			else if (cur <= 0x7ff) {
				*res++ = static_cast<char>(0xc0 | (cur >> 6));
				*res++ = static_cast<char>(0x80 | (cur & 0x3f));
			}
			else if ((cur & 0xF800) != 0xD800) {
				*res++ = static_cast<char>(0xe0 | (cur >> 12));
				*res++ = static_cast<char>(0x80 | ((cur >> 6) & 0x3f));
				*res++ = static_cast<char>(0x80 | (cur & 0x3f));
			}
			else {
				unsigned int next = begin[++i];
				unsigned int val = ((cur - 0xD800) << 10) + next - 0xDC00 + 0x10000;

				*res++ = static_cast<char>(0xf0 | (val >> 18));
				*res++ = static_cast<char>(0x80 | ((val >> 12) & 0x3f));
				*res++ = static_cast<char>(0x80 | ((val >> 6) & 0x3f));
				*res++ = static_cast<char>(0x80 | (val & 0x3f));
			}
		}
	}
	return result;
}

inline std::string string_to_unmanaged(String^ str) {
	if (!str) {
		return std::string();
	}
	return from_wstring(str->Data(), str->Length());
}

Animation^ Animation::LoadFromData(String^ jsonData) {
	auto data = string_to_unmanaged(jsonData);
	return ref new Animation(data);
}

Animation^ Animation::LoadFromFile(String^ filePath) {
	std::ifstream t(filePath->Data(), std::ios::binary | std::ios::ate);
	std::streamsize size = t.tellg();
	t.seekg(0, std::ios::beg);

	char* buffer = static_cast<char*> (malloc(size));
	if (t.read(buffer, size))
	{
		t.close();

		std::string data;

		bool compressed = gzip::is_compressed(buffer, size);
		if (compressed) {
			data = gzip::decompress(buffer, size);
		}
		else {
			data = std::string(buffer, size);
		}

		return ref new Animation(data);
	}

	return nullptr;
}

Animation::Animation(std::string jsonData) {
	auto animation = rlottie::Animation::loadFromData(jsonData, "data");

	animation.swap(m_animation);
}

Array<byte>^ Animation::RenderSync(int frameNo) {
	uint32_t width = 512;
	uint32_t height = 512;
	auto stride = width * 4;

	std::uint32_t* buffer = static_cast<std::uint32_t*> (malloc(width * height * 4));

	rlottie::Surface surface(buffer, width, height, stride);
	m_animation->renderSync(frameNo, surface, true);

	uint8_t* pixels = (uint8_t*)buffer;

	Array<byte>^ res = ref new Array<byte>(pixels, width * height * 4);
	delete[] buffer;

	return res;
}