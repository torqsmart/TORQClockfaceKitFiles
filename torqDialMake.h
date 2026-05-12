#ifndef TORQDIALMAKE_H
#define TORQDIALMAKE_H

#include <string>

class TorqDialMake
{
public:
    TorqDialMake();
    ~TorqDialMake();

    bool createDial(const std::string &sourceFolder, const std::string &outputTqx);
};

#endif // TORQDIALMAKE_H
