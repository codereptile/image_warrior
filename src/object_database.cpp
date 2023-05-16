#include "object_database.h"

#include <context.h>
#include <numeric>
#include <filesystem>

Object::Object(Object::Type type, boost::filesystem::path path)
        : type_(type),
          path_(std::move(path)) {

}

bool Object::IsImageFile(const boost::filesystem::path &file_path) {
    boost::filesystem::path extension = file_path.extension();
    std::string ext_str = extension.string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);

    const std::vector<std::string> image_extensions = {
            ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".tif",
            ".ico", ".jfif", ".webp", ".svg", ".svgz", ".eps", ".pcx",
            ".raw", ".cr2", ".nef", ".orf", ".sr2", ".heif", ".pdf",
            ".ai", ".indd", ".qxd", ".qxp", ".jp2", ".jpx", ".j2k",
            ".j2c", ".wdp", ".hdp", ".exr"
    };

    return std::find(image_extensions.begin(), image_extensions.end(), ext_str) != image_extensions.end();
}

std::shared_ptr<Object> Object::Create(const boost::filesystem::path &path) {
    if (IsImageFile(path)) {
        return std::make_shared<ImageObject>(path);
    }
    return nullptr;
}

ImageObject::ImageObject(boost::filesystem::path path)
        : Object(Type::IMAGE, std::move(path)) {

}

float similarity(const Object &a, const Object &b) {
    if (a.type_ != b.type_) {
        return 0.0f;
    } else {
        switch (a.type_) {
            case Object::Type::IMAGE: {
                const std::vector<float> &image_a_features = dynamic_cast<const ImageObject &>(a).features;
                const std::vector<float> &image_b_features = dynamic_cast<const ImageObject &>(b).features;
                if (image_a_features.size() != image_b_features.size()) {
                    throw std::invalid_argument("Vectors are of unequal length");
                }

                float dot_product = std::inner_product(image_a_features.begin(), image_a_features.end(),
                                                       image_b_features.begin(), 0.0f);
                float image_a_features_magnitude = std::sqrt(
                        std::inner_product(image_a_features.begin(), image_a_features.end(), image_a_features.begin(),
                                           0.0f));
                float image_b_features_magnitude = std::sqrt(
                        std::inner_product(image_b_features.begin(), image_b_features.end(), image_b_features.begin(),
                                           0.0f));

                return dot_product / (image_a_features_magnitude * image_b_features_magnitude);
            }
            default:
                return 0.0f;
        }
    }
}

ObjectDatabase::ObjectDatabase(Context &ctx, boost::filesystem::path dir)
        : ctx_(ctx),
          dir_(std::move(dir)) {
    if (!boost::filesystem::exists(dir_)) {
        throw std::runtime_error("Directory does not exist: " + dir_.generic_string());
    }
    if (!boost::filesystem::is_directory(dir_)) {
        throw std::runtime_error("Path is not a directory: " + dir_.generic_string());
    }
    // TODO: actually load database here
}

void ObjectDatabase::Update() {
    for (const auto &entry: boost::filesystem::recursive_directory_iterator(dir_)) {
        if (boost::filesystem::is_regular_file(entry)) {
            if (!contains(entry.path())) {
                if (auto object = Object::Create(entry.path())) {
                    objects_.push_back(std::move(object));
                } else {
                    spdlog::debug("Unrecognized file: {}", entry.path().generic_string());
                }
            }
        }
    }
    sort_objects();
}

const std::shared_ptr<Object> &ObjectDatabase::find_by_path(const boost::filesystem::path &path) const {
    for (const auto &object: objects_) {
        if (object->path_ == path) {
            return object;
        }
    }
    throw std::runtime_error("Object not found: " + path.generic_string());
}

bool ObjectDatabase::contains(const boost::filesystem::path &path) const {
    return std::ranges::any_of(objects_, [&path](const auto &object) {
        return object->path_ == path;
    });
}

size_t ObjectDatabase::size() const {
    return objects_.size();
}

const std::vector<std::shared_ptr<Object>> &ObjectDatabase::get_objects() const {
    return objects_;
}

std::vector<std::shared_ptr<Object>> ObjectDatabase::find_similar(
        const std::shared_ptr<Object> &object,
        float threshold) const {
    std::vector<std::shared_ptr<Object>> similar_objects;
    for (const auto &other_object: objects_) {
        if (object != other_object) {
            float similarity_value = similarity(*object, *other_object);
            if (similarity_value >= threshold) {
                similar_objects.push_back(other_object);
            }
        }
    }
    return similar_objects;
}

void ObjectDatabase::add_object(const std::shared_ptr<Object> &object) {
    objects_.push_back(object);
    sort_objects();
}

void ObjectDatabase::sort_objects() {
    std::sort(objects_.begin(), objects_.end(), [](const auto &a, const auto &b) {
        return a->path_.generic_string() < b->path_.generic_string();
    });
}

void ObjectDatabase::remove_object(const std::shared_ptr<Object> &object) {
    boost::filesystem::remove(object->path_);
    objects_.erase(std::remove(objects_.begin(), objects_.end(), object), objects_.end());
}

void copy_object(ObjectDatabase &from, ObjectDatabase &to, const std::shared_ptr<Object> &object) {
    if (from.contains(object->path_)) {
        to.add_object(object);
        from.remove_object(object);
        auto new_path = to.dir_ / object->path_.filename();
        if (boost::filesystem::exists(new_path)) {
            spdlog::warn("File already exists: {}", new_path.generic_string());
            size_t i = 1;
            do {
                new_path = to.dir_ / (object->path_.stem().generic_string() + " (" + std::to_string(i) + ")" +
                                      object->path_.extension().generic_string());
                i++;
            } while (boost::filesystem::exists(new_path));
        }
        std::filesystem::copy(object->path_.generic_string(), new_path.generic_string());
    } else {
        throw std::runtime_error("Object not found in database: " + object->path_.generic_string());
    }
}

void move_object(ObjectDatabase &from, ObjectDatabase &to, const std::shared_ptr<Object> &object) {
    if (from.contains(object->path_)) {
        auto new_path = to.dir_ / object->path_.filename();
        if (boost::filesystem::exists(new_path)) {
            spdlog::warn("File already exists: {}", new_path.generic_string());
            size_t i = 1;
            do {
                new_path = to.dir_ / (object->path_.stem().generic_string() + " (" + std::to_string(i) + ")" +
                                      object->path_.extension().generic_string());
                i++;
            } while (boost::filesystem::exists(new_path));
        }
        std::filesystem::rename(object->path_.generic_string(), new_path.generic_string());
        from.remove_object(object);

        object->path_ = new_path;
        to.add_object(object);
    } else {
        throw std::runtime_error("Object not found in database: " + object->path_.generic_string());
    }
}
