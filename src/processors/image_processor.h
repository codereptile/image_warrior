#pragma once

#include <torch/torch.h>
#include <torch/script.h>
#include <opencv2/opencv.hpp>
#include <boost/filesystem.hpp>

#include <processors/processor.h>
#include <utils.h>

class Context;

class ImageProcessor : public Processor {
public:
    explicit ImageProcessor(Context &ctx);

    void Process(const ObjectDatabase &db) override;

private:
    void reset();

    static cv::Mat LoadImage(const boost::filesystem::path &file_path);

    void ImageProcessingThread(const ObjectDatabase &db);

    void ImageLoaderThread();

    Context &ctx_;

    torch::Device device_;
    std::shared_ptr<torch::jit::script::Module> model_;

    // Settings:
    size_t threads_;
    size_t batch_size_limit_;

    // Inner stuff:

    bool continue_processing_;
    size_t processed_images_count_;

    std::vector<boost::filesystem::path> paths_to_process_;
    size_t paths_to_process_index_;
    std::mutex paths_to_process_mutex_;

    std::vector<boost::filesystem::path> batch_paths_;
    std::vector<torch::Tensor> batch_tensors_;
    std::mutex batch_mutex_;
};