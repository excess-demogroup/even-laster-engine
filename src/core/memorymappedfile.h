#ifndef MEMORYMAPPEDFILE_H
#define MEMORYMAPPEDFILE_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class MemoryMappedFile
{
public:
	explicit MemoryMappedFile(const std::string &path)
	{
		WIN32_FILE_ATTRIBUTE_DATA attr;
		if (!GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &attr))
			throw std::runtime_error("failed to get file attributes");
		size = attr.nFileSizeLow;

		if (attr.nFileSizeHigh != 0)
			throw std::runtime_error("too large file");

		hfile = CreateFile(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (INVALID_HANDLE_VALUE == hfile)
			throw std::runtime_error("failed to open file for reading");

		hmap = CreateFileMapping(hfile, 0, PAGE_READONLY, 0, 0, nullptr);
		if (!hmap)
			throw std::runtime_error("failed to create file mapping");

		data = MapViewOfFile(hmap, FILE_MAP_READ, 0, 0, 0);
		if (!data)
			throw std::runtime_error("failed to map view of file");
	}

	~MemoryMappedFile()
	{
		UnmapViewOfFile(data);
		CloseHandle(hmap);
		CloseHandle(hfile);
	}

	const void *getData() const { return data; }
	size_t getSize() const { return size; }

private:
	HANDLE hfile;
	HANDLE hmap;
	void *data;
	size_t size;
};

#endif // MEMORYMAPPEDFILE_H
