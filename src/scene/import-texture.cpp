#include "import-texture.h"

#include <string>
#include <stdexcept>
#include <algorithm>
#include <vector>

#include <sys/stat.h>

using std::string;
using std::runtime_error;
using std::make_unique;
using std::max;
using std::vector;
using std::unique_ptr;

#include <FreeImage.h>

static FIBITMAP *loadBitmap(string filename, VkFormat *format)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(filename.c_str(), 0);
	if (fif == FIF_UNKNOWN) {
		fif = FreeImage_GetFIFFromFilename(filename.c_str());
		if (fif == FIF_UNKNOWN)
			throw runtime_error("unknown image type");
	}

	if (!FreeImage_FIFSupportsReading(fif))
		throw runtime_error(string("file format can't be read: ") + FreeImage_GetFIFDescription(fif));

	FIBITMAP *dib = FreeImage_Load(fif, filename.c_str());
	if (!dib)
		throw runtime_error("failed to load image");

	auto imageType = FreeImage_GetImageType(dib);
	FIBITMAP *temp;
	switch (imageType)
	{
	case FIT_BITMAP:
		temp = dib;
		dib = FreeImage_ConvertTo32Bits(dib);
		FreeImage_Unload(temp);
		if (!dib)
			throw runtime_error("failed to convert to 32bits!");
		*format = VK_FORMAT_R8G8B8A8_UNORM;
		break;

	case FIT_RGBF:
		*format = VK_FORMAT_R16G16B16A16_SFLOAT;
		break;

	default:
		throw runtime_error("unsupported image-type!");
	}

	// FreeImage uses bottom-left origin, we use top-left
	FreeImage_FlipVertical(dib);
	return dib;
}

static int getBpp(FIBITMAP *dib)
{
	switch (FreeImage_GetImageType(dib))
	{
	case FIT_BITMAP: return FreeImage_GetBPP(dib);
	case FIT_RGBF: return sizeof(uint16_t) * 8 * 4; // expand to RGBA, which is always supported
	default:
		unreachable("unsupported type!");
	}
}

inline uint16_t float_to_half(float input)
{
	__m128 single = _mm_set_ss(input);
	__m128i half = _mm_cvtps_ph(single, 0);
	return static_cast<uint16_t>(_mm_cvtsi128_si32(half));
}

static StagingBuffer *copyToStagingBuffer(FIBITMAP *dib)
{
	auto imageType = FreeImage_GetImageType(dib);
	auto width = FreeImage_GetWidth(dib);
	auto height = FreeImage_GetHeight(dib);

	auto bpp = getBpp(dib);
	assert(bpp % 8 == 0);
	auto pixelSize = bpp / 8;

	auto pitch = width * pixelSize;
	auto size = pitch * height;

	auto stagingBuffer = new StagingBuffer(size);
	void *ptr = stagingBuffer->map(0, size);

	for (auto y = 0u; y < height; ++y) {
		auto srcRow = FreeImage_GetScanLine(dib, y);
		auto dstRow = static_cast<uint8_t *>(ptr) + pitch * y;
		FIRGBF *srcRowRGBf;
		uint16_t *dstRowHalf = (uint16_t *)dstRow;

		switch (imageType)
		{
		case FIT_BITMAP:
			for (auto x = 0u; x < width; ++x) {
				dstRow[x * 4 + 0] = srcRow[x * 4 + FI_RGBA_RED];
				dstRow[x * 4 + 1] = srcRow[x * 4 + FI_RGBA_GREEN];
				dstRow[x * 4 + 2] = srcRow[x * 4 + FI_RGBA_BLUE];
				dstRow[x * 4 + 3] = srcRow[x * 4 + FI_RGBA_ALPHA];
			}
			break;
		case FIT_RGBF:
			srcRowRGBf = (FIRGBF *)srcRow;
			for (auto x = 0u; x < width; ++x) {
				dstRowHalf[x * 4 + 0] = float_to_half(srcRowRGBf[x].red);
				dstRowHalf[x * 4 + 1] = float_to_half(srcRowRGBf[x].green);
				dstRowHalf[x * 4 + 2] = float_to_half(srcRowRGBf[x].blue);
				dstRowHalf[x * 4 + 3] = float_to_half(1.0f);
			}
			break;
		default:
			unreachable("unsupported type!");
		}
	}
	stagingBuffer->unmap();
	return stagingBuffer;
}

static void uploadMipChain(TextureBase &texture, FIBITMAP *dib, int mipLevels, int arrayLayer = 0)
{
	auto baseWidth = FreeImage_GetWidth(dib);
	auto baseHeight = FreeImage_GetHeight(dib);

	for (auto mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
		auto mipWidth = TextureBase::mipSize(baseWidth, mipLevel),
		     mipHeight = TextureBase::mipSize(baseHeight, mipLevel);

		if (mipLevel > 0) {
			auto temp = dib;
			dib = FreeImage_Rescale(dib, mipWidth, mipHeight, FILTER_BOX);
			assert(dib != nullptr);
			FreeImage_Unload(temp);
		}

		assert(FreeImage_GetWidth(dib) == mipWidth);
		assert(FreeImage_GetHeight(dib) == mipHeight);

		auto stagingBuffer = copyToStagingBuffer(dib);
		texture.uploadFromStagingBuffer(stagingBuffer, mipLevel, arrayLayer);
		// TODO: delete staging buffer
	}

	FreeImage_Unload(dib);
}

std::unique_ptr<Texture2D> importTexture2D(string filename, TextureImportFlags flags)
{
	VkFormat format = VK_FORMAT_UNDEFINED;
	auto dib = loadBitmap(filename, &format);
	assert(format != VK_FORMAT_UNDEFINED);

	if (flags & TextureImportFlags::PREMULTIPLY_ALPHA)
		FreeImage_PreMultiplyWithAlpha(dib);

	auto baseWidth = FreeImage_GetWidth(dib);
	auto baseHeight = FreeImage_GetHeight(dib);

	auto mipLevels = 1;
	if (flags & TextureImportFlags::GENERATE_MIPMAPS)
		mipLevels = TextureBase::maxMipLevels(max(baseWidth, baseHeight));

	auto texture = make_unique<Texture2D>(format, baseWidth, baseHeight, mipLevels, 1, true);
	uploadMipChain(*texture, dib, mipLevels);
	return texture;
}

unique_ptr<Texture2DArray> importTexture2DArray(string folder, TextureImportFlags flags)
{
	VkFormat firstFormat = VK_FORMAT_UNDEFINED;
	unsigned int firstWidth, firstHeight;

	vector<FIBITMAP *> bitmaps;
	for (int i = 0; true; ++i) {
		char path[256];
		snprintf(path, sizeof(path), "%s/%04d.png", folder.c_str(), i);

		struct stat st;
		if ((stat(path, &st) < 0) ||
		    (st.st_mode & S_IFMT) != S_IFREG)
			break;

		VkFormat format = VK_FORMAT_UNDEFINED;
		auto dib = loadBitmap(path, &format);

		auto width = FreeImage_GetWidth(dib);
		auto height = FreeImage_GetHeight(dib);

		if (i == 0) {
			firstFormat = format;
			firstWidth = width;
			firstHeight = height;
		} else if (firstFormat != format ||
		           firstWidth != width ||
		           firstHeight != height)
			throw runtime_error("inconsistent format or size!");

		if (flags & TextureImportFlags::PREMULTIPLY_ALPHA)
			FreeImage_PreMultiplyWithAlpha(dib);

		bitmaps.push_back(dib);
	}

	if (bitmaps.size() == 0)
		throw runtime_error("empty texture-array!");

	auto mipLevels = 1;
	if (flags & TextureImportFlags::GENERATE_MIPMAPS)
		mipLevels = TextureBase::maxMipLevels(max(firstWidth, firstHeight));

	assert(bitmaps.size() < INT_MAX);
	auto texture = make_unique<Texture2DArray>(firstFormat, firstWidth, firstHeight, int(bitmaps.size()), mipLevels, true);
	for (int i = 0; i < texture->getArrayLayers(); ++i)
		uploadMipChain(*texture, bitmaps[i], mipLevels, i);

	return texture;
}

unique_ptr<TextureCube> importTextureCube(string filename, TextureImportFlags flags)
{
	VkFormat format = VK_FORMAT_UNDEFINED;
	auto dib = loadBitmap(filename, &format);
	assert(format != VK_FORMAT_UNDEFINED);

	auto imageWidth = FreeImage_GetWidth(dib);
	auto imageHeight = FreeImage_GetHeight(dib);
	auto baseSize = imageWidth / 3;

	if (imageWidth % 3 != 0 ||
		imageHeight != baseSize * 4)
		throw runtime_error("unexpected image size!");

	if (flags & TextureImportFlags::PREMULTIPLY_ALPHA)
		FreeImage_PreMultiplyWithAlpha(dib);

	auto mipLevels = 1;
	if (flags & TextureImportFlags::GENERATE_MIPMAPS)
		mipLevels = TextureBase::maxMipLevels(baseSize);

	auto texture = make_unique<TextureCube>(format, baseSize, mipLevels);

	static const int offsets[6][2] = {
		{ 2, 2 }, // -X
		{ 0, 2 }, // +X
		{ 1, 3 }, // +Y
		{ 1, 1 }, // -Y
		{ 1, 2 }, // +Z
		{ 1, 0 }, // -Z - this one is upside down :(
	};
	for (auto face = 0; face < 6; ++face) {
		auto left = offsets[face][0] * baseSize,
		     top  = offsets[face][1] * baseSize;
		auto faceDib = FreeImage_Copy(dib, left, top, left + baseSize, top + baseSize);

		if (face == 5) {
			FreeImage_FlipVertical(faceDib);
			FreeImage_FlipHorizontal(faceDib);
		}

		uploadMipChain(*texture, faceDib, mipLevels, face);
	}

	FreeImage_Unload(dib);
	return texture;
}
