#ifndef IMPORT_TEXTURE_H
#define IMPORT_TEXTURE_H

#include "texture.h"
#include <string>

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

Texture2D importTexture2D(std::string filename, TextureImportFlags flags);
TextureCube importTextureCube(std::string filename, TextureImportFlags flags);
Texture2DArray importTexture2DArray(std::string filename, TextureImportFlags flags);

#endif // IMPORT_TEXTURE_H
