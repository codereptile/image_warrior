#pragma once

#include <iostream>

#include <boost/property_tree/ptree.hpp>

// TODO: remove this line when upgrading to boost 1.73
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/property_tree/json_parser.hpp>

#include <spdlog/spdlog.h>

#include <utils.h>
#include <object_database.h>
#include <processors/processor.h>

class Context {
public:
    explicit Context(const std::string &config_path);

    ObjectDatabase &GetInputDatabase();

    ObjectDatabase &GetOutputDatabase();

    [[nodiscard]] const boost::property_tree::ptree &GetConfigTree() const;

    void LoadDatabases();

    void UpdateDatabases();

    void InitializeProcessors();

    void ProcessDatabases();

private:
    boost::property_tree::ptree config_tree_;

    std::shared_ptr<ObjectDatabase> input_db_;
    std::shared_ptr<ObjectDatabase> output_db_;

    std::vector<std::shared_ptr<Processor>> processors_;
};
