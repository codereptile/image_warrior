#include "file_utils.h"

cv::Mat LoadImage(const boost::filesystem::path &file_path) {
    auto image = cv::imread(file_path.string());
    if (image.empty()) {
        throw std::runtime_error("Failed to load image: " + file_path.string());
    }
    return image;
}

bool IsImageFile(const boost::filesystem::path &file_path) {
    boost::filesystem::path extension = file_path.extension();
    std::string ext_str = extension.string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);

    const std::vector<std::string> image_extensions = {
            ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".tif",
            ".ico", ".jfif", ".webp", ".svg", ".svgz", ".eps", ".pcx",
            ".raw", ".cr2", ".nef", ".orf", ".sr2", ".heic", ".heif",
            ".pdf", ".ai", ".indd", ".qxd", ".qxp", ".jp2", ".jpx",
            ".j2k", ".j2c", ".wdp", ".hdp", ".exr", ".dng", ".cr3"
    };

    return std::find(image_extensions.begin(), image_extensions.end(), ext_str) != image_extensions.end();
}
