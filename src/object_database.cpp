#include "object_database.h"

#include <context.h>

Object::Object(Object::Type type, boost::filesystem::path path)
        : type_(type),
          path_(std::move(path)),
          hash_(0) {

}

bool Object::IsImageFile(const boost::filesystem::path &file_path) {
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

std::shared_ptr<Object> Object::Create(const boost::filesystem::path &path) {
    if (IsImageFile(path)) {
        return std::make_shared<ImageObject>(path);
    }
    return nullptr;
}

ImageObject::ImageObject(boost::filesystem::path path)
        : Object(Type::IMAGE, std::move(path)) {

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
            if (!Contains(entry.path())) {
                if (auto object = Object::Create(entry.path())) {
                    objects_.push_back(std::move(object));
                } else {
                    spdlog::debug("Skipping file: {}", entry.path().generic_string());
                }
            }
        }
    }
}

const std::shared_ptr<Object> &ObjectDatabase::Find(const boost::filesystem::path &path) const {
    for (const auto &object: objects_) {
        if (object->path_ == path) {
            return object;
        }
    }
    throw std::runtime_error("Object not found: " + path.generic_string());
}

bool ObjectDatabase::Contains(const boost::filesystem::path &path) const {
    return std::ranges::any_of(objects_, [&path](const auto &object) {
        return object->path_ == path;
    });
}

size_t ObjectDatabase::Size() const {
    return objects_.size();
}

const std::vector<std::shared_ptr<Object>> &ObjectDatabase::Objects() const {
    return objects_;
}


