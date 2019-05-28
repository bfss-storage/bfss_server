package config

import (
	"github.com/fsnotify/fsnotify"
	"io/ioutil"
	"log"
	"sync"
)

const (
	// ServerPath 服务配置路径
	ServerPath = "settings/server.yml"
)

var once sync.Once
var globalManager *Manager

func Server() *ServerCfgWrap {
	if globalManager == nil {
		return nil
	}
	return &globalManager.server
}

// Manager 配置管理器
type Manager struct {
	server  ServerCfgWrap
	watcher *fsnotify.Watcher
}

// parser 配置解析器
type parser interface {
	parse([]byte) bool
}

// LoadServerConfig 加载服务器配置
func LoadServerConfig() {
	once.Do(func() {

		// 创建管理器
		cfgMgr := Manager{}
		watcher, err := fsnotify.NewWatcher()
		if err != nil {
			log.Panic(err)
		}
		cfgMgr.watcher = watcher

		// 加载系统配置
		bin, err := ioutil.ReadFile(ServerPath)
		if err != nil {
			log.Panic(err)
		}
		if !cfgMgr.server.parse(bin) {
			log.Panicf("Failed to parse %v", ServerPath)
		}

		globalManager = &cfgMgr
	})
}
