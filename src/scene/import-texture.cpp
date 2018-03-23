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


Texture2D importTexture2D(std::string filename, TextureImportFlags flags)
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
	auto format = VK_FORMAT_R8G8B8A8_UNORM;
	FIBITMAP *temp;
	switch (imageType)
	{
	case FIT_BITMAP:
		temp = dib;
		dib = FreeImage_ConvertTo32Bits(dib);
		FreeImage_Unload(temp);
		if (!dib)
			throw std::runtime_error("failed to convert to 32bits!");
		break;

	case FIT_RGBF:
		format = VK_FORMAT_R32G32B32_SFLOAT; // TODO: convert to FP16?
		break;

	default:
		throw std::runtime_error("unsupported image-type!");
	}

	if (flags & TextureImportFlags::PREMULTIPLY_ALPHA)
		FreeImage_PreMultiplyWithAlpha(dib);

	auto baseWidth = FreeImage_GetWidth(dib);
	auto baseHeight = FreeImage_GetHeight(dib);

	auto mipLevels = 1;
	if (flags & TextureImportFlags::GENERATE_MIPMAPS)
		mipLevels = 32 - clz(std::max(baseWidth, baseHeight));

	auto bpp = FreeImage_GetBPP(dib);
	assert(bpp % 8 == 0);
	auto pixelSize = bpp / 8;

	Texture2D texture(format, baseWidth, baseHeight, mipLevels, 1, true);

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

		auto pitch = mipWidth * pixelSize;
		auto size = pitch * mipHeight;

		auto stagingBuffer = new StagingBuffer(size);
		void *ptr = stagingBuffer->map(0, size);

		for (auto y = 0; y < mipHeight; ++y) {
			auto srcRow = FreeImage_GetScanLine(dib, mipHeight - 1 - y);
			auto dstRow = static_cast<uint8_t *>(ptr) + pitch * y;
			FIRGBF *srcRowRGBf;
			float *dstRowFloat = (float *)dstRow;

			switch (imageType)
			{
			case FIT_BITMAP:
				for (int x = 0; x < mipWidth; ++x) {
					dstRow[x * 4 + 0] = srcRow[x * 4 + FI_RGBA_RED];
					dstRow[x * 4 + 1] = srcRow[x * 4 + FI_RGBA_GREEN];
					dstRow[x * 4 + 2] = srcRow[x * 4 + FI_RGBA_BLUE];
					dstRow[x * 4 + 3] = srcRow[x * 4 + FI_RGBA_ALPHA];
				}
				break;
			case FIT_RGBF:
				srcRowRGBf = (FIRGBF *)srcRow;
				for (int x = 0; x < mipWidth; ++x) {
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
		texture.uploadFromStagingBuffer(stagingBuffer, mipLevel);
		// TODO: delete staging buffer
	}

	FreeImage_Unload(dib);
	return texture;
}
