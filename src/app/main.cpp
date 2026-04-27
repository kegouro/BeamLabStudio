#include "app/ApplicationBootstrap.h"

int main(int argc, char** argv)
{
    beamlab::app::ApplicationBootstrap bootstrap;
    return bootstrap.run(argc, argv);
}
