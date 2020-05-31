#pragma once

#include <string>

#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>

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

			return {};
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

inline std::string string_to_unmanaged(winrt::hstring str) {
	//if (!str) {
	//	return std::string();
	//}
	return from_wstring(str.data(), str.size());
}

inline std::string DecompressFromFile(winrt::hstring filePath) {
	FILE* file = _wfopen(filePath.c_str(), L"rb");
	if (file == NULL) {
		return nullptr;
	}

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* buffer = (char*)malloc(length);
	fread(buffer, 1, length, file);
	fclose(file);

	std::string data;

	bool compressed = gzip::is_compressed(buffer, length);
	if (compressed) {
		data = gzip::decompress(buffer, length);
	}
	else {
		data = std::string(buffer, length);
	}

	return data;
}