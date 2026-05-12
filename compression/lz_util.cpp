#include "lz_util.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;

LZUtil::LZUtil()
{
}

LZUtil::~LZUtil()
{
}

void LZUtil::setCompressionLevel(int level)
{
    compressionLevel = std::max(1, std::min(level, 9));
}

bool LZUtil::packWatchfaceFolder(const std::string &folderPath, const std::string &outputFilePath)
{
    std::vector<TqxFileEntry> entries;
    std::vector<uint8_t> payload;

    if (!gatherFiles(folderPath, entries, payload))
    {
        std::cerr << "Failed to gather files from folder: " << folderPath << std::endl;
        return false;
    }

    if (entries.empty())
    {
        std::cerr << "No files found in watchface folder: " << folderPath << std::endl;
        return false;
    }

    stats.originalSize = payload.size();

    auto start = std::chrono::high_resolution_clock::now();
    bool result = writeTqxFile(outputFilePath, entries, payload);
    auto end = std::chrono::high_resolution_clock::now();
    stats.compressionTime = std::chrono::duration<double>(end - start).count();

    if (result && stats.originalSize > 0)
    {
        stats.compressionRatio = static_cast<double>(stats.compressedSize) / static_cast<double>(stats.originalSize);
    }

    return result;
}

bool LZUtil::unpackTqx(const std::string &inputFilePath, const std::string &outputFolderPath)
{
    std::ifstream input(inputFilePath, std::ios::binary);
    if (!input)
    {
        std::cerr << "Cannot open .tqx file: " << inputFilePath << std::endl;
        return false;
    }

    std::vector<uint8_t> header(16);
    input.read(reinterpret_cast<char *>(header.data()), header.size());
    if (input.gcount() != static_cast<std::streamsize>(header.size()) || std::string(header.begin(), header.begin() + 4) != "TQX1")
    {
        std::cerr << "Invalid .tqx header: " << inputFilePath << std::endl;
        return false;
    }

    uint32_t version = readUint32(header.data() + 4);
    uint32_t blockSizeValue = readUint32(header.data() + 8);
    uint32_t fileCount = readUint32(header.data() + 12);
    if (version != 1)
    {
        std::cerr << "Unsupported .tqx version: " << version << std::endl;
        return false;
    }

    std::vector<TqxFileEntry> entries;
    entries.reserve(fileCount);

    for (uint32_t i = 0; i < fileCount; ++i)
    {
        uint16_t nameLen;
        input.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
        if (!input)
        {
            std::cerr << "Failed to read file name length." << std::endl;
            return false;
        }

        std::vector<char> nameBuf(nameLen);
        input.read(nameBuf.data(), nameLen);
        if (!input)
        {
            std::cerr << "Failed to read file name." << std::endl;
            return false;
        }

        TqxFileEntry entry;
        entry.name.assign(nameBuf.begin(), nameBuf.end());
        input.read(reinterpret_cast<char *>(&entry.originalSize), sizeof(entry.originalSize));
        input.read(reinterpret_cast<char *>(&entry.offset), sizeof(entry.offset));
        if (!input)
        {
            std::cerr << "Failed to read file metadata." << std::endl;
            return false;
        }

        entries.push_back(entry);
    }

    fs::create_directories(outputFolderPath);
    size_t totalUncompressed = 0;
    std::vector<uint8_t> stream;

    while (input)
    {
        uint32_t uncompressedSize;
        uint32_t compressedSize;
        input.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));
        if (!input)
            break;
        input.read(reinterpret_cast<char *>(&compressedSize), sizeof(compressedSize));
        if (!input)
        {
            std::cerr << "Failed to read compressed block sizes." << std::endl;
            return false;
        }

        std::vector<uint8_t> compressedBlock(compressedSize);
        input.read(reinterpret_cast<char *>(compressedBlock.data()), compressedSize);
        if (!input)
        {
            std::cerr << "Failed to read compressed block data." << std::endl;
            return false;
        }

        std::vector<uint8_t> decompressed = decompressBlock(compressedBlock, uncompressedSize);
        stream.insert(stream.end(), decompressed.begin(), decompressed.end());
        totalUncompressed += decompressed.size();
    }

    for (const auto &entry : entries)
    {
        const uint8_t *fileBegin = stream.data() + entry.offset;
        std::vector<uint8_t> fileData(fileBegin, fileBegin + entry.originalSize);

        fs::path outPath = fs::path(outputFolderPath) / fs::path(entry.name);
        fs::create_directories(outPath.parent_path());
        std::ofstream output(outPath, std::ios::binary);
        output.write(reinterpret_cast<const char *>(fileData.data()), fileData.size());
    }

    stats.decompressedSize = totalUncompressed;
    return true;
}

void LZUtil::displayCompressionStats() const
{
    std::cout << "Compression stats:\n";
    std::cout << "  original size: " << stats.originalSize << " bytes\n";
    std::cout << "  compressed size: " << stats.compressedSize << " bytes\n";
    std::cout << "  ratio: " << stats.compressionRatio << "\n";
    std::cout << "  compression time: " << stats.compressionTime << " seconds\n";
}

bool LZUtil::gatherFiles(const std::string &folderPath, std::vector<TqxFileEntry> &entries, std::vector<uint8_t> &payload)
{
    fs::path root(folderPath);
    if (!fs::exists(root) || !fs::is_directory(root))
    {
        return false;
    }

    for (const auto &item : fs::recursive_directory_iterator(root))
    {
        if (!item.is_regular_file())
            continue;

        fs::path relativePath = item.path().lexically_relative(root);
        TqxFileEntry entry;
        entry.name = relativePath.generic_string();
        entry.offset = payload.size();

        std::vector<uint8_t> fileBytes;
        if (!readFileContents(item.path(), fileBytes))
            return false;

        entry.originalSize = fileBytes.size();
        payload.insert(payload.end(), fileBytes.begin(), fileBytes.end());
        entries.push_back(std::move(entry));
    }

    return true;
}

bool LZUtil::writeTqxFile(const std::string &outputFilePath, const std::vector<TqxFileEntry> &entries, const std::vector<uint8_t> &payload)
{
    std::ofstream output(outputFilePath, std::ios::binary);
    if (!output)
        return false;

    output.write("TQX1", 4);
    uint32_t version = 1;
    output.write(reinterpret_cast<const char *>(&version), sizeof(version));
    uint32_t blockSizeValue = static_cast<uint32_t>(blockSize);
    output.write(reinterpret_cast<const char *>(&blockSizeValue), sizeof(blockSizeValue));
    uint32_t fileCount = static_cast<uint32_t>(entries.size());
    output.write(reinterpret_cast<const char *>(&fileCount), sizeof(fileCount));

    for (const auto &entry : entries)
    {
        uint16_t nameLen = static_cast<uint16_t>(entry.name.size());
        output.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
        output.write(entry.name.data(), nameLen);
        output.write(reinterpret_cast<const char *>(&entry.originalSize), sizeof(entry.originalSize));
        output.write(reinterpret_cast<const char *>(&entry.offset), sizeof(entry.offset));
    }

    for (size_t offset = 0; offset < payload.size(); offset += blockSize)
    {
        size_t chunkSize = std::min(blockSize, payload.size() - offset);
        std::vector<uint8_t> block(payload.begin() + offset, payload.begin() + offset + chunkSize);
        std::vector<uint8_t> compressedBlock = compressBlock(block);

        uint32_t uncompressedSize = static_cast<uint32_t>(chunkSize);
        uint32_t compressedSize = static_cast<uint32_t>(compressedBlock.size());
        output.write(reinterpret_cast<const char *>(&uncompressedSize), sizeof(uncompressedSize));
        output.write(reinterpret_cast<const char *>(&compressedSize), sizeof(compressedSize));
        output.write(reinterpret_cast<const char *>(compressedBlock.data()), compressedBlock.size());
    }

    output.flush();
    stats.compressedSize = static_cast<size_t>(output.tellp());
    stats.tqxFileSize = stats.compressedSize;
    return true;
}

bool LZUtil::readFileContents(const fs::path &filePath, std::vector<uint8_t> &data)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        return false;

    input.seekg(0, std::ios::end);
    std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);

    data.resize(static_cast<size_t>(size));
    if (size > 0)
    {
        input.read(reinterpret_cast<char *>(data.data()), size);
        if (!input)
            return false;
    }

    return true;
}

std::vector<uint8_t> LZUtil::compressBlock(const std::vector<uint8_t> &input)
{
    constexpr size_t maxDistance = 4096;
    constexpr size_t minMatchLength = 3;
    constexpr size_t maxMatchLength = 255 + 3;

    std::vector<uint8_t> output;
    size_t pos = 0;
    size_t literalStart = 0;

    auto flushLiterals = [&](size_t end)
    {
        while (literalStart < end)
        {
            size_t literalLen = std::min<size_t>(end - literalStart, 15);
            output.push_back(static_cast<uint8_t>(literalLen));
            output.insert(output.end(), input.begin() + literalStart, input.begin() + literalStart + literalLen);
            output.push_back(0x00);
            literalStart += literalLen;
        }
    };

    while (pos < input.size())
    {
        size_t bestLength = 0;
        size_t bestOffset = 0;
        size_t searchStart = (pos > maxDistance) ? (pos - maxDistance) : 0;

        for (size_t candidate = searchStart; candidate + minMatchLength <= pos; ++candidate)
        {
            size_t matchLength = 0;
            while (matchLength < maxMatchLength && pos + matchLength < input.size() && input[candidate + matchLength] == input[pos + matchLength])
            {
                ++matchLength;
            }

            if (matchLength >= minMatchLength && matchLength > bestLength)
            {
                bestLength = matchLength;
                bestOffset = pos - candidate;
            }
        }

        if (bestLength >= minMatchLength)
        {
            if (literalStart < pos)
            {
                flushLiterals(pos);
            }

            size_t literalLen = 0;
            output.push_back(static_cast<uint8_t>(literalLen));
            output.push_back(0x01);

            uint16_t offsetValue = static_cast<uint16_t>(bestOffset);
            output.push_back(static_cast<uint8_t>(offsetValue & 0xFF));
            output.push_back(static_cast<uint8_t>((offsetValue >> 8) & 0xFF));

            size_t matchLenMinus3 = bestLength - 3;
            output.push_back(static_cast<uint8_t>(std::min<size_t>(matchLenMinus3, 255)));

            pos += bestLength;
            literalStart = pos;
        }
        else
        {
            pos++;
        }
    }

    if (literalStart < input.size())
    {
        flushLiterals(input.size());
    }

    return output;
}

std::vector<uint8_t> LZUtil::decompressBlock(const std::vector<uint8_t> &input, size_t uncompressedSize)
{
    std::vector<uint8_t> output;
    output.reserve(uncompressedSize);
    size_t pos = 0;

    while (pos < input.size())
    {
        if (pos >= input.size())
            break;

        uint8_t literalLen = input[pos++];
        if (pos + literalLen > input.size())
            break;

        output.insert(output.end(), input.begin() + pos, input.begin() + pos + literalLen);
        pos += literalLen;

        if (pos >= input.size())
            break;

        uint8_t hasMatch = input[pos++];
        if (hasMatch == 0)
            continue;

        if (pos + 3 > input.size())
            break;

        uint16_t offset = static_cast<uint16_t>(input[pos] | (input[pos + 1] << 8));
        pos += 2;
        uint8_t matchLenMinus3 = input[pos++];
        size_t matchLen = static_cast<size_t>(matchLenMinus3) + 3;

        size_t start = output.size() - offset;
        for (size_t i = 0; i < matchLen; ++i)
            output.push_back(output[start + i]);
    }

    if (output.size() != uncompressedSize)
    {
        output.resize(uncompressedSize);
    }

    return output;
}

void LZUtil::appendUint16(std::vector<uint8_t> &buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void LZUtil::appendUint32(std::vector<uint8_t> &buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void LZUtil::appendUint64(std::vector<uint8_t> &buffer, uint64_t value)
{
    for (int i = 0; i < 8; ++i)
        buffer.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
}

uint16_t LZUtil::readUint16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t LZUtil::readUint32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

uint64_t LZUtil::readUint64(const uint8_t *data)
{
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<uint64_t>(data[i]) << (8 * i);
    return value;
}
