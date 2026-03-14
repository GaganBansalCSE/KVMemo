#include <iostream>
#include <string>

#include "server/server_app.h"

using namespace kvmemo;

int main(int argc, char* argv[])
{
    int port = 6379;

    if (argc >= 2)
    {
        port = std::stoi(argv[1]);
    }

    std::cout << "Starting KVMemo Server..." << std::endl;
    std::cout << "Listening on port " << port << std::endl;

    try
    {
        /**
         * ------------------------------------------------------------
         * Server Application
         * ------------------------------------------------------------
         */

        server::ServerApp server(port);

        /**
         * ------------------------------------------------------------
         * Start Server Loop
         * ------------------------------------------------------------
         */

        server.Run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Fatal server error: "
                  << ex.what()
                  << std::endl;

        return 1;
    }

    return 0;
}