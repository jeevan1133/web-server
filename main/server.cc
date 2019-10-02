#include "../lib/server.hpp"
#include "../lib/utility.hpp"

int main(int ac, char *av[])
{
    using utility::MT2;

    std::pair<std::ifstream, bool> ifs = utility::getConfFile(ac, av);
    if (ifs.second) {
        try {
            MT2 keyValue;
            keyValue = utility::parseConfFile(ifs.first);
            std::string portNumber = utility::searchForKey(keyValue, "[port]");
            boost::asio::io_context io;
            webServer::server s{io, portNumber};
            std::cout << "Server is running at " << portNumber << '\n';
            io.run();
        }
        catch (InvalidFile &e) {
            std::cout << e.what() << '\n'
                << e.getErrMsg() << '\n';
        }
        catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << "\n";
        }
    }
    else {
        std::cout << "failed" << '\n';
        std::exit(EXIT_FAILURE);
    }
}