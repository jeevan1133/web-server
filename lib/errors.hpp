#ifndef LIB_ERRORS_H
#define LIB_ERRORS_H

#include <stdexcept>
#include <string>
#include <sstream>

class FileNotFound : public std::runtime_error {
    public:
        FileNotFound() : std::runtime_error("File not found") {}
        FileNotFound(std::string msg, std::string fileName) : 
            std::runtime_error(msg.c_str())
    {
        this->m_msg = msg;
        this->fileName = fileName;
    }

        std::string getErrMsg() const 
        {
            std::stringstream buf;
            buf << "File " << this->fileName
                << " can not be found.";
            return buf.str();
        }
    private:
        std::string m_msg;
        std::string fileName;
};

class InvalidUsage : public std::runtime_error {
    public:
        InvalidUsage() : std::runtime_error("") {}
        InvalidUsage (std::string msg) : 
            std::runtime_error(msg.c_str())
    {
        this->m_msg = msg;

    }

        std::string getErrMsg() const
        {
            std::stringstream buf;
            buf << "Invalid use of the program. Use it with a "
                << "configuration file.\n"
                << "./[prog_name] configuration_file\n"
                << "Must include [Port]";
            return buf.str();
        }

    private:
        std::string m_msg;

};

class InvalidFile : public std::runtime_error {
    public:
        InvalidFile() : std::runtime_error("") {}
        InvalidFile (std::string msg) : 
            std::runtime_error(msg.c_str()) 
    {
        this->m_msg = msg;

    }

        std::string getErrMsg() const
        {
            std::stringstream buf;
            buf << "Invalid configuration file. Configuration file should be:\n"
                << "#This is a comment" << '\n'
                << "[port]" << '\n'
                << "port_number\n";
            return buf.str();
        }

    private:
        std::string m_msg;

};
#endif
