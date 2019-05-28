package config

import (
	yaml "gopkg.in/yaml.v2"
)

// 服务器配置
type serverCfg struct {
	ServicePort        string `yaml:"service_port"`          // 服务端口号
	ServiceHost        string `yaml:"host"`                  // 服务地址
	BfssApiServicePort string `yaml:"bfss_api_service_port"` // BFSS_API服务端口号
	BfssApiServiceHost string `yaml:"bfss_api_host"`         // BFSS_API服务地址
}

// ServerCfgWrap 包装器
type ServerCfgWrap struct {
	data serverCfg
}

// 解析数据
func (d *ServerCfgWrap) parse(bin []byte) bool {
	err := yaml.Unmarshal(bin, &d.data)
	if err != nil {
		return false
	}
	return true
}

func (d *ServerCfgWrap) GetServicePort() string {
	return d.data.ServicePort
}

func (d *ServerCfgWrap) GetServiceHost() string {
	return d.data.ServiceHost
}

func (d *ServerCfgWrap) GetBfssApiServicePort() string {
	return d.data.BfssApiServicePort
}

func (d *ServerCfgWrap) GetBfssApiServiceHost() string {
	return d.data.BfssApiServiceHost
}
