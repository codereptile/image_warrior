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

float similarity(const Object &a, const Object &b);

class ObjectDatabase {
public:
    ObjectDatabase(Context &ctx, boost::filesystem::path dir);

    void Update();

    [[nodiscard]] const std::shared_ptr<Object> &find_by_path(const boost::filesystem::path &path) const;

    [[nodiscard]] bool contains(const boost::filesystem::path &path) const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] const std::vector<std::shared_ptr<Object>> &get_objects() const;

    [[nodiscard]] std::vector<std::shared_ptr<Object>>
    find_similar(const std::shared_ptr<Object> &object, float threshold) const;

    void add_object(const std::shared_ptr<Object> &object);

    void remove_object(const std::shared_ptr<Object> &object);

    friend void copy_object(ObjectDatabase &from, ObjectDatabase &to, const std::shared_ptr<Object> &object);

    friend void move_object(ObjectDatabase &from, ObjectDatabase &to, const std::shared_ptr<Object> &object);

private:

    void sort_objects();

    Context &ctx_;
    boost::filesystem::path dir_;

    std::vector<std::shared_ptr<Object>> objects_;
};

