#pragma once

#include <opencv2/opencv.hpp>
#include <boost/filesystem/path.hpp>

cv::Mat LoadImage(
        const boost::filesystem::path &file_path
);

bool IsImageFile(
        const boost::filesystem::path &file_path
);