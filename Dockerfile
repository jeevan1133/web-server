FROM ubuntu

WORKDIR /app

COPY . /app
RUN rm -rf ./.git* ./.idea/ cmake* CMake*

RUN apt-get -y update && apt-get -y upgrade

RUN apt-get install -y g++ nano wget
RUN mv install_boost.sh /usr/local/
WORKDIR /usr/local/
RUN sh install_boost.sh
RUN rm -rf boost*.bz2 install_bosot.sh
WORKDIR /app

RUN g++ -std=c++17 -I /usr/local/boost_1_70_0 main/*.cc lib/*.hpp \
        -lboost_system -lboost_filesystem -lpthread


CMD ["/bin/bash", "./a.out"]
