#ifndef LZ_UTIL_H
#define LZ_UTIL_H

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

struct CompressionStats
{
    size_t originalSize = 0;
    size_t compressedSize = 0;
    size_t decompressedSize = 0;
    size_t tqxFileSize = 0;
    double compressionRatio = 0.0;
    double compressionTime = 0.0;
    double decompressionTime = 0.0;
};

struct TqxFileEntry
{
    std::string name;
    uint64_t originalSize = 0;
    uint64_t offset = 0;
};

class LZUtil
{
public:
    LZUtil();
    ~LZUtil();

    void setCompressionLevel(int level);
    bool packWatchfaceFolder(const std::string &folderPath, const std::string &outputFilePath);
    bool unpackTqx(const std::string &inputFilePath, const std::string &outputFolderPath);
    void displayCompressionStats() const;

private:
    size_t blockSize = 2048;
    int compressionLevel = 1;
    CompressionStats stats;

    bool gatherFiles(const std::string &folderPath, std::vector<TqxFileEntry> &entries, std::vector<uint8_t> &payload);
    bool writeTqxFile(const std::string &outputFilePath, const std::vector<TqxFileEntry> &entries, const std::vector<uint8_t> &payload);
    bool readFileContents(const std::filesystem::path &filePath, std::vector<uint8_t> &data);

    static std::vector<uint8_t> compressBlock(const std::vector<uint8_t> &input);
    static std::vector<uint8_t> decompressBlock(const std::vector<uint8_t> &input, size_t uncompressedSize);

    static void appendUint16(std::vector<uint8_t> &buffer, uint16_t value);
    static void appendUint32(std::vector<uint8_t> &buffer, uint32_t value);
    static void appendUint64(std::vector<uint8_t> &buffer, uint64_t value);
    static uint16_t readUint16(const uint8_t *data);
    static uint32_t readUint32(const uint8_t *data);
    static uint64_t readUint64(const uint8_t *data);
};

#endif // LZ_UTIL_H
