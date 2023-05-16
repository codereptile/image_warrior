#include "image_processor.h"

#include <context.h>
#include <object_database.h>

ImageProcessor::ImageProcessor(Context &ctx)
        : Processor("Image Processor"),
          ctx_(ctx),
          device_(torch::cuda::is_available() ? torch::kCUDA : torch::kCPU),
          model_(std::make_shared<torch::jit::script::Module>(
                  torch::jit::load(ctx_.GetConfigTree().get<std::string>("image_processor.model_path"), device_))),
          threads_(ctx_.GetConfigTree().get<size_t>("image_processor.threads")),
          batch_size_limit_(ctx_.GetConfigTree().get<size_t>("image_processor.batch_size_limit")),
          continue_processing_(true),
          processed_images_count_(0),
          paths_to_process_index_(0) {
    model_->eval();
}

void ImageProcessor::Process(const ObjectDatabase &db) {
    StderrSuppressor stderr_suppressor;

    for (const auto &object: db.Objects()) {
        if (object->type_ == Object::Type::IMAGE) {
            paths_to_process_.push_back(object->path_);
        }
    }

    std::vector<std::thread> loading_threads;
    for (size_t i = 0; i < threads_; ++i) {
        loading_threads.emplace_back(&ImageProcessor::ImageLoaderThread, this);
    }

    std::thread processing_thread(&ImageProcessor::ImageProcessingThread, this, std::ref(db));

    for (auto &thread: loading_threads) {
        thread.join();
    }

    continue_processing_ = false;

    processing_thread.join();
}

cv::Mat ImageProcessor::LoadImage(const boost::filesystem::path &file_path) {
    auto image = cv::imread(file_path.string());
    if (image.empty()) {
        throw std::runtime_error("Image is empty!");
    }
    return image;
}

void ImageProcessor::ImageProcessingThread(const ObjectDatabase &db) {
    processed_images_count_ = 0;
    while (true) {
        std::vector<torch::Tensor> tensors;
        std::vector<boost::filesystem::path> paths;

        {
            std::lock_guard<std::mutex> lock(batch_mutex_);
            tensors.swap(batch_tensors_);
            paths.swap(batch_paths_);
        }

        if (tensors.empty()) {
            if (!continue_processing_) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::cout.flush();

        torch::Tensor input_tensor = torch::stack(tensors).contiguous().to(device_);

        // Process the images with the model
        std::vector<torch::jit::IValue> input = {input_tensor};
        torch::NoGradGuard no_grad;
        torch::Tensor output = model_->forward(input).toTensor();

        // Convert the output tensor to a std::vector<float> and return it
        output = output.to(torch::kCPU);

        for (int i = 0; i < output.size(0); i++) {
            torch::Tensor single_output = output[i];

            auto image_object = std::dynamic_pointer_cast<ImageObject>(db.Find(paths[i]));
            if (!image_object) {
                throw std::runtime_error("Failed to find image object: " + paths[i].string());
            }

            image_object->features = std::vector<float>(single_output.data_ptr<float>(),
                                                        single_output.data_ptr<float>() + single_output.numel());
        }

        processed_images_count_ += paths.size();

        spdlog::info("Processed {} images\033[A", processed_images_count_);
    }

    spdlog::info("Processed {} images", processed_images_count_);
}

void ImageProcessor::ImageLoaderThread() {
    boost::filesystem::path image_path;
    while (true) {
        // check if we can load more images:
        {
            std::lock_guard<std::mutex> lock(paths_to_process_mutex_);
            if (paths_to_process_index_ >= paths_to_process_.size()) {
                break;
            }
            image_path = paths_to_process_[paths_to_process_index_++];
        }

        cv::Mat resized_image;

        // load image:
        try {
            auto image = LoadImage(image_path);
            cv::resize(image, resized_image, cv::Size(224, 224));
        } catch (const std::exception &e) {
            spdlog::warn("Failed to load image {}: {}", image_path.string(), e.what());
            // Update the progress bar, so it's always visible:
            spdlog::info("Processed {} images\033[A", processed_images_count_);
            continue;
        }

        // Convert to tensor
        torch::Tensor image_tensor = torch::from_blob(resized_image.data,
                                                      {resized_image.rows, resized_image.cols, 3},
                                                      torch::kByte);
        image_tensor = image_tensor.permute({2, 0, 1}); // Rearrange dimensions to CxHxW

        // Normalize
        image_tensor = image_tensor.to(torch::kFloat32).div(255);
        image_tensor[0] = image_tensor[0].sub_(0.485).div_(0.229);
        image_tensor[1] = image_tensor[1].sub_(0.456).div_(0.224);
        image_tensor[2] = image_tensor[2].sub_(0.406).div_(0.225);

        // Add to batch, but wait if the batch is full:
        while (true) {
            batch_mutex_.lock();
            if (batch_paths_.size() >= batch_size_limit_) {
                batch_mutex_.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            batch_paths_.push_back(image_path);
            batch_tensors_.push_back(image_tensor);
            batch_mutex_.unlock();
            break;
        }
    }
}
