sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt install -y gcc-11 g++-11 binutils
sudo update-alternatives \
    --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-11 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-11

# The rest
sudo apt install -y make build-essential bison flex texinfo nasm mtools wget \
                    unzip fuse libfuse-dev uuid-dev parted libsdl2-dev pkg-config