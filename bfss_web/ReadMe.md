# BFSS-HTTP Web Server

## 环境搭建
### 生成thrift接口文件
```bash
cd ../bfss_api
thrift --gen go --out ../.. -r ./bfss_api.thrift
```
注意：
工程根目录是 $GOPATH/src/bfssproject/bfss_web

### 生成可执行文件
```bash
# get dependencies first.
go get
go build
```