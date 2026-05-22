#include "src/Application.h"

int main(int argc, char* argv[])
{
    const strategic_nexus::Application app;
    return app.run(strategic_nexus::parseRunConfig(argc, argv));
}
