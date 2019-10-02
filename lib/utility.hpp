#ifndef LIB_UTILITY_H
#define LIB_UTILITY_H

#include "errors.hpp"
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <list>
#include <utility>
#include <map>
#include <boost/algorithm/string.hpp>

namespace utility {
    using MT1 = std::list<std::pair<std::string, std::string>>;
    using MT2 = std::map<std::string, MT1>;

    std::pair<std::ifstream, bool> getConfFile(int &ac, char *av[])
    {
        std::ifstream ifs;
        try {
            if (ac != 2) {
                throw InvalidUsage {"Invalid use of the program\n"};
            }
            ifs.open(av[1]);
            if (!ifs.is_open()) {
                throw FileNotFound {"Supplied file can't be found\n", av[1]};
            }
            return {std::move(ifs),true}; 
        }
        catch(FileNotFound &e) {
            std::cerr << e.getErrMsg() << '\n';
        }
        catch (InvalidUsage &e) {
            std::cerr << e.getErrMsg() << '\n';
        }
        return {std::move(ifs), false};
    }

    auto checkCommentOrList([](const std::string &s, const auto ch) {
            auto it = std::begin(s);
            const auto is_present (
                    std::find_if(it, std::next(it, 1), [&ch] (auto c) {
                        return c == ch;
            }));
            return (std::distance(it, is_present) == 0);
    });

    std::ifstream parseRest(std::ifstream &&ifs,
            MT2 &confFileMetaData, std::string s)
    {
        std::string line;
        MT1 dataPair;
        auto keyValue ([](auto s) {
                std::regex rgx ("(\\w+)\\s?=\\s?\\[\\s?(.*?)\\s?\\]", std::regex::icase);
                std::smatch match;
                if (!std::regex_search(s,match,rgx)) {
                throw InvalidFile {"Unknown configuration file"};
                }
                return std::pair<std::string, std::string>{match[1], match[2]};
                });
        while(std::getline(ifs, line)) {
            if (line.empty()) {
                break;
            }
            dataPair.push_back({keyValue(line)});
        }
        confFileMetaData.insert({s, dataPair});
        return std::move(ifs);
    }

    MT2 parseConfFile(std::ifstream &ifs)
    {
        std::string line;
        MT2 confFileMetaData;
        while (std::getline(ifs, line)) {
            auto filtered (utility::checkCommentOrList(line, '#'));
            if (!filtered)  {
                auto header (utility::checkCommentOrList(line, '['));
                if (header) {
                    ifs = utility::parseRest(std::move(ifs),
                                confFileMetaData, line);
                }
            }
        }
        ifs.close();
        return confFileMetaData;
    }

    std::string searchForKey(MT2& keyValue, const std::string s)
    {       
        std::list<std::string> results;
        auto search = keyValue.find(s);
        if (search != keyValue.end()) {
            auto sec = search->second;
            auto text = sec.front().second;
            boost::split(results, text,
                    [](char c){return c == ',';});
        }
        return results.front();
    }

}
#endif //LIB_UTILITY_H
