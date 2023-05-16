#pragma once

#include <iostream>
#include <unistd.h>
#include <stack>
#include <format>
#include <boost/property_tree/ptree.hpp>

void PrintConfigTree(const boost::property_tree::ptree &tree, int indent);


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