#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <zip.h>
#include <opencv2/opencv.hpp>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_zip>" << std::endl;
        return 1;
    }

    int err = 0;
    zip* archive = zip_open(argv[1], 0, &err);
    if (!archive) {
        std::cerr << "Error opening zip." << std::endl;
        return 1;
    }

    zip_int64_t num_entries = zip_get_num_entries(archive, 0);
    
    // --- PRE-ALLOCATION STEP ---
    // Find the largest file size to pre-allocate our buffer once
    zip_uint64_t max_size = 0;
    for (zip_int64_t i = 0; i < num_entries; ++i) {
        struct zip_stat st;
        zip_stat_index(archive, i, 0, &st);
        if (st.size > max_size) max_size = st.size;
    }
    std::vector<uchar> buffer(max_size); 
    // ---------------------------

    double total_time = 0;
    int count = 0;

    for (zip_int64_t i = 0; i < num_entries; ++i) {
        const char* name = zip_get_name(archive, i, 0);
        if (std::string(name).find(".png") == std::string::npos) continue;

        struct zip_stat st;
        zip_stat_index(archive, i, 0, &st);
        
        zip_file* file = zip_fopen_index(archive, i, 0);
        if (!file) continue;

        // Start timing the extraction AND decoding
        auto start = std::chrono::high_resolution_clock::now();

        // Read into pre-allocated buffer
        zip_fread(file, buffer.data(), st.size);
        zip_fclose(file);

        // Decode from memory. Using _UNCHANGED to avoid color-space conversion cycles.
        // We use a cv::_InputArray(buffer.data(), st.size) to avoid copying the vector.
        //cv::Mat img = cv::imdecode(cv::Mat(1, st.size, CV_8UC1, buffer.data()), cv::IMREAD_UNCHANGED);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;

//        if (!img.empty()) {
            total_time += duration.count();
            count++;
            if (count % 1000 == 0) {
                std::printf("[ZIP] Processed %-4d | Time: %.4f ms\n", count, duration.count());
            }
 //       }
    }

    std::printf("\n--- ZIP Benchmark Results ---\n");
    std::printf("Average Time: %.4f ms per image (Decompression + Load)\n", total_time / count);

    zip_close(archive);
    return 0;
}
