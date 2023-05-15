#include <iostream>

#include <vector>
#include <thread>
#include <boost/filesystem/path.hpp>
#include <torch/torch.h>
#include <torch/script.h>
#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>

struct StderrSuppressor {
    StderrSuppressor() {
        fflush(stderr);
        fd_ = dup(STDOUT_FILENO);
        auto file = freopen("/dev/null", "w", stderr);
        if (file == nullptr) {
            throw std::runtime_error("Failed to redirect stderr to /dev/null");
        }
    }

    ~StderrSuppressor() {
        fflush(stderr);
        dup2(fd_, fileno(stderr));
        close(fd_);
    }

private:
    int fd_;
};

struct ImageWarriorSettings {
    torch::Device device_;
    std::string model_path_;

    std::string input_dir_;
    std::string output_dir_;

    // Image processing settings:
    size_t threads_count_;
    size_t max_batch_size_;
};

struct ObjectInfo {
    enum class ObjectType {
        UNKNOWN,
        IMAGE,
    };

    ObjectInfo() : type(ObjectType::UNKNOWN) {}

    explicit ObjectInfo(ObjectType type) : type(type) {}

    virtual ~ObjectInfo() = default;

    ObjectType type;
};

struct ImageInfo : public ObjectInfo {
    ImageInfo() : ObjectInfo(ObjectType::IMAGE) {}

    std::vector<float> features;
};

class ObjectDatabase {
public:
    explicit ObjectDatabase() = default;

    class ObjectDatabaseIterator {
    public:
        explicit ObjectDatabaseIterator(
                std::map<boost::filesystem::path, std::shared_ptr<ObjectInfo>>::iterator iterator)
                : iterator_(iterator) {}

        ObjectDatabaseIterator operator++() {
            ++iterator_;
            return *this;
        } // Prefix increment
        ObjectDatabaseIterator operator++(int) {
            ObjectDatabaseIterator temp = *this;
            ++iterator_;
            return temp;
        } // Postfix increment

        bool operator==(const ObjectDatabaseIterator &rhs) const { return iterator_ == rhs.iterator_; }

        bool operator!=(const ObjectDatabaseIterator &rhs) const { return iterator_ != rhs.iterator_; }

        const boost::filesystem::path &operator*() { return iterator_->first; }

    private:
        std::map<boost::filesystem::path, std::shared_ptr<ObjectInfo>>::iterator iterator_;
    };

    ObjectDatabaseIterator begin() { return ObjectDatabaseIterator(objects_.begin()); }

    ObjectDatabaseIterator end() { return ObjectDatabaseIterator(objects_.end()); }

    std::shared_ptr<ObjectInfo> &GetObjectInfo(const boost::filesystem::path &path) {
        auto it = objects_.find(path);
        if (it == objects_.end()) {
            throw std::runtime_error("Object not found in database.");
        }
        return it->second;
    }

    void LoadFiles(const boost::filesystem::path &directory) {
        if (!boost::filesystem::exists(directory) || !boost::filesystem::is_directory(directory)) {
            throw std::runtime_error("Provided path is not a directory.");
        }

        for (const auto &entry: boost::filesystem::recursive_directory_iterator(directory)) {
            if (boost::filesystem::is_regular_file(entry)) {
                // TODO: bruh!
                objects_.emplace(entry.path(), std::make_shared<ObjectInfo>());
            }
        }
    }

private:
    std::map<boost::filesystem::path, std::shared_ptr<ObjectInfo>> objects_;
};

class ImageProcessor {
public:
    ImageProcessor(
            std::shared_ptr<ImageWarriorSettings> settings,
            std::shared_ptr<ObjectDatabase> object_database
    ) : settings_(std::move(settings)),
        model_(std::make_shared<torch::jit::script::Module>(
                torch::jit::load(settings_->model_path_, settings_->device_))),
        continue_processing_(true),
        paths_to_process_index_(0),
        object_database_(std::move(object_database)) {
        model_->eval();
    }

    void Process() {
        StderrSuppressor stderr_suppressor;

        for (const auto &path: *object_database_) {
            if (IsImageFile(path)) {
                paths_to_process_.push_back(path);
                object_database_->GetObjectInfo(path) = std::make_shared<ImageInfo>();
            }
        }

        std::vector<std::thread> loading_threads;
        for (size_t i = 0; i < settings_->threads_count_; ++i) {
            loading_threads.emplace_back(&ImageProcessor::ImageLoaderThread, this);
        }

        std::thread processing_thread(&ImageProcessor::ImageProcessingThread, this);

        for (auto &thread: loading_threads) {
            thread.join();
        }

        continue_processing_ = false;

        processing_thread.join();
    }

private:
    static cv::Mat LoadImage(const boost::filesystem::path &file_path) {
        auto image = cv::imread(file_path.string());
        if (image.empty()) {
            throw std::runtime_error("Failed to load image: " + file_path.string());
        }
        return image;
    }

    static bool IsImageFile(const boost::filesystem::path &file_path) {
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


    void ImageProcessingThread() {
        size_t processed_images_count = 0;

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

            torch::Tensor input_tensor = torch::stack(tensors).contiguous().to(settings_->device_);

            // Process the images with the model
            std::vector<torch::jit::IValue> input = {input_tensor};
            torch::NoGradGuard no_grad;
            torch::Tensor output = model_->forward(input).toTensor();

            // Convert the output tensor to a std::vector<float> and return it
            output = output.to(torch::kCPU);

            for (int i = 0; i < output.size(0); i++) {
                torch::Tensor single_output = output[i];

                auto object_info = object_database_->GetObjectInfo(paths[i]);
                auto image_info = std::dynamic_pointer_cast<ImageInfo>(object_info);
                if (!image_info) {
                    throw std::runtime_error("Object info is not an image info.");
                }

                image_info->features = std::vector<float>(single_output.data_ptr<float>(),
                                                          single_output.data_ptr<float>() + single_output.numel());
            }

            processed_images_count += paths.size();

            std::cout << "Processed " << processed_images_count << " images.                   \r" << std::flush;
        }

        std::cout << "Processed " << processed_images_count << " images.                   " << std::endl;
    }

    void ImageLoaderThread() {
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
                if (batch_paths_.size() >= settings_->max_batch_size_) {
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

    std::shared_ptr<ImageWarriorSettings> settings_;
    std::shared_ptr<ObjectDatabase> object_database_;

    std::shared_ptr<torch::jit::script::Module> model_;


    bool continue_processing_;

    std::vector<boost::filesystem::path> paths_to_process_;
    size_t paths_to_process_index_;
    std::mutex paths_to_process_mutex_;

    std::vector<boost::filesystem::path> batch_paths_;
    std::vector<torch::Tensor> batch_tensors_;
    std::mutex batch_mutex_;

};

int main() {
    auto settings = std::make_shared<ImageWarriorSettings>(
            ImageWarriorSettings{
                    torch::cuda::is_available() ? torch::kCUDA : torch::kCPU,
                    "models/resnet152_traced.pt",
                    "/mnt/stash/stash/ALL_PHOTOS/100CANON",
                    "./output",
                    20,
                    300
            }
    );

    auto object_database = std::make_shared<ObjectDatabase>();
    object_database->LoadFiles(settings->input_dir_);

    ImageProcessor image_processor(settings, object_database);
    image_processor.Process();
}