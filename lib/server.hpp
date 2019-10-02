#ifndef LIB_SERVER_H
#define LIB_SERVER_H

#include "errors.hpp"
#include <fstream>
#include <iostream>
#include <utility>
#include <string>
#include <boost/asio.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/lexical_cast.hpp>
#include <regex>
#include <list>


namespace webServer {
    using boost::asio::ip::tcp;

    class session : public std::enable_shared_from_this<session> {
        /* We could have these private variables in a different class
         * for they are used to get the request header of the client.
         */
        boost::asio::streambuf buff;    //buff for request header
        std::string method;             // the current method: GET POST DELETE
        std::string url;                // the url that's accessed
        std::string version;            // HTTP/1.1 only
        std::map<std::string, std::string> headers; //maps key to value of request header
        boost::asio::io_context::strand writeStrand;
        boost::asio::io_context &io_service;
        tcp::socket socket_;
        //std::deque<std::string> outgoing_queue;
        std::deque<void*> outgoing_queue;
        std::deque<std::size_t> outgoing_queue_length;
        boost::interprocess::mapped_region region_;

        /*
         *  This function reads every header seperated by '\r'
         *  and inserts the key value pair in the map
         *  @param: a string delimited by '\r'
         *  @return: None
         */
        void onReadHeader(const std::string &line) {
            auto self(shared_from_this());
            if (line.empty()) {
                return;
            }
            std::stringstream ssHeader(line);
            std::string headerName;
            std::getline(ssHeader, headerName, ':');

            std::string value;
            std::getline(ssHeader, value);
            headers.insert({headerName, value});
        }

        /*
         * Checks whether the client requested for any range
         * as specified in HTTP/1.1
         * @param: None
         * @return: true if the request header contains 'Range'
         */
        std::string headerContainsRange() {
            auto self(shared_from_this());
            auto found(headers.find("Range"));
            if (found != headers.end()) {
                return found->second;
            }
            return {};
        }

        /*
         * Returns the response header to be sent
         * @param: None
         * @return: tuple that contains  <content_length_of_the_file,<start_range,end_range>>
         */
        std::tuple<std::pair<std::string, std::size_t>, std::pair<unsigned, unsigned>>
        getResponseHeader()
        {
            auto self(shared_from_this());

            std::stringstream ssOut;
            {
                ssOut << (url == "/" ? "HTTP/1.1 200 OK" : "HTTP/1.1 404 Not Found") << std::endl;
                ssOut << "Content-Type: text/html" << std::endl;
                ssOut << "Content-Length: ";
            }
            return getContentLength(ssOut);
        }

        /*
         * Gets the filesize of the file you are sending
         * Change the name of the file.
         * This file is about ~1.2Gb
         * @param: None
         * @return: the size of the file
         */
        static std::size_t getFileSize() {
            namespace fs = boost::filesystem;
            std::string file = "the_name_of_the_file";
            return fs::file_size(file);
        }

        /*
         * This functions sends the response.
         * @param: None
         * @return: tuple in the form of message, range_start, range_end
         */
        std::tuple<std::pair<std::string, std::size_t>, std::pair<unsigned, unsigned>>
                        getContentLength(std::stringstream &ssOut)
        {
            auto self(shared_from_this());
            unsigned start_ = 0;
            unsigned end_ = 0;
            for (auto &p: headers) {
                std::cout << p.first << ": " << p.second <<'\n';
            }
            if (url == "/") { /* Send the data when requested */
                const std::string end{};
                if (auto val(headerContainsRange()); val != end) {
                    auto findRange([](const auto &val) {
                        std::regex rgx(R"(bytes=([0-9]*[.]?[0-9]+)-([0-9]*[.]?[0-9]+))");
                        std::smatch match;
                        if (!std::regex_search(val, match, rgx)) {
                            return std::pair<unsigned, unsigned>{0.0, 0.0};
                        }
                        std::string s_ = static_cast<std::string>(match[1]);
                        std::string e_ = static_cast<std::string>(match[2]);
                        e_ = e_.empty() ? "0.0" : e_;
                        return std::pair<unsigned, unsigned>{
                                std::ceil(boost::lexical_cast<float>(s_)),
                                std::ceil(boost::lexical_cast<float>(e_))};
                    });
                    auto [s_, e_] = findRange(val);
                    start_ = s_;
                    end_ = e_;

                    if (start_ == end_) {
                        std::cerr << "Send the whole file" << '\n';
                        ssOut << getFileSize();
                    } else {
                        std::cerr << "Send parts of the file" << '\n';
                        ssOut << end_ - start_;
                    }
                } else {
                    //Send the whole file to the client
                    ssOut << getFileSize();
                }
                ssOut << "\r\n";
            } else {
                std::string sHTML = "<html><body><h1>404 Not Found</h1><p>There's nothing here.</p></body></html>";
                ssOut << sHTML.length() << std::endl;
                ssOut << "\r\n";
                ssOut << sHTML;
            }
            std::size_t len = ssOut.str().length();
            return {{ssOut.str(), len}, { start_, end_}};
        }

        /*
         * This function sends the response header to the connected client
         * @param: self: the shared_ptr to itself
         * @return: the start and end range requested by the client
         */
        std::pair<unsigned, unsigned> sendHeaderFirst()
        {
            auto self(shared_from_this());
            auto tuple_ = getResponseHeader();
            auto hdr_len_pair = std::get<0>(tuple_);
            char* data = const_cast<char *>(hdr_len_pair.first.c_str());
            auto header  = std::make_shared<void*>(data); // This contains data
            auto len = hdr_len_pair.second;
            boost::asio::post(self->io_service,
                                  self->writeStrand.wrap([self, header, len]() {
                                      self->queuePackets(header, len);
                                  }));
            return std::get<1>(tuple_);
        }

        void startPacketSend()
        {
            auto self(shared_from_this());
            std::cout << "async_write: " << outgoing_queue_length.front() << '\n';
            boost::asio::async_write(self->socket_,
                    boost::asio::buffer(self->outgoing_queue.front(),  self->outgoing_queue_length.front()),
                    self->writeStrand.wrap(
                    [self](boost::system::error_code const &ec, std::size_t s) {
                        if (!ec) {
                            std::cerr << "Sent a packet of " << s << " bytes to the client\n";
                            self->outgoing_queue.pop_front();
                            self->outgoing_queue_length.pop_front();
                            if (!self->outgoing_queue.empty()) {
                                self->startPacketSend();
                            }
                        }
                        else {
                            if ((boost::asio::error::eof == ec) ||
                                (boost::asio::error::connection_reset == ec) ||
                                boost::asio::error::broken_pipe == ec) {
                                self->socket_.close();
                                return ;
                            }
                        }
            }));
        }

        void queuePackets(const std::shared_ptr<void*> data, std::size_t len)
        {
            auto self(shared_from_this());
            bool writeInProgress = !self->outgoing_queue.empty();
            if (self->socket_.is_open()) {
                outgoing_queue.push_back(*data);
                outgoing_queue_length.push_back(len);
                if (!writeInProgress) {
                    self->startPacketSend();
                }
            }
        }

        /*
         * This function sends the data to the client once all the response header is sent.
         * @param: self, FileName to send, start and end range
         * @return: None
         */
        static void sendData(unsigned start_, unsigned end_,
                                std::shared_ptr<session> self )
        {
            // Put the filename of your choice that you're sending to the client
            std::string fileName = "the_name_of_the_file_goes_here";
            std::size_t len = end_ - start_ ;
            boost::interprocess::file_mapping fm(fileName.c_str(), boost::interprocess::read_only);
            boost::interprocess::mapped_region region(fm, boost::interprocess::read_only,
                    std::ceil(start_), (int)(end_ - start_));
            self->region_.swap(region);
            auto reg_addr = std::make_shared<void*>(self->region_.get_address());
            self->queuePackets(reg_addr, len);
        }

        void startSendingPackets()
        {
            auto self(shared_from_this());
            try {
                auto[start_, end_] = self->sendHeaderFirst();
                if (start_ == end_) {
                    end_ = self->getFileSize();
                }
                self->sendData(start_, end_, self);
            }
            catch (boost::filesystem::filesystem_error &e) {
                self->url = "abcd"; // Since we couldn't open the file. Send 404 error.
                self->sendHeaderFirst();
            }
        }

        /*
         * This function reads all of the contents of the header.
         * And sends data when all the contents are read form the header.
         * This function returns immediately if there's no data to be read
         * @param: The session that is shared
         * @return: None
         */
        void parseRestOfTheHeaders()
        {
            auto self(shared_from_this());
            boost::asio::async_read_until(self->socket_,
                                   self->buff, '\r',
                                   [self](const boost::system::error_code ec,
                                           std::size_t s) {
                if (!ec) {
                    std::string line, ignore;
                    std::istream stream {&self->buff};
                    std::getline(stream, line, '\r');
                    std::getline(stream, ignore, '\n');
                    self->onReadHeader(line);
                    if (line.length() == 0) {
                        self->startSendingPackets();
                    } else {
                        self->parseRestOfTheHeaders();
                    }
                }
            });
        }

        /*
         * This function reads the first string from the header
         * delimited by '\r'. Thus, we get the requested URI.
         * @param: The session object that is shared
         * @return: None
         */
        void readFirstURI()
        {
            auto self(shared_from_this());
            std::string line;
            std::string ignore;
            std::istream stream{&self->buff};
            std::getline(stream, line, '\r');
            std::getline(stream, ignore, '\n');
            std::stringstream ssRequestLine(line);
            ssRequestLine >> method;
            ssRequestLine >> url;
            ssRequestLine >> version;
        }

        /*
         * reads the rest of the header from the client
         */

        void readAndSendPackets()
        {
            auto self(shared_from_this());
            self->readFirstURI();
            self->parseRestOfTheHeaders();
        }

        /*
         * Invoked when a client is connected.
         * This functions returns immediately if there is no data to be read
         * @param: None
         * @return: None
         */
        void do_read()
        {
            auto self(shared_from_this());
            boost::asio::async_read_until(self->socket_, self->buff,
                             '\r',
                             [self, this] (const boost::system::error_code& ec,
                                     std::size_t s) {
                if (!ec) {
                    this->readAndSendPackets();
                }
            });
        }

    public:

        session(tcp::socket socket, boost::asio::io_context& io_context)
                :  writeStrand(io_context),io_service(io_context),
                   socket_(std::move(socket))
                {
            std::clog << "Client @" << socket_.remote_endpoint().address();
            std::clog << " with " << socket_.remote_endpoint().port() << '\n';
        }

        /*
         * Interface provided to accepted client
         */
        void start()
        {
            do_read();
        }
    };

    class server {
    public:
        server(boost::asio::io_service &io_context, std::string port)
        : acceptor_(io_context, tcp::endpoint(tcp::v6(), std::stoi(port)))
        {
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            do_accept(io_context);
        }

    private:
        tcp::acceptor acceptor_;

        /*
         * Function that asynchronously accepts client.
         * This functions returns immediately if there's no connection.
         * Wait is done on boost's internal mechanism
         * @param: None
         * @return: None
         */
        void do_accept(boost::asio::io_service &io_context)
        {
            acceptor_.async_accept(
                    [this, &io_context](boost::system::error_code ec,
                            tcp::socket socket) {
                        if (!ec) {
                            std::make_shared<session>(std::move(socket), io_context)->start();
                        }
                        do_accept(io_context);
            });
        }
    };
}
#endif
