# parallel_downloading

You need to install libcurl to use the curl/curl.h header
which can be found [here](https://curl.haxx.se/download.html)

From source: download the source from the libcurl website
From linux: apt-get install libcurl4-gnutls-dev

If you are building libcurl from the source, you may have to link the cpp file with the libcurl library using -lcurl during compile, otherwise just compile it

You might also have to link the pthread library with -lpthread

Right now the client.cpp downloads two jpg seperately using threads
Not much functionality right now

- You may need to install [bazel](https://docs.bazel.build/versions/master/install.html) inorder to build the project and run ```./build```
- or you could change the source file

