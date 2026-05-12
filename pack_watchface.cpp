#include "torqDialMake.h"
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage: pack_watchface <watchface-folder> <output-file.tqx>\n";
        return 1;
    }

    const std::string sourceFolder = argv[1];
    const std::string outputTqx = argv[2];

    TorqDialMake maker;
    if (!maker.createDial(sourceFolder, outputTqx))
    {
        std::cerr << "Failed to create watchface .tqx file." << std::endl;
        return 1;
    }

    return 0;
}
