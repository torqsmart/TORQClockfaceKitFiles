#include "torqDialMake.h"
#include "compression/lz_util.h"

#include <iostream>

TorqDialMake::TorqDialMake()
{
}

TorqDialMake::~TorqDialMake()
{
}

bool TorqDialMake::createDial(const std::string &sourceFolder, const std::string &outputTqx)
{
    std::cout << "Packing watchface folder: " << sourceFolder << std::endl;
    std::cout << "Writing output .tqx file: " << outputTqx << std::endl;

    LZUtil packer;
    packer.setCompressionLevel(1);
    bool ok = packer.packWatchfaceFolder(sourceFolder, outputTqx);

    if (!ok)
    {
        std::cerr << "Failed to create .tqx watchface." << std::endl;
        return false;
    }

    packer.displayCompressionStats();
    return true;
}
