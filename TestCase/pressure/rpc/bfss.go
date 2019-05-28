package rpc

import (
	"pressure/app"
	"pressure/thrift/BFSS_API"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"

	"github.com/apache/thrift/lib/go/thrift"
)

var mutex sync.RWMutex

var mapClientTransport map[*BFSS_API.BFSS_APIDClient]thrift.TTransport

func init() {
	mapClientTransport = make(map[*BFSS_API.BFSS_APIDClient]thrift.TTransport)
}

// NewClient establishs a new connection to API service.
func NewClient() (*BFSS_API.BFSS_APIDClient, error) {
	tSocket, err := thrift.NewTSocket(app.Cfg.Server)
	if err != nil {
		log.Error(err)
		return nil, err
	}
	tSocket.SetTimeout(time.Minute)

	transportFactory := thrift.NewTFramedTransportFactory(thrift.NewTTransportFactory())
	transport, err := transportFactory.GetTransport(tSocket)
	//transport := thrift.NewTFramedTransport(tSocket)
	if err != nil {
		log.Error(err)
		return nil, err
	}
	protocolFactory := thrift.NewTBinaryProtocolFactoryDefault()

	client := BFSS_API.NewBFSS_APIDClientFactory(transport, protocolFactory)

	if err := transport.Open(); err != nil {
		log.Error(err)
		return nil, err
	}
	mutex.Lock()
	mapClientTransport[client] = transport
	mutex.Unlock()
	return client, nil
}

// CloseClient close a connection to API service.
func CloseClient(client *BFSS_API.BFSS_APIDClient) {
	t, ok := mapClientTransport[client]
	if ok {
		t.Close()
		mutex.Lock()
		delete(mapClientTransport, client)
		mutex.Unlock()
	}
}
