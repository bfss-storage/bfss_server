package common

import (
	"bfssproject/bfss_web/BFSS_API"
	"github.com/apache/thrift/lib/go/thrift"
	"log"
	"net"
	"sync"
)

var botOnce sync.Once
var ApiClient *BFSS_API.BFSS_APIDClient

func CreateApiClient(host string, port string) {
	botOnce.Do(func() {
		tSocket, err := thrift.NewTSocket(net.JoinHostPort(host, port))
		if err != nil {
			log.Print("From ReadBlk Func,tSocket err:", err)
		}
		transportFactory := thrift.NewTFramedTransportFactory(thrift.NewTTransportFactory())
		transport, err := transportFactory.GetTransport(tSocket)
		protocolFactory := thrift.NewTBinaryProtocolFactoryDefault()

		ApiClient = BFSS_API.NewBFSS_APIDClientFactory(transport, protocolFactory)

		if err := transport.Open(); err != nil {
			transport.Close()
			log.Print("From ReadBlk Func,Error opening:", host+":"+port)
		}
		//defer transport.Close()
	})
}
