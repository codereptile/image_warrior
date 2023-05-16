#include <iostream>
#include <format>

#include <context.h>
#include <object_database.h>

std::string text_temperature(const std::string &input, float value) {
    if (value > 1) {
        value = 1.0f;
    }
    if (value < 0) {
        value = 0.0f;
    }

    float r, g, b;

    float h = 1.0f - value; // Convert the value to a hue (we reverse it to go from violet to red)
    h *= 0.5;
    float s = 1.0;
    float v = 1.0;

    int i = std::floor(h * 6);
    float f = h * 6 - static_cast<float>(i);
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
        case 0:
            r = v, g = t, b = p;
            break;
        case 1:
            r = q, g = v, b = p;
            break;
        case 2:
            r = p, g = v, b = t;
            break;
        case 3:
            r = p, g = q, b = v;
            break;
        case 4:
            r = t, g = p, b = v;
            break;
        case 5:
            r = v, g = p, b = q;
            break;
    }

    std::ostringstream oss;
    oss << "\033[38;2;" << static_cast<int>(r * 255) << ";"
        << static_cast<int>(g * 255) << ";"
        << static_cast<int>(b * 255) << "m"
        << input << "\033[0m";
    return oss.str();
}


std::string center(const std::string &input, int block_size) {
    int padding = block_size - static_cast<int>(input.size());
    if (padding < 0) {
        return input;
    }

    int left_padding = padding / 2;
    int right_padding = padding - left_padding;

    std::string result;
    result.reserve(block_size);
    for (int i = 0; i < left_padding; ++i) {
        result += " ";
    }
    result += input;
    for (int i = 0; i < right_padding; ++i) {
        result += " ";
    }
    return result;
}

int main() {
    auto start_time = std::chrono::high_resolution_clock::now();
    Context context("config.json");
    context.load_databases();
    context.update_databases();
    context.initialize_processors();
    context.process_databases();
    auto end_time = std::chrono::high_resolution_clock::now();
    long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    spdlog::info("Done preprocessing in {}s", static_cast<double>(elapsed_ms) / 1000.0);

    spdlog::info("Copying unique objects...");
    start_time = std::chrono::high_resolution_clock::now();
    int copy_count = 0;
    auto objects = context.get_input_database().get_objects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto &similar_objects = context.get_output_database().find_similar(objects[i], 0.999);
        if (similar_objects.empty()) {
            copy_object(context.get_input_database(), context.get_output_database(), objects[i]);
            ++copy_count;
            spdlog::debug("Copying {}", objects[i]->path_.generic_string());
        }
        spdlog::info("Processed {}/{} objects\033[A", i + 1, objects.size());
    }
    spdlog::info("Processed {}/{} objects", objects.size(), objects.size());
    end_time = std::chrono::high_resolution_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    spdlog::info("Done copying unique objects in {}s", static_cast<double>(elapsed_ms) / 1000.0);
    spdlog::info("Copied {} objects", copy_count);

//    const auto &objects = context.get_input_database().get_objects();
//    for (int i = 0; i < objects.size(); ++i) {
//        std::cout << std::format("{:^3}: {}\n", i, objects[i]->path_.generic_string());
//        for (auto &object : context.get_input_database().find_similar(objects[i], 0.9)) {
//            std::cout << std::format("\t{}\n", object->path_.generic_string());
//        }
//    }
//
//    const int block_size = 8;
//    for (int i = 0; i < objects.size(); ++i) {
//        if (i == 0) {
//            std::cout << center("", block_size) << " ";
//            for (int j = 0; j < objects.size(); ++j) {
//                std::cout << center(std::to_string(j + 1), block_size) << " ";
//            }
//            std::cout << "\n";
//        }
//        std::cout << center(std::to_string(i + 1), block_size) << " ";
//        for (int j = 0; j < objects.size(); ++j) {
//            const auto similarity_value = similarity(*objects[i], *objects[j]);
//            std::cout << text_temperature(center(std::format("{:.4f}", similarity_value), block_size), similarity_value) << " ";
//        }
//        std::cout << "\n";
//    }
}