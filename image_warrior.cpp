#include <iostream>
#include <format>

#include <context.h>

int main() {
    Context context("config.json");
    context.LoadDatabases();
    context.UpdateDatabases();
    context.InitializeProcessors();
    context.ProcessDatabases();

//    for (const auto &object: context.GetInputDatabase().GetObjects()) {
//        if (object->type_ == Object::Type::IMAGE) {
//            auto& image = dynamic_cast<ImageObject&>(*object);
//            std::cout << image.features.size() << "\n";
//        }
//    }

    const auto &objects = context.GetInputDatabase().GetObjects();
    for (auto &object: objects) {
        std::cout << object->path_.generic_string() << "\n";
    }

    for (int i = 0; i < objects.size(); ++i) {
        for (int j = 0; j < objects.size(); ++j) {
            std::cout << std::format("{:.2f} ", Similarity(*objects[i], *objects[j]));
        }
        std::cout << "\n";
    }
}