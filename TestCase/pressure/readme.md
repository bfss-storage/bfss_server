# BFSS Press test based on golang(>= go.1.11)
## Install dependency
```
go mod tidy
```

## 配置服务器host和port
conf/config.json
```json
{
    "rpc": {
        "server": "localhost",
        "port": "9092"
    }
}
```

## 测试开发直接按照下面命令一键编译运行
```
make dev
```
可以修改Makefile的dev指令设置命令行参数

## 生产环境
### build
```
make build #生成可执行文件在bin目录下
```

### run
-h 参看命令行参数
```
Usage of ./bfss-pressure:
  -data-dir string
        xlsx统计信息的保存目录，默认当前工作目录
  -enable-concurrent
        是否并发测试，多个client连接并发写
  -expected-writes string
        预期写入服务器的所有文件大小（默认1G）,默认单位是字节，
        例如：
        -expected-writes 1024, 表示1k
        -expected-writes 10G
  -routines int
        并发协程数量，默认0，表示cpu核数（开启并发有效）
  -sample string
        采样率S100k表示1k~100k之间，其他以此类推。共五个选项[S100k,S1M,S5M,S10M,S100M] (default "S100K")
  -server string
        RPC服务的ip和端口(默认从config.json配置文件获取，这边会覆盖配置文件设置)
```
例如：
```
./bfss-test -expected-writes 10G -sample S10M -server localhost:9092
```