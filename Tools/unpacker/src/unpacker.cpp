/******************************************************************************
 *  Aula F108 Pro Unpacker
 *
 *  Copyright (c) 2026 Au{R}oN
 *
 *  Licensed under the Creative Commons Attribution 4.0 International License
 *  (CC BY 4.0).
 *
 *  You are free to:
 *  - Share: copy and redistribute the material in any medium or format
 *  - Adapt: remix, transform, and build upon the material for any purpose,
 *           even commercially
 *
 *  Under the following terms:
 *  - Attribution: You must give appropriate credit, provide a link to the
 *    license, and indicate if changes were made.
 *
 *  Full license text available at:
 *  https://creativecommons.org/licenses/by/4.0/
 *
 *  Description:
 *  SPI unpacking tool for Aula F108 Pro keyboard.
 *
 *  Author: Au{R}oN
 *  Year:   2026
 *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "spi_blocks.h"

namespace fs = std::filesystem;

std::string INPUT_FILE;
const std::string OUTPUT_DIR = "export";

std::vector<uint8_t> render(const std::vector<uint8_t>& data, uint32_t offset, int w, int h) {
    std::vector<uint8_t> pixels(w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        size_t pos = offset + i * 2;
        if (pos + 1 >= data.size()) {
            pixels[i * 3] = pixels[i * 3 + 1] = pixels[i * 3 + 2] = 0;
            continue;
        }
        uint16_t word = data[pos] | (data[pos + 1] << 8);
        uint8_t r = ((word >> 11) & 0x1F) << 3;
        uint8_t g = ((word >>  5) & 0x3F) << 2;
        uint8_t b = ((word      ) & 0x1F) << 3;
        pixels[i * 3] = r;
        pixels[i * 3 + 1] = g;
        pixels[i * 3 + 2] = b;
    }
    return pixels;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    INPUT_FILE = argv[1];

    std::ifstream file(INPUT_FILE, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open " << INPUT_FILE << std::endl;
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    fs::create_directories(OUTPUT_DIR);

    int total = 0;
    for (const auto& block : BLOCKS) {
        total += std::get<1>(block);
    }

    std::cout << "Input:    " << INPUT_FILE << " (" << data.size() / 1024 / 1024 << " MB)" << std::endl;
    std::cout << "Output:  " << OUTPUT_DIR << "/" << std::endl;
    std::cout << "Blocks: " << BLOCKS.size() << std::endl;
    std::cout << "Total:  " << total << " images" << std::endl;
    std::cout << std::endl;

    int exported = 0;

    for (size_t block_idx = 0; block_idx < BLOCKS.size(); ++block_idx) {
        const auto& block = BLOCKS[block_idx];
        uint32_t block_off = std::get<0>(block);
        int block_count = std::get<1>(block);
        std::string block_label = std::get<2>(block);
        int W = std::get<3>(block);
        int H = std::get<4>(block);
        int frame_size = W * H * 2;

        std::cout << std::dec << "[" << std::setfill('0') << std::setw(2) << block_idx << "] " << block_label << "  (" << block_count << " img  " << W << "x" << H << "  @ 0x" << std::hex << block_off << std::dec << ")" << std::endl;

        for (int img_idx = 0; img_idx < block_count; ++img_idx) {
            uint32_t img_off = block_off + img_idx * frame_size;
            std::ostringstream oss;
            oss << std::setfill('0') << std::setw(4) << img_idx
                << "_"
                << std::hex << std::uppercase << std::setfill('0') << std::setw(7) << img_off
                << ".png";
            std::string filename = oss.str();
            fs::path filepath = fs::path(OUTPUT_DIR) / filename;

            auto pixels = render(data, img_off, W, H);
            if (!stbi_write_png(filepath.string().c_str(), W, H, 3, pixels.data(), W * 3)) {
                std::cerr << "Error writing " << filepath << std::endl;
            }
            exported++;
            std::cout << "     [" << std::setfill(' ') << std::setw(3) << img_idx + 1 << "/" << block_count << "] " << filename << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "Export completed: " << exported << " images in " << OUTPUT_DIR << "/" << std::endl;
    return 0;
}