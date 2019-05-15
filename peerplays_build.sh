#!/bin/bash

export CMAKE_VERSION_MAJOR=3
export CMAKE_VERSION_MINOR=13
export CMAKE_VERSION_PATCH=2
export CMAKE_VERSION=${CMAKE_VERSION_MAJOR}.${CMAKE_VERSION_MINOR}.${CMAKE_VERSION_PATCH}

##### print begin
print_help() {
    printf "Help:"
    printf "\\t-d --no-deps            \\tSkip deps check\\n"
    printf "\\t-g --replace-gcc        \\tReplace gcc with 6.0 version\\n"
    printf "\\t-b --boost-root         \\tBoost root directory(will download and build automatically without that flag)\\n"
    printf "\\t-c --skip-install-cmake \\tSkip cmake ${CMAKE_VERSION} installation\\n"
    printf "\\t-s --skip-cmake         \\tSkip cmake command\\n"
    printf "\\t-d --cmake-debug        \\tBuild in debug mode\\n"
    printf "\\t   --erase-build    \\tErase build directory\\n"
    printf "By default:\\n"
    printf "\\t\\t-Check deps(apt)\\n"
    printf "\\t\\t-Download gcc if need\\n"
    printf "\\t\\t-Download and build boost\\n"
    printf "\\t\\t-Make directory build, run cmake in Release and build project\\n"

}
print_success(){
    txtbld=$(tput bold)
    bldred=${txtbld}$(tput setaf 1)
    txtrst=$(tput sgr0)
    printf "${bldred}\\n"
    printf " ____                ____  _                   ____            \\n"
    printf "|  _ \ ___  ___ _ __|  _ \| | __ _ _   _ ___  |  _ \ _ __ ___  \\n"
    printf "| |_) / _ \/ _ \ \'__| |_) | |/ _\` | | | / __| | |_) | \'__/ _ \ \\n"
    printf "|  __/  __/  __/ |  |  __/| | (_| | |_| \__ \ |  __/| | | (_) |\\n"
    printf "|_|   \___|\___|_|  |_|   |_|\__,_|\__, |___/ |_|   |_|  \___/ \\n"
    printf "                                   |___/                        \\n"
    printf "\\n${txtrst}"
}
##### print end

##### parsing args begin
DEFAULT=NO
REPLACE_GCC=NO
ERASE_BUILD=NO
SKIP_INSTALL_CMAKE=NO
BUILD_WITHOUT_CMAKE=NO
NO_DEPS=NO
DEBUG=NO
BOOST_ROOT="EMPTY"
HAVE_GCC_6=YES

while [ $# -gt 0 ]; do
    key="$1"
    case "$key" in
        -h|--help)
        print_help
        exit 0

        ;;
        -g|--replace-gcc)
        REPLACE_GCC=YES
        ;;

        --erase-build)
        ERASE_BUILD=YES
        ;;

        -c|--skip-install-cmake)
        SKIP_INSTALL_CMAKE=YES
        ;;

        -s|--skip-cmake)
        BUILD_WITHOUT_CMAKE=YES
        ;;

        -d|--no-deps)
        NO_DEPS=YES
        ;;

        -d|--cmake-debug)
        DEBUG=YES
        ;;

        -b=*|--boost-root=*)
        BOOST_ROOT="${key#*=}"
        ;;

        -b|--boost-root)
        shift
        BOOST_ROOT="$1"
        ;;

        *)
        DEFAULT=YES
        ;;
    esac
    # Shift after checking all the cases to get the next option
    shift
done
##### parsing args end

##### build dir begin
erase_build_dir() { 
    rm -rf build && mkdir build && cd build
    return 0
}
build_dir() { 
    if [ ! -d build ]; then
        mkdir build
    fi

    cd build
    return 0
}
##### build dir end

##### apt dependencies begin
DEP_ARRAY=(
    git llvm-4.0 clang-4.0 libclang-4.0-dev make automake libbz2-dev libssl-dev doxygen graphviz \
	libgmp3-dev autotools-dev build-essential libicu-dev python2.7 python2.7-dev python3 python3-dev \
	autoconf libtool curl zlib1g-dev sudo ruby libusb-1.0-0-dev libcurl4-gnutls-dev pkg-config \
    libdb++-dev libdb-dev openssl libreadline-dev ntp libleveldb-dev sudo wget
) # PP
deps() {
    COUNT=0
    printf "\\nChecking for installed dependencies...\\n"
    for (( i=0; i<${#DEP_ARRAY[@]}; i++ )); do
    	pkg=$( dpkg -s "${DEP_ARRAY[$i]}" 2>/dev/null | grep Status | tr -s ' ' | cut -d\  -f4 )
    	if [ -z "$pkg" ]; then
    		DEP=$DEP" ${DEP_ARRAY[$i]} "
    		DISPLAY="${DISPLAY}${COUNT}. ${DEP_ARRAY[$i]}\\n"
    		printf " - Package %s${bldred} NOT${txtrst} found!\\n" "${DEP_ARRAY[$i]}"
    		(( COUNT++ ))
    	else
    		printf " - Package %s found.\\n" "${DEP_ARRAY[$i]}"
    		continue
    	fi
    done
    if [ ${COUNT} -gt 1 ]; then
    	printf "\\nThe following dependencies are required to install PP:\\n"
    	printf "${DISPLAY}\\n\\n" 
    	if ! sudo apt-get -y install ${DEP}; then
    		printf " - APT dependency failed.\\n"
    		exit 1
    	else
    		printf " - APT dependencies installed successfully.\\n"
    	fi
    else 
    	printf " - No required APT dependencies to install.\\n"
    fi
    return 0
}
##### apt dependencies end

##### cmake dep begin
install_cmake(){
    printf "Checking CMAKE installation...\\n"
    type cmake
    if [ $? -ne 0 ]; then
    	printf "Installing CMAKE...\\n"
    	curl -LO https://cmake.org/files/v$CMAKE_VERSION_MAJOR.$CMAKE_VERSION_MINOR/cmake-$CMAKE_VERSION.tar.gz \
    	&& tar -xzf cmake-$CMAKE_VERSION.tar.gz \
    	&& cd cmake-$CMAKE_VERSION \
    	&& ./bootstrap --prefix=$HOME \
    	&& make -j"${JOBS}" \
    	&& make install \
    	&& cd .. \
    	&& rm -f cmake-$CMAKE_VERSION.tar.gz \
    	|| exit 1
    	printf " - CMAKE successfully installed @ ${CMAKE} \\n"
    else
    	printf " - CMAKE found\\n$(cmake --version).\\n"
    fi
    if [ $? -ne 0 ]; then exit -1; fi
    return 0
}
##### cmake dep end

##### gcc dep begin
gcc_install(){
    printf "Installation GCC 6.5.0\\n"
    sudo apt-get update && \
    sudo apt-get install build-essential software-properties-common -y && \
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    sudo apt-get update && \
    sudo apt-get install gcc-6 g++-6 -y

    if [[ $REPLACE_GCC == YES ]]; then
        sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6
    else
        export CC=/usr/bin/gcc-6
        export CXX=/usr/bin/g++-6
        HAVE_GCC_6=NO
    fi
    return 0
}
##### gcc dep end

##### mongodb begin
mongo_db_check() {
    printf "\\nChecking MongoDB installation...\\n"
    if [ ! -d $MONGODB_ROOT ]; then
    	printf "Installing MongoDB into ${MONGODB_ROOT}...\\n"
    	curl -OL http://downloads.mongodb.org/linux/mongodb-linux-x86_64-ubuntu$OS_MAJ$OS_MIN-$MONGODB_VERSION.tgz \
    	&& tar -xzf mongodb-linux-x86_64-ubuntu$OS_MAJ$OS_MIN-$MONGODB_VERSION.tgz \
    	&& mv $SRC_LOCATION/mongodb-linux-x86_64-ubuntu$OS_MAJ$OS_MIN-$MONGODB_VERSION $MONGODB_ROOT \
    	&& touch $MONGODB_LOG_LOCATION/mongod.log \
    	&& rm -f mongodb-linux-x86_64-ubuntu$OS_MAJ$OS_MIN-$MONGODB_VERSION.tgz \
    	&& cp -f $REPO_ROOT/scripts/mongod.conf $MONGODB_CONF \
    	&& mkdir -p $MONGODB_DATA_LOCATION \
    	&& rm -rf $MONGODB_LINK_LOCATION \
    	&& rm -rf $BIN_LOCATION/mongod \
    	&& ln -s $MONGODB_ROOT $MONGODB_LINK_LOCATION \
    	&& ln -s $MONGODB_LINK_LOCATION/bin/mongod $BIN_LOCATION/mongod \
    	|| exit 1
    	printf " - MongoDB successfully installed @ ${MONGODB_ROOT} (Symlinked to ${MONGODB_LINK_LOCATION}).\\n"
    else
    	printf " - MongoDB found with correct version @ ${MONGODB_ROOT} (Symlinked to ${MONGODB_LINK_LOCATION}).\\n"
    fi
    if [ $? -ne 0 ]; then exit -1; fi
    printf "Checking MongoDB C driver installation...\\n"
    if [ ! -d $MONGO_C_DRIVER_ROOT ]; then
    	printf "Installing MongoDB C driver...\\n"
    	curl -LO https://github.com/mongodb/mongo-c-driver/releases/download/$MONGO_C_DRIVER_VERSION/mongo-c-driver-$MONGO_C_DRIVER_VERSION.tar.gz \
    	&& tar -xzf mongo-c-driver-$MONGO_C_DRIVER_VERSION.tar.gz \
    	&& cd mongo-c-driver-$MONGO_C_DRIVER_VERSION \
    	&& mkdir -p cmake-build \
    	&& cd cmake-build \
    	&& $CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME -DENABLE_BSON=ON -DENABLE_SSL=OPENSSL -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DENABLE_STATIC=ON .. \
    	&& make -j"${JOBS}" \
    	&& make install \
    	&& cd ../.. \
    	&& rm mongo-c-driver-$MONGO_C_DRIVER_VERSION.tar.gz \
    	|| exit 1
    	printf " - MongoDB C driver successfully installed @ ${MONGO_C_DRIVER_ROOT}.\\n"
    else
    	printf " - MongoDB C driver found with correct version @ ${MONGO_C_DRIVER_ROOT}.\\n"
    fi
    if [ $? -ne 0 ]; then exit -1; fi
    printf "Checking MongoDB C++ driver installation...\\n"
    if [ ! -d $MONGO_CXX_DRIVER_ROOT ]; then
    	printf "Installing MongoDB C++ driver...\\n"
    	curl -L https://github.com/mongodb/mongo-cxx-driver/archive/r$MONGO_CXX_DRIVER_VERSION.tar.gz -o mongo-cxx-driver-r$MONGO_CXX_DRIVER_VERSION.tar.gz \
    	&& tar -xzf mongo-cxx-driver-r${MONGO_CXX_DRIVER_VERSION}.tar.gz \
    	&& cd mongo-cxx-driver-r$MONGO_CXX_DRIVER_VERSION/build \
    	&& $CMAKE -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME .. \
    	&& make -j"${JOBS}" VERBOSE=1 \
    	&& make install \
    	&& cd ../.. \
    	&& rm -f mongo-cxx-driver-r$MONGO_CXX_DRIVER_VERSION.tar.gz \
    	|| exit 1
    	printf " - MongoDB C++ driver successfully installed @ ${MONGO_CXX_DRIVER_ROOT}.\\n"
    else
    	printf " - MongoDB C++ driver found with correct version @ ${MONGO_CXX_DRIVER_ROOT}.\\n"
    fi
    if [ $? -ne 0 ]; then exit -1; fi
}
##### mongodb end

##### boost begin
boost_install(){
    printf "\\n Installing boost 1.67!\\n"
    if [ ! -d boost_dir ]; then
        mkdir boost_dir
    fi
    cd boost_dir
    if [ ! -f boost_1_67_0.tar.gz ]; then
        wget -O boost_1_67_0.tar.gz https://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.gz/download
    fi
    if [ ! -d boost_1_67_0 ]; then
        tar xzvf boost_1_67_0.tar.gz
    fi
    cd boost_1_67_0        
    if [ ! -f b2 ]; then
        sh bootstrap.sh
        if [[ $HAVE_GCC_6 == NO ]]; then
            echo "using gcc : 6 : /usr/bin/g++-6 ; " >> tools/build/src/user-config.jam
        fi
    fi
    ./b2 -j$(cat /proc/cpuinfo | grep processor | wc -l)
    if [ $? -ne 0 ]; then exit -1; fi
    BOOST_ROOT=$( pwd )
    cd ../..
}
##### boost end

##### build begin
build_this(){
    if [[ $BUILD_WITHOUT_CMAKE == NO ]]; then
        if [[ $DEBUG == YES ]]; then
            cmake -DBOOST_ROOT=$BOOST_ROOT -DCMAKE_BUILD_TYPE=Debug ..
        else
            cmake -DBOOST_ROOT=$BOOST_ROOT ..
        fi
    fi
    cmake --build . -- -j$(cat /proc/cpuinfo | grep processor | wc -l) && print_success
    return 0
}
##### build end

if [[ $NO_DEPS == NO ]]; then
    deps
fi

if [[ $ERASE_BUILD == YES ]]; then
    erase_build_dir
else
    build_dir
fi

if [[ $SKIP_INSTALL_CMAKE == NO ]]; then
    install_cmake
fi

mongo_db_check

if [ "$( gcc -v 2>&1 | tail -1 | awk '{print $3}' )" != "6.5.0" ]; then
    gcc_install
fi

if [[ $BOOST_ROOT == "EMPTY" ]]; then
    boost_install
fi

build_this