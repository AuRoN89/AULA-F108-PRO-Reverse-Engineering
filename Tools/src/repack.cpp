#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <regex>
#include <cstdint>
#include <algorithm>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "spi_blocks.h"

namespace fs = std::filesystem;

std::string INPUT_FILE;
std::string OUTPUT_FILE;
const std::string PATCH_DIR = "patch";

std::vector<uint8_t> img_to_rgb565_le(const uint8_t* pixels, int w, int h) {
    std::vector<uint8_t> out(w * h * 2);
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = pixels[i * 3];
        uint8_t g = pixels[i * 3 + 1];
        uint8_t b = pixels[i * 3 + 2];
        uint16_t word = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        out[i * 2] = word & 0xFF;
        out[i * 2 + 1] = (word >> 8) & 0xFF;
    }
    return out;
}

const uint32_t INVALID_OFFSET = UINT32_MAX;

uint32_t parse_offset(const std::string& filename) {
    std::regex re(R"(^\d+_([0-9A-Fa-f]{7})\.png$)", std::regex_constants::icase);
    std::smatch match;
    if (std::regex_match(filename, match, re)) {
        return std::stoul(match[1], nullptr, 16);
    }
    return INVALID_OFFSET;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    INPUT_FILE = argv[1];
    OUTPUT_FILE = INPUT_FILE + "_patched.bin";

    std::cout << "Input:  " << INPUT_FILE << std::endl;
    std::cout << "Output: " << OUTPUT_FILE << std::endl;
    std::cout << std::endl;

    if (!fs::exists(PATCH_DIR) || !fs::is_directory(PATCH_DIR)) {
        std::cerr << "ERROR: " << PATCH_DIR << " folder not found." << std::endl;
        return 1;
    }

    std::vector<std::string> patch_files;
    for (const auto& entry : fs::directory_iterator(PATCH_DIR)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            patch_files.push_back(entry.path().filename().string());
        }
    }
    std::sort(patch_files.begin(), patch_files.end());

    if (patch_files.empty()) {
        std::cerr << "No PNG files found in " << PATCH_DIR << " folder." << std::endl;
        return 0;
    }

    std::cout << "Files found: " << patch_files.size() << std::endl;
    std::cout << std::endl;

    std::ifstream infile(INPUT_FILE, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening " << INPUT_FILE << std::endl;
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    // Build offset map
    std::map<uint32_t, std::tuple<int, int, std::string>> offset_map;
    for (const auto& block : BLOCKS) {
        uint32_t block_off = std::get<0>(block);
        int block_count = std::get<1>(block);
        std::string label = std::get<2>(block);
        int W = std::get<3>(block);
        int H = std::get<4>(block);
        int frame_size = W * H * 2;
        for (int i = 0; i < block_count; ++i) {
            offset_map[block_off + i * frame_size] = {W, H, label};
        }
    }

    int patched_count = 0;
    int skipped_count = 0;
    int error_count = 0;

    for (const auto& filename : patch_files) {
        fs::path filepath = fs::path(PATCH_DIR) / filename;
        uint32_t offset = parse_offset(filename);

        if (offset == INVALID_OFFSET) {
            std::cout << "  SKIP  " << filename << " — unexpected filename format (expected: NNNN_XXXXXXX.png)" << std::endl;
            skipped_count++;
            continue;
        }

        if (offset_map.find(offset) == offset_map.end()) {
            std::cout << "  SKIP  " << filename << " — offset 0x" << std::hex << offset << " not found in BLOCKS TABLE" << std::endl;
            skipped_count++;
            continue;
        }

        auto [W, H, label] = offset_map[offset];
        int frame_size = W * H * 2;

        if (offset + frame_size > data.size()) {
            std::cout << "  ERR   " << filename << " — offset 0x" << std::hex << offset << " out of bounds" << std::endl;
            error_count++;
            continue;
        }

        int width, height, channels;
        uint8_t* img_data = stbi_load(filepath.string().c_str(), &width, &height, &channels, 3);
        if (!img_data) {
            std::cout << "  ERR   " << filename << " — error loading image" << std::endl;
            error_count++;
            continue;
        }

        if (width != W || height != H) {
            std::cout << "  ERR   " << filename << " — dimensioni errate (" << width << "x" << height << " vs " << W << "x" << H << ")" << std::endl;
            stbi_image_free(img_data);
            error_count++;
            continue;
        }

        auto raw = img_to_rgb565_le(img_data, W, H);
        stbi_image_free(img_data);

        std::vector<uint8_t> original(data.begin() + offset, data.begin() + offset + frame_size);

        if (raw == original) {
            std::cout << "  SKIP  " << filename << " — unchanged" << std::endl;
            skipped_count++;
            continue;
        }

        std::copy(raw.begin(), raw.end(), data.begin() + offset);

        std::cout << "  PATCH " << filename << " — 0x" << std::hex << offset << "  " << std::dec << W << "x" << H << "  [" << label << "]" << std::endl;
        patched_count++;
    }

    std::cout << std::endl;
    std::cout << "Patched: " << patched_count << "  |  Skipped: " << skipped_count << "  |  Errors: " << error_count << std::endl;
    std::cout << std::endl;

    if (patched_count > 0) {
        std::ofstream outfile(OUTPUT_FILE, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outfile.close();
        std::cout << "Saved: " << OUTPUT_FILE << std::endl;
    } else {
        std::cout << "No changes made. Output not generated." << std::endl;
    }

    return 0;
}