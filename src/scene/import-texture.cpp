#include "import-texture.h"

#include <FreeImage.h>

#ifdef _MSC_VER

#include <intrin.h>
static inline uint32_t clz(uint32_t x)
{
	unsigned long r = 0;
	if (_BitScanReverse(&r, x))
		return 31 - r;
	else
		return 32;
}

#elif defined(__GNUC__)

static inline uint32_t clz(uint32_t x)
{
	return x ? __builtin_clz(x) : 32;
}

#else

static inline uint32_t clz(uint32_t x)
{
	unsigned n = 0;
	for (int i = 1; i < 32; i++) {
		if (x & (1 << 31))
			return n;
		n++;
		x <<= 1;
	}
	return n;
}

#endif

static FIBITMAP *loadBitmap(std::string filename, VkFormat *format)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(filename.c_str(), 0);
	if (fif == FIF_UNKNOWN) {
		fif = FreeImage_GetFIFFromFilename(filename.c_str());
		if (fif == FIF_UNKNOWN)
			throw std::runtime_error("unknown image type");
	}

	if (!FreeImage_FIFSupportsReading(fif))
		throw std::runtime_error(std::string("file format can't be read: ") + FreeImage_GetFIFDescription(fif));

	FIBITMAP *dib = FreeImage_Load(fif, filename.c_str());
	if (!dib)
		throw std::runtime_error("failed to load image");

	auto imageType = FreeImage_GetImageType(dib);
	FIBITMAP *temp;
	switch (imageType)
	{
	case FIT_BITMAP:
		temp = dib;
		dib = FreeImage_ConvertTo32Bits(dib);
		FreeImage_Unload(temp);
		if (!dib)
			throw std::runtime_error("failed to convert to 32bits!");
		*format = VK_FORMAT_R8G8B8A8_UNORM;
		break;

	case FIT_RGBF:
		*format = VK_FORMAT_R32G32B32_SFLOAT; // TODO: convert to FP16?
		break;

	default:
		throw std::runtime_error("unsupported image-type!");
	}

	// FreeImage uses bottom-left origin, we use top-left
	FreeImage_FlipVertical(dib);
	return dib;
}

StagingBuffer *copyToStagingBuffer(FIBITMAP *dib)
{
	auto bpp = FreeImage_GetBPP(dib);
	assert(bpp % 8 == 0);
	auto pixelSize = bpp / 8;

	auto imageType = FreeImage_GetImageType(dib);
	auto width = FreeImage_GetWidth(dib);
	auto height = FreeImage_GetHeight(dib);

	auto pitch = width * pixelSize;
	auto size = pitch * height;

	auto stagingBuffer = new StagingBuffer(size);
	void *ptr = stagingBuffer->map(0, size);

	for (auto y = 0u; y < height; ++y) {
		auto srcRow = FreeImage_GetScanLine(dib, y);
		auto dstRow = static_cast<uint8_t *>(ptr) + pitch * y;
		FIRGBF *srcRowRGBf;
		float *dstRowFloat = (float *)dstRow;

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
				dstRowFloat[x * 3 + 0] = srcRowRGBf[x].red;
				dstRowFloat[x * 3 + 1] = srcRowRGBf[x].green;
				dstRowFloat[x * 3 + 2] = srcRowRGBf[x].blue;
			}
			break;
		default:
			unreachable("unsupported type!");
		}
	}
	stagingBuffer->unmap();
	return stagingBuffer;
}

void uploadMipChain(TextureBase &texture, FIBITMAP *dib, int mipLevels, int face = 0)
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
		texture.uploadFromStagingBuffer(stagingBuffer, mipLevel, face);
		// TODO: delete staging buffer
	}

	FreeImage_Unload(dib);
}

Texture2D importTexture2D(std::string filename, TextureImportFlags flags)
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
		mipLevels = 32 - clz(std::max(baseWidth, baseHeight));

	Texture2D texture(format, baseWidth, baseHeight, mipLevels, 1, true);
	uploadMipChain(texture, dib, mipLevels);
	return texture;
}