#pragma once

#include <memory>
#include <string>
#include <set>
#include <boost/filesystem.hpp>
#include <utility>

class Context;

class Object {
public:
    enum class Type {
        UNDEFINED,
        IMAGE,
    };

    Type type_;
    boost::filesystem::path path_;
    uint64_t hash_;

    Object(Type type, boost::filesystem::path path);

    virtual ~Object() = default;

    bool operator==(const Object &other) = delete;

    bool operator!=(const Object &other) = delete;

    static bool IsImageFile(const boost::filesystem::path &file_path);

    static std::shared_ptr<Object> Create(const boost::filesystem::path &path);
};

class ImageObject : public Object {
public:
    explicit ImageObject(boost::filesystem::path path);

    std::vector<float> features;
};

class ObjectDatabase {
public:
    ObjectDatabase(Context &ctx, boost::filesystem::path dir);

    void Update();

    [[nodiscard]] const std::shared_ptr<Object> &Find(const boost::filesystem::path &path) const;

    [[nodiscard]] bool Contains(const boost::filesystem::path &path) const;

    [[nodiscard]] size_t Size() const;

    [[nodiscard]] const std::vector<std::shared_ptr<Object>> &Objects() const;

private:
    Context &ctx_;
    boost::filesystem::path dir_;

    std::vector<std::shared_ptr<Object>> objects_;
};