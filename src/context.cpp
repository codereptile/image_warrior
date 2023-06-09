#include "context.h"

#include <processors/image_processor.h>

Context::Context(const std::string &config_path) {
    try {
        boost::property_tree::json_parser::read_json(config_path, config_tree_);
    } catch (const boost::property_tree::json_parser_error &e) {
        std::cout << "Failed to parse config file: " << config_path << ": " << e.what() << std::endl;
        exit(1);
    }

    spdlog::set_pattern(config_tree_.get<std::string>("log_pattern"));
    spdlog::set_level(spdlog::level::from_str(config_tree_.get<std::string>("log_level")));
}

ObjectDatabase &Context::get_input_database() {
    return *input_db_;
}

ObjectDatabase &Context::get_output_database() {
    return *output_db_;
}

const boost::property_tree::ptree &Context::get_config_tree() const {
    return config_tree_;
}

void Context::load_databases() {
    spdlog::info("Loading input database...");
    try {
        input_db_ = std::make_shared<ObjectDatabase>(*this, config_tree_.get<std::string>("input_dir"));
    } catch (const std::runtime_error &e) {
        std::cout << std::endl << "Failed to load input database: " << e.what() << std::endl;
        exit(1);
    }
    spdlog::info("Input database loaded, size: {}", input_db_->size());

    spdlog::info("Loading output database...");
    if (!boost::filesystem::exists(config_tree_.get<std::string>("output_dir"))) {
        boost::filesystem::create_directory(config_tree_.get<std::string>("output_dir"));
    }
    try {
        output_db_ = std::make_shared<ObjectDatabase>(*this, config_tree_.get<std::string>("output_dir"));
    } catch (const std::runtime_error &e) {
        std::cout << std::endl << "Failed to load output database: " << e.what() << std::endl;
        exit(1);
    }
    spdlog::info("Output database loaded, size: {}", output_db_->size());
}

void Context::update_databases() {
    spdlog::info("Updating input database...");
    input_db_->Update();
    spdlog::info("Input database updated, size: {}", input_db_->size());

    spdlog::info("Updating output database...");
    output_db_->Update();
    spdlog::info("Output database updated, size: {}", output_db_->size());
}

void Context::initialize_processors() {
    spdlog::info("Initializing processors...");

    if (config_tree_.get<bool>("image_processor.enabled")) {
        spdlog::info("Initializing image processor...");
        spdlog::info("Using model: {}", boost::filesystem::absolute(
                config_tree_.get<std::string>("image_processor.model_path")).generic_string());
        processors_.emplace_back(std::make_shared<ImageProcessor>(*this));
        spdlog::info("Image processor initialized");
    }

    spdlog::info("Processors initialized");
}

void Context::process_databases() {
    spdlog::info("Processing input database...");

    for (const auto &processor: processors_) {
        spdlog::info("Processing input database with processor: {}", processor->GetName());
        processor->Process(get_input_database());
    }

    spdlog::info("Input database processed");

    spdlog::info("Processing output database...");
    for (const auto &processor: processors_) {
        spdlog::info("Processing output database with processor: {}", processor->GetName());
        processor->Process(get_output_database());
    }

    spdlog::info("Output database processed");
}
