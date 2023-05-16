#include "utils.h"

void PrintConfigTree(const boost::property_tree::ptree &tree, int indent) {
    const std::string indentation(indent * 2, ' ');

    for (const auto &node: tree) {
        const auto &key = node.first;
        const auto &value = node.second;

        if (value.empty()) {
            std::cout << indentation << key << ": " << value.get_value<std::string>() << std::endl;
        } else {
            std::cout << indentation << key << ":" << std::endl;
            PrintConfigTree(value, indent + 1);
        }
    }
}
