#include "services/video/VideoThumbnailer.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

void printUsage(const char *programName) {
  std::cout << "Usage: " << programName
            << " <video_file> [output_path] [time_seconds]\n";
  std::cout << "  video_file    - Path to the video file\n";
  std::cout << "  output_path   - Output image path (default: thumbnail.ppm)\n";
  std::cout
      << "  time_seconds  - Time in seconds to extract frame (default: 10.0)\n";
  std::cout << "\nExample: " << programName << " video.mp4 preview.ppm 15.5\n";
}

bool fileExists(const std::string &path) {
  return fs::exists(path) && fs::is_regular_file(path);
}

int main(int argc, char *argv[]) {
  std::string videoPath;
  std::string outputPath = "thumbnail.ppm";
  double timeInSeconds = 10.0;

  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  videoPath = argv[1];

  if (argc >= 3) {
    outputPath = argv[2];
  }

  if (argc >= 4) {
    try {
      timeInSeconds = std::stod(argv[3]);
    } catch (const std::exception &e) {
      std::cerr << "Invalid time format: " << argv[3] << "\n";
      return 1;
    }
  }

  if (!fileExists(videoPath)) {
    std::cerr << "Error: Video file not found: " << videoPath << "\n";
    return 1;
  }

  std::cout << "========================================\n";
  std::cout << "Video Thumbnailer Test\n";
  std::cout << "========================================\n";
  std::cout << "Video file:   " << videoPath << "\n";
  std::cout << "Output file:  " << outputPath << "\n";
  std::cout << "Time:         " << timeInSeconds << " seconds\n";
  std::cout << "========================================\n\n";

  VideoThumbnailer thumbnailer;

  auto startTime = std::chrono::high_resolution_clock::now();

  bool success =
      thumbnailer.extractThumbnail(videoPath, outputPath, timeInSeconds);

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime);

  std::cout << "\n========================================\n";
  if (success) {
    std::cout << "✓ Thumbnail extraction SUCCESSFUL\n";
    std::cout << "  Output file: " << outputPath << "\n";

    if (fileExists(outputPath)) {
      auto fileSize = fs::file_size(outputPath);
      std::cout << "  File size:   " << fileSize << " bytes\n";
    }
  } else {
    std::cout << "✗ Thumbnail extraction FAILED\n";
  }
  std::cout << "  Time:        " << duration.count() << " ms\n";
  std::cout << "========================================\n";

  return success ? 0 : 1;
}
