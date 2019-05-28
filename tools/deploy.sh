#!/usr/bin/env bash
###

BFSSDIR="/opt/bfss"

MONGO=10.0.1.185
MEMCC=10.0.1.186

function stop_regm()
{
    echo ===stop_regm ${1}===
    ssh -T root@${1} << EOF
PID=\`ps -ef |grep BFSS_REGMD |grep -v grep | awk '{print \$2}'\`
if [ -n "\$PID" ]; then kill \$PID; fi
mkdir -p ${BFSSDIR}/regm
EOF

}
function restart_regm()
{
    echo ===restart_regm ${1}===
    scp ./BFSS_REGMD root@${1}:${BFSSDIR}/regm

    ssh -T root@${1} << EOF
cd ${BFSSDIR}/regm; pwd
rm -f bfss_regm.log*
cat << ECFG > bfss_regm.cfg
regmd:{
mongodb_server_uri = "mongodb://${MONGO}";
service_bind_addr = "${1}:9090";
service_simplethread = 10;
};
ECFG
cat << ELOG > log4cxx.bfss_regm.properties
log4j.rootLogger=WARN, R
log4j.appender.R=org.apache.log4j.RollingFileAppender
log4j.appender.R.File=bfss_regm.log
log4j.appender.R.MaxFileSize=200MB
log4j.appender.R.MaxBackupIndex=10
log4j.appender.R.layout=org.apache.log4j.PatternLayout
log4j.appender.R.layout.ConversionPattern=%d [%t] %5p  %F (line:%L) - %m%n
ELOG
./BFSS_REGMD > /dev/null 2>&1 &
EOF
}

function stop_sn()
{
    echo ===stop_sn ${1}===
    ssh -T root@${1} << EOF
PID=\`ps -ef |grep BFSS_SND |grep -v grep | awk '{print \$2}'\`
if [ -n "\$PID" ] ; then kill \$PID; fi
mkdir -p ${BFSSDIR}/sn
EOF
}
function restart_sn()
{
    echo ===restart_sn ${1} ${2} ===
    scp ./BFSS_SND root@${1}:${BFSSDIR}/sn
    scp ./BFSS_SN_Test root@${1}:${BFSSDIR}/sn
    VID=`echo ${1} | awk -F '.' '{print \$4}'`

    ssh -T root@${1} << EOF
cd ${BFSSDIR}/sn; pwd
rm -f bfss_sn.log*

export GTEST_BFSS_RESET_SN=/dev/nvme1n1
./BFSS_SN_Test --gtest_filter=BFSS_SN_R.Reset_SN_blkdevice >> bfss_sn.log
unset GTEST_BFSS_RESET_SN

cat << ECFG > bfss_sn.cfg
snd:{
regmd_server_uri = "regmd://${2}:9090";
mongodb_server_uri = "mongodb://${MONGO}";
service_blk_max = "1000G";
service_blk_dev = "/dev/nvme1n1";
service_desc = "SN-${1}";
service_volume_id = 10${VID};
service_bind_addr = "${1}:9091";
service_simplethread = 10;
service_remote_uri = "snd://${1}:9091";
max_cache_size = "6G";
};
ECFG
cat << ELOG > log4cxx.bfss_sn.properties
log4j.rootLogger=WARN, R
log4j.appender.R=org.apache.log4j.RollingFileAppender
log4j.appender.R.File=bfss_sn.log
log4j.appender.R.MaxFileSize=200MB
log4j.appender.R.MaxBackupIndex=10
log4j.appender.R.layout=org.apache.log4j.PatternLayout
log4j.appender.R.layout.ConversionPattern=%d [%t] %5p  %F (line:%L) - %m%n
ELOG
./BFSS_SND > /dev/null 2>&1 &
EOF
}

function stop_api()
{
    echo ===stop_api ${1}===
    ssh -T root@${1} << EOF
PID=\`ps -ef |grep BFSS_APID |grep -v grep | awk '{print \$2}'\`
if [ -n "\$PID" ] ; then kill \$PID; fi
mkdir -p ${BFSSDIR}/api
EOF
}
function restart_api()
{
    echo ===restart_api ${1} ${2}===
    scp ./BFSS_APID root@${1}:${BFSSDIR}/api

    ssh -T root@${1} << EOF
cd ${BFSSDIR}/api; pwd
rm -f bfss_api.log*
cat << ECFG > bfss_apid.cfg
apid:{
mongodb_server_uri = "mongodb://${MONGO}";
regmd_server_uri = "regmd://${2}:9090";
memcache_server_uri = "memc://${MEMCC}:11211";
service_bind_addr = "${1}:9092";
service_simplethread = 10;
};
ECFG
cat << ELOG > log4cxx.bfss_api.properties
log4j.rootLogger=WARN, R
log4j.appender.R=org.apache.log4j.RollingFileAppender
log4j.appender.R.File=bfss_api.log
log4j.appender.R.MaxFileSize=200MB
log4j.appender.R.MaxBackupIndex=10
log4j.appender.R.layout=org.apache.log4j.PatternLayout
log4j.appender.R.layout.ConversionPattern=%d [%t] %5p  %F (line:%L) - %m%n
ELOG
./BFSS_APID > /dev/null 2>&1 &
EOF
}


function reset_mongo()
{
    echo ===reset_mongo ${1}===
# mongo 10.0.1.185:27017/BKeyDb   --eval "printjson(db.dropDatabase())"
# mongo 10.0.1.185:27017/ObjectDb --eval "printjson(db.dropDatabase())"
# mongo 10.0.1.185:27017/VolumeDb --eval "printjson(db.dropDatabase())"

    mongo ${1}:27017/BKeyDb   --eval "printjson(db.dropDatabase())"
    mongo ${1}:27017/ObjectDb --eval "printjson(db.dropDatabase())"
    mongo ${1}:27017/VolumeDb --eval "printjson(db.dropDatabase())"
}

function reset_memcc()
{
    echo ===reset_memcc ${1}===
    printf "flush_all\r\nquit\r\n" |nc ${1} 11211
}


function rebuild_bfss()
{
    echo ===rebuild_bfss ${1}===
    ssh -T root@${1} << EOF
mkdir -p ${BFSSDIR}
rm -f ./bfss.tar.gz
rm -rf ${BFSSDIR}/bfssproject
EOF

    scp ./bfss.tar.gz root@10.0.1.185:${BFSSDIR}

    ssh -T root@${1} << EOF
cd ${BFSSDIR}
tar zxf ./bfss.tar.gz
scl enable devtoolset-7 bash
mkdir -p ./bfssproject/build
cd ./bfssproject/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make clean
make -j8
EOF

    scp root@${1}:${BFSSDIR}/bfssproject/out/bin/BFSS_* ./
    scp root@${1}:${BFSSDIR}/bfssproject/TestCase/bin/BFSS_* ./
}


function restart_test_bigfiles() {
    echo ===restart_test_bigfiles ${1}===
    for i in $(seq 1 12);
    do
        gnome-terminal --tab -t "B$i" -- bash -c "ssh -T root@${1} << ETST
cd /home/bfss/tests/
export GTEST_BFSS_API_IP=10.0.1.119
export GTEST_BFSS_API_PORT=30000
export GTEST_BFSS_C=${GTEST_BFSS_C:=15}      # [20->768K~1M] [15->24~32K] [18->192~256K]
./BFSS_API_Test --gtest_filter=BFSS_API_Test_Exceptions.Write_BigFile --gtest_repeat=-1 --gtest_break_on_failure |tee big.$i.log
ETST"
        sleep 1
    done
}
function restart_test_base() {
    echo ===restart_test_base ${1}===
    for i in $(seq 1 12);
    do
        gnome-terminal --tab -t "A$i" -- bash -c "ssh -T root@${1} << ETST
cd /home/bfss/tests/
export GTEST_BFSS_API_IP=10.0.1.119
export GTEST_BFSS_API_PORT=30000
export GTEST_BFSS_C=${GTEST_BFSS_C:=15}      #chip size [20->768K~1M] [15->24~32K] [18->192~256K]
./BFSS_API_Test --gtest_filter=BFSS_API_TestBase* --gtest_repeat=-1 --gtest_break_on_failure |tee base.$i.log
ETST"
        sleep 1
    done
}
function restart_test_wirte() {
    echo ===restart_test_wirte ${1}===
    for i in $(seq 1 10);
    do
        gnome-terminal --tab -t "C$i" -- bash -c "ssh -T root@${1} << ETST
cd /home/bfss/tests/
export GTEST_BFSS_API_IP=10.0.1.119
export GTEST_BFSS_API_PORT=30000
export GTEST_BFSS_FILE_SS=${GTEST_BFSS_FILE_SS:=1024000}
export GTEST_BFSS_FILE_TG=${GTEST_BFSS_FILE_TG:=3}
./BFSS_API_Test --gtest_filter=BFSS_API_Test_Exceptions.Write_File --gtest_repeat=10 --gtest_break_on_failure |tee write.$i.log
ETST"
        sleep 1
    done
}
function restart_local_test() {
    PID=`ps -ef |grep BFSS_API_Test |grep -v 181 |grep -v grep | awk '{print \$2}'`
    if [ -n "$PID" ] ; then kill $PID; fi
    for i in $(seq 1 13); do
        export GTEST_BFSS_Z=${GTEST_BFSS_Z:=0}       #1 2 4... 2^Z size
        export GTEST_BFSS_X=${GTEST_BFSS_X:=20}      #[20->768K~1M] [15->24~32K] [18->192~256K]
        export GTEST_BFSS_S=${GTEST_BFSS_S:=200}     #200M
        export GTEST_BFSS_C=${GTEST_BFSS_C:=20}      #chip size [20->1M] [15->32K] [18->256K]
        gnome-terminal --tab -t B$i -- bash -c "cd ~/projx/src/bfssproject/TestCase/bin; ./BFSS_API_Test --gtest_filter=BFSS_API_TestBase* --gtest_repeat=-1 --gtest_break_on_failure |tee testB.$i.log"
        sleep 2
    done
    for i in $(seq 1 8); do
        export GTEST_BFSS_Z=0       #No 1 2 4... 2^Z size
        export GTEST_BFSS_X=30      #768M~1G
        export GTEST_BFSS_S=10000   #10G
        gnome-terminal --tab -t C$i -- bash -c "cd ~/projx/src/bfssproject/TestCase/bin; ./BFSS_API_Test --gtest_filter=BFSS_API_TestBase.CreateObject:BFSS_API_TestBase.Write --gtest_repeat=-1 --gtest_break_on_failure |tee testC.$i.log"
        sleep 2
    done
}
function stop_test() {
    echo ===stop_test ${1}===
    ssh -T root@${1} << EOF
PID=\`ps -ef |grep BFSS_API_Test |grep -v grep | awk '{print \$2}'\`
if [ -n "\$PID" ] ; then kill \$PID; fi
EOF
}
function restart_test() {
    echo ===restart_test ${1}===
    stop_test ${1}
    scp ./BFSS_*_Test root@${1}:/home/bfss/tests
    restart_test_base ${1}
    restart_test_bigfiles ${1}
    restart_test_wirte ${1}
}


### ========================================================================
WORKDIR=$(dirname $(readlink -f $0))

stop_regm 10.0.1.185
stop_sn   10.0.1.182
stop_sn   10.0.1.183
stop_sn   10.0.1.184
stop_api  10.0.1.185
stop_api  10.0.1.186

# 更新代码+打包
cd ..
#git pull --force# ##### please pull code manually.
cd ..
tar czf bfss.tar.gz --exclude=bfssproject/.git --exclude=bfssproject/cmake-build* --exclude=bfssproject/out --exclude=bfssproject/tools --exclude=bfssproject/bfss_web bfssproject

# 解压+编译+取可执行文件
rebuild_bfss 10.0.1.185

# 停服务+部署+启动服务
reset_mongo  10.0.1.185
reset_memcc  10.0.1.186

restart_regm 10.0.1.185
restart_sn   10.0.1.182 10.0.1.185
restart_sn   10.0.1.183 10.0.1.185
restart_sn   10.0.1.184 10.0.1.185
restart_api  10.0.1.185 10.0.1.185
restart_api  10.0.1.186 10.0.1.185

# 启动测试
restart_test 10.0.1.181

cd ${WORKDIR}