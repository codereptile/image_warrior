#include <iostream>

#include <context.h>

int main() {
    Context context("config.json");
    context.LoadDatabases();
    context.UpdateDatabases();
    context.InitializeProcessors();
    context.ProcessDatabases();
}