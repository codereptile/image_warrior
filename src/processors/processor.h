#pragma once

#include <string>

class ObjectDatabase;

class Processor {
public:
    Processor() : name_("Undefined Processor") {}

    Processor(const std::string &name) : name_(name) {}

    [[nodiscard]] const std::string &GetName() const {
        return name_;
    }

    virtual void Process(const ObjectDatabase &db) = 0;

    virtual ~Processor() = default;

private:
    std::string name_;
};

class ImageProcessor;