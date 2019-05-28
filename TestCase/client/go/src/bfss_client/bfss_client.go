
package main

import (
    "context"
    "github.com/apache/thrift/lib/go/thrift"
    "net"
    "fmt"
    "log"
    "BFSS_API"
)

const (
    HOST = "localhost"
    PORT = "9090"
)

func main()  {
    tSocket, err := thrift.NewTSocket(net.JoinHostPort(HOST, PORT))
    if err != nil {
        log.Fatalln("tSocket error:", err)
    }
    transportFactory := thrift.NewTFramedTransportFactory(thrift.NewTTransportFactory())
    transport, err := transportFactory.GetTransport(tSocket)
    protocolFactory := thrift.NewTBinaryProtocolFactoryDefault()

    client := BFSS_API.NewBFSSDClientFactory(transport, protocolFactory)

    if err := transport.Open(); err != nil {
        log.Fatalln("Error opening:", HOST + ":" + PORT)
    }
    defer transport.Close()


    d, err := client.GetFileInfo(context.Background(), "std-test")
    fmt.Println(d)





}
