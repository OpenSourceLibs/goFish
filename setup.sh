#!/bin/sh
## Constants
HOME_DIRECTORY=/home/ubuntu
PROJECT_DIRECTORY=~
OPENCV_VERSION=4.1.0

## Install Opencv 4.1
sudo apt-get update

if [ $1 = "FULL" ]; then
    sudo apt-get install cmake
    sudo apt-get install gcc
    sudo apt-get install g++
    sudo apt-get install golang-go
fi

if [ $1 = "FULL" ] || [ $1 = "OPENCV" ]; then
    ## Get OpenCV git repos.
    cd ~ || exit
    git clone https://github.com/opencv/opencv.git
    git clone https://github.com/opencv/opencv_contrib.git
    
    ## Sync up the OpenCV repos.
    cd ~/opencv_contrib || exit
    git checkout $OPENCV_VERSION
    cd ~/opencv || exit
    git checkout $OPENCV_VERSION
    mkdir build
    cd build || exit
    
    ## Make and install OpenCV.
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/usr/local -D OPENCV_GENERATE_PKGCONFIG=ON -D OPENCV_EXTRA_MODULES_PATH=$HOME_DIRECTORY/opencv_contrib/modules -DBUILD_opencv_java=OFF -DBUILD_opencv_python=OFF ..
    make -j7
    sudo make install
    
    ## Remove the OpenCV git repos to save on space.
    cd ~ || exit
    rm -r ~/opencv*
fi

## Compile the project.
cd $PROJECT_DIRECTORY/goFish || exit
cd static/ || exit
mkdir temp
mkdir videos
mkdir video-info
mkdir proc_videos
cd ../ || exit

## Install Go project dependencies.
go get github.com/dgrijalva/jwt-go
go get github.com/fatih/structs
go get github.com/go-yaml/yaml
go get github.com/mitchellh/mapstructure

## Build Go
cd goServer || exit
go build
cd .. || exit

## Build C++
cd findFish || exit
mkdir build && cd build || exit
cmake .. && make && cd ../..

cd CameraCalibration || exit
mkdir build && cd build || exit
cmake .. && make && cd ../..
