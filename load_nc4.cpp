#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <netcdf.h>
#include <opencv2/opencv.hpp>

// Error handling macro for NetCDF calls
#define NC_CHECK(e) {if(e) {printf("NetCDF Error: %s\n", nc_strerror(e)); return 1;}}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <path_to_nc4> <variable_name>" << std::endl;
        return 1;
    }

    const char* file_path = argv[1];
    const char* var_name = argv[2];
    int ncid, varid;

    // 1. Open the NetCDF file in read-only mode
    NC_CHECK(nc_open(file_path, NC_NOWRITE, &ncid));

    // 2. Get the ID for the specific variable (e.g., "temperature" or "image_data")
    NC_CHECK(nc_inq_varid(ncid, var_name, &varid));

    // 3. Verify dimensions (expecting time=205, y=200, x=200)
    int ndims;
    int dimids[3];
    size_t dim_lens[3];
    NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
    NC_CHECK(nc_inq_vardimid(ncid, varid, dimids));

    if (ndims != 3) {
        std::cerr << "Error: Expected 3 dimensions (time, y, x), found " << ndims << std::endl;
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        NC_CHECK(nc_inq_dimlen(ncid, dimids[i], &dim_lens[i]));
    }

    size_t num_frames = dim_lens[0]; // time
    int rows = (int)dim_lens[1];     // y
    int cols = (int)dim_lens[2];     // x

    std::printf("Found Variable: %s [%zu x %d x %d]\n", var_name, num_frames, rows, cols);
    std::cout << "Starting 16-bit (ushort) sequential load benchmark...\n" << std::endl;

    // 4. Sequential Loading Loop
    double total_time = 0;
    size_t start[] = {0, 0, 0};
    size_t count[] = {1, (size_t)rows, (size_t)cols};

    for (size_t t = 0; t < num_frames; ++t) {
        start[0] = t; // Set current "time" slice

        // Pre-allocate 16-bit Unsigned Single Channel Matrix
        cv::Mat img(rows, cols, CV_16UC1);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Read the 16-bit data directly into memory buffer
        int status = nc_get_vara_ushort(ncid, varid, start, count, (unsigned short*)img.data);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end_time - start_time;
        
        if (status == NC_NOERR) {
            total_time += duration.count();
            // Printing every 10th frame to avoid console I/O bottlenecking the benchmark
//            if (t % 10 == 0 || t == num_frames - 1) {
//               std::printf("[OK] Frame %-4zu | Time: %.4f ms\n", t, duration.count());
//            }
        } else {
            std::printf("[FAIL] Error at frame %zu: %s\n", t, nc_strerror(status));
            break;
        }
    }

    // 5. Final Statistics
    std::cout << "\n--- Benchmark Results ---" << std::endl;
    std::printf("Average Load Time: %.4f ms per frame\n", total_time / num_frames);
  //  std::printf("Total Time for %zu frames: %.2f ms\n", num_frames, total_time);

    nc_close(ncid);
    return 0;
}
