// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "archive.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <zlib.h>

namespace {
constexpr std::uint32_t end_of_central_directory = 0x06054b50;
constexpr std::uint32_t central_directory_header = 0x02014b50;
constexpr std::uint32_t local_file_header = 0x04034b50;
constexpr std::size_t end_record_size = 22;
constexpr std::size_t max_zip_comment = 65535;
constexpr std::size_t central_header_size = 46;
constexpr std::size_t local_header_size = 30;
constexpr std::size_t max_entry_size = 64 * 1024 * 1024;
constexpr std::uint16_t encrypted_flag = 1;
constexpr std::uint16_t stored_method = 0;
constexpr std::uint16_t deflated_method = 8;

std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p > b.size() || b.size() - p < 2) throw std::runtime_error("Truncated ZIP archive");
    return std::uint16_t(b[p] | (unsigned(b[p + 1]) << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
    return std::uint32_t(u16(b, p)) | (std::uint32_t(u16(b, p + 2)) << 16);
}
std::string normalize(std::string_view name) {
    const auto separator = name.find_last_of("/\\");
    std::string result(name.substr(separator == std::string_view::npos ? 0 : separator + 1));
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return c == '\\' ? '/' : char(std::tolower(c));
    });
    return result;
}
}
Archive::Archive(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open ZIP archive: " + path.string());
    bytes_ = {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::vector<std::uint8_t> Archive::read(std::string_view path) const {
    if (bytes_.size() < end_record_size) throw std::runtime_error("Invalid ZIP archive");
    std::size_t end = bytes_.size() - end_record_size;
    const auto search_limit = end_record_size + max_zip_comment;
    const auto minimum = bytes_.size() > search_limit ? bytes_.size() - search_limit : 0;
    while (u32(bytes_, end) != end_of_central_directory || end + end_record_size + u16(bytes_, end + 20) != bytes_.size()) {
        if (end == minimum) throw std::runtime_error("ZIP directory not found");
        --end;
    }
    if (u16(bytes_, end + 4) || u16(bytes_, end + 6) || u16(bytes_, end + 8) != u16(bytes_, end + 10))
        throw std::runtime_error("Multi-disk ZIP is unsupported");
    auto pos = std::size_t(u32(bytes_, end + 16));
    const auto count = u16(bytes_, end + 10);
    for (unsigned i = 0; i < count; ++i) {
        if (u32(bytes_, pos) != central_directory_header) throw std::runtime_error("Invalid ZIP directory");
        const auto flags = u16(bytes_, pos + 8), method = u16(bytes_, pos + 10);
        const auto crc = u32(bytes_, pos + 16), packed = u32(bytes_, pos + 20), size = u32(bytes_, pos + 24);
        const auto name = u16(bytes_, pos + 28), extra = u16(bytes_, pos + 30), comment = u16(bytes_, pos + 32);
        const auto local = std::size_t(u32(bytes_, pos + 42));
        if (pos > bytes_.size() || bytes_.size() - pos < central_header_size + name + extra + comment) throw std::runtime_error("Invalid ZIP entry");
        std::string entry(reinterpret_cast<const char*>(bytes_.data() + pos + central_header_size), name);
        pos += central_header_size + name + extra + comment;
        if (entry.empty() || entry.back() == '/' || entry.back() == '\\' || normalize(entry) != normalize(path)) continue;
        if (flags & encrypted_flag || (method != stored_method && method != deflated_method)) throw std::runtime_error("Unsupported ZIP entry");
        if (u32(bytes_, local) != local_file_header) throw std::runtime_error("Invalid ZIP local header");
        const auto data = local + local_header_size + u16(bytes_, local + 26) + u16(bytes_, local + 28);
        if (data > bytes_.size() || packed > bytes_.size() - data || size > max_entry_size) throw std::runtime_error("Invalid ZIP entry size");
        std::vector<std::uint8_t> result(size);
        if (method == stored_method) {
            if (packed != size) throw std::runtime_error("Invalid ZIP entry size");
            std::copy_n(bytes_.data() + data, size, result.data());
        } else {
            z_stream stream{};
            stream.next_in = const_cast<Bytef*>(bytes_.data() + data);
            stream.avail_in = packed;
            stream.next_out = result.data();
            stream.avail_out = size;
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) throw std::runtime_error("Cannot initialize ZIP decompressor");
            const auto status = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
            if (status != Z_STREAM_END || stream.total_out != size || stream.total_in != packed) throw std::runtime_error("Invalid compressed ZIP entry");
        }
        if (crc32(0, result.data(), size) != crc) throw std::runtime_error("ZIP checksum mismatch");
        return result;
    }
    throw std::runtime_error("File not found in ZIP: " + std::string(path));
}
