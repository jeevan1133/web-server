#/bin/sh

wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.bz2

tar --bzip2 -xf boost*.bz2
cd boost_1_70_0
./bootstrap.sh

./b2 --without-python install
