package main

import (
	"bfssproject/bfss_web/common"
	"bfssproject/bfss_web/config"
	"bfssproject/bfss_web/service"
	"github.com/gin-gonic/gin"
	"log"
	"net/http"
	"runtime"
)

func init() {
	runtime.GOMAXPROCS(runtime.NumCPU())
	gin.SetMode(gin.DebugMode)
}

func main() {
	//启动服务
	log.Println("Server is starting...")
	InitConfig()

	//监听http端口
	log.Fatal(HttpListenAndServe(":" + config.Server().GetServicePort()))

}

//初始化配置
func InitConfig() {
	config.LoadServerConfig()
	host := config.Server().GetBfssApiServiceHost()
	port := config.Server().GetBfssApiServicePort()
	common.CreateApiClient(host, port)
}

func HttpListenAndServe(addr string) error {
	log.Println("Http Server Listen At", addr)

	//http.Handle("/static/", http.StripPrefix("/static", http.FileServer(http.Dir("./static")))) // 断点续传测试方法
	http.HandleFunc("/readblk", api.ReadBlk)

	return http.ListenAndServe(addr, nil)
}
