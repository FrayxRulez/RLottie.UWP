#include "pch.h"
#include "Animation.h"
#include "StringUtils.h"
#include "rlottie.h"

using namespace RLottie;
using namespace Platform;
using namespace Platform::Collections;

Animation^ Animation::LoadFromData(String^ jsonData, String^ key) {
	auto data = string_to_unmanaged(jsonData);
	auto cache = string_to_unmanaged(jsonData);
	return ref new Animation(data, cache);
}

Animation^ Animation::LoadFromFile(String^ filePath) {
	auto data = DecompressFromFile(filePath);
	auto cache = string_to_unmanaged(filePath);
	return ref new Animation(data, cache);
}

Animation::Animation(std::string jsonData, std::string key) {
	auto animation = rlottie::Animation::loadFromData(jsonData, key);

	animation.swap(m_animation);
}

Array<byte>^ Animation::RenderSync(int frameNo, int width, int height) {
	//uint32_t width = 512;
	//uint32_t height = 512;
	auto stride = width * 4;

	std::uint32_t* buffer = static_cast<std::uint32_t*> (malloc(width * height * 4));

	rlottie::Surface surface(buffer, width, height, stride);
	m_animation->renderSync(frameNo, surface, true);

	uint8_t* pixels = (uint8_t*)buffer;

	Array<byte>^ res = ref new Array<byte>(pixels, width * height * 4);
	delete[] buffer;

	return res;
}

CanvasBitmap^ Animation::RenderSync(ICanvasResourceCreator^ resourceCreator, int frame, int width, int height) {
	auto bytes = RenderSync(frame, width, height);

	// Would probably make sense to use ABI here, but I have no idea about how to
	auto bitmap = CanvasBitmap::CreateFromBytes(resourceCreator, bytes, width, height, Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized);
	delete[] bytes;

	return bitmap;
}