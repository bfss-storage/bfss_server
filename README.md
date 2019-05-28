# Toolchains install guide
------

### thrift
```
wget https://www-eu.apache.org/dist/thrift/0.12.0/thrift-0.12.0.tar.gz
tar xzf thrift-0.12.0.tar.gz
cd thrift-0.12.0
./bootstrap.sh
./configure
make
sudo make install
```
* 参考 https://thrift.apache.org/docs/BuildingFromSource
* 尤其需要注意 Basic requirements 和 Ubuntu install -- https://thrift.apache.org/docs/install/debian

------
### apr - apache portable runtime 

```
wget https://www-eu.apache.org/dist//apr/apr-1.6.5.tar.gz
tar xzf apr-1.6.5.tar.gz
cd apr-1.6.5
./configure
make
sudo make install
```

```
wget https://www-us.apache.org/dist//apr/apr-util-1.6.1.tar.gz
tar xzf apr-util-1.6.1.tar.gz
cd apr-util-1.6.1
./configure
make
sudo make install
```
* configure提示 "APR could not be located", 需要在configure命令后面指定APR路径，默认如下： 
```shell
./configure --with-apr=/usr/local/apr
```
* 参考 https://apr.apache.org/compiling_unix.html

------
### log4cxx
```
wget https://www-eu.apache.org/dist/logging/log4cxx/0.10.0/apache-log4cxx-0.10.0.zip
unzip apache-log4cxx-0.10.0.zip
cd apache-log4cxx-0.10.0
./configure
make
make check
sudo make install
```
* log4cxx 如果编译编译时发生错误请用下面的方式打补丁
```
cd ..
patch  -s -p0 <${bfssproject}/fix.patch
```
* 参考 https://logging.apache.org/log4cxx/latest_stable/building/autotools.html
* 如果报错，可参考https://stackoverflow.com/questions/50808912/using-log4cxx-in-cmake 将 https://github.com/Kitware/vibrant/blob/master/CMake/FindLog4cxx.cmake 取下来，复制到目录：

```
sudo mkdir /usr/local/lib/cmake/liblog4cxx
sudo cp tools/FindLog4cxx.cmake /usr/local/lib/cmake/liblog4cxx/liblog4cxxConfig.cmake
# 原帖是复制到/usr/share/cmake-3.10/Modules/Findliblog4cxx.cmake，但是不一定解决问题。
#      复制到/usr/local/lib/cmake/liblog4cxx/liblog4cxxConfig.cmake，验证可行。
```

------
### mangocxx - MONGODB C++ DRIVER 
```
wget https://github.com/mongodb/mongo-c-driver/releases/download/1.13.1/mongo-c-driver-1.13.1.tar.gz
tar xzf mongo-c-driver-1.13.1.tar.gz
cd mongo-c-driver-1.13.1
mkdir cmake-build
cd cmake-build
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
```
* 安装mangocxx之前需要先安装mangoc
* 参考 http://mongoc.org/libmongoc/current/installing.html

```
git clone https://github.com/mongodb/mongo-cxx-driver.git --branch releases/stable --depth 1
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBSONCXX_POLY_USE_MNMLSTC=1 .
sudo make
sudo make install
```
* 因为make过程中有安装MNMLSTC/core (default for non-Windows platforms) Select with -DBSONCXX_POLY_USE_MNMLSTC=1
* 参考 http://mongocxx.org/mongocxx-v3/installation/

------
### libconfig
```
wget http://www.hyperrealm.com/packages/libconfig-1.4.10.tar.gz
tar xzf libconfig-1.4.10.tar.gz 
cd libconfig-1.4.10/
./configure
sudo make
sudo make install
```
* 参考 http://www.hyperrealm.com/oss_libconfig.shtml


------

## 安装 mangodb

* 参考 https://docs.mongodb.com/manual/tutorial/install-mongodb-on-ubuntu/

------

## 安装 libmemcached 


```
# 安装memcached服务
sudo apt install memcached

# 安装libmemcached客户端SDK

wget https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
tar xzf libmemcached-1.0.18.tar.gz
cd libmemcached-1.0.18
./configure
sudo make
sudo make install

```
* 出现下面错误需要打补丁
```
clients/memflush.cc:42:22: error: ISO C++ forbids comparison between pointer and integer [-fpermissive]
   if (opt_servers == false)
```
补丁命令：
```
patch -d ${libmemcached-dir} -p0 < ${BFSS_PROJECT/tools/libmemcached-build.patch}
```
* 参考 https://github.com/oneinstack/oneinstack/issues/154

更新cmake：
```
# enter your bfss-project dir, eg.
cd ~/bfssproject/tools
sudo mkdir /usr/local/lib/cmake/libmemcached-1.0.18 -p
sudo cp FindLibmemcached.cmake /usr/local/lib/cmake/libmemcached-1.0.18/libmemcachedConfig.cmake
```

* 参考 http://www.voidcn.com/article/p-wwhwyzkw-wd.html


------


## 安装 google-breakpad
```
git clone https://chromium.googlesource.com/breakpad/breakpad
./configure && make
sudo make install
```
* 参考https://chromium.googlesource.com/breakpad/breakpad/
 

------
