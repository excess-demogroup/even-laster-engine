class MemoryMappedFile
{
public:
	explicit MemoryMappedFile(const char *path)
	{
		WIN32_FILE_ATTRIBUTE_DATA attr;
		if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attr))
			throw std::runtime_error("failed to get file attributes");
		size = attr.nFileSizeLow; // TODO: care about nFileSizeHigh?

		hfile = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == hfile)
			throw std::runtime_error("failed to open file for reading");

		hmap = CreateFileMapping(hfile, 0, PAGE_READONLY, 0, 0, NULL);
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
