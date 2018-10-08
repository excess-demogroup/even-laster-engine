#ifndef IMPORT_TEXTURE_H
#define IMPORT_TEXTURE_H

#include "texture.h"
#include <string>
#include <memory>

enum TextureImportFlags {
	NONE = 0,
	GENERATE_MIPMAPS = 1 << 0,
	PREMULTIPLY_ALPHA = 1 << 1,
};

inline TextureImportFlags operator|(const TextureImportFlags &a, const TextureImportFlags &b)
{
	return static_cast<TextureImportFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline TextureImportFlags operator |= (TextureImportFlags &a, const TextureImportFlags &b)
{
	return static_cast<TextureImportFlags>(static_cast<int>(a) | static_cast<int>(b));
}

std::unique_ptr<Texture2D> importTexture2D(const std::string &filename, TextureImportFlags flags);
std::unique_ptr<TextureCube> importTextureCube(const std::string &filename, TextureImportFlags flags);
std::unique_ptr<Texture2DArray> importTexture2DArray(const std::string &filename, TextureImportFlags flags);
std::unique_ptr<Texture3D> importCubeFile(const std::string &filename);

#endif // IMPORT_TEXTURE_H
