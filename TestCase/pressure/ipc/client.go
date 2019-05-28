package ipc

import (
	"encoding/json"
	"net/rpc"
	"os"
	"pressure/app"

	"github.com/sirupsen/logrus"
)

// Client handles connection with ipc server.
type Client struct {
	conn *rpc.Client
}

var dclient *Client

// DClient default client.
func DClient() *Client {
	return dclient
}

// DoInit do some ipc init.
func DoInit() {
	if dclient == nil {
		dclient, _ = NewClient(app.Cfg.IpcAddr)
	}
}

// NewClient establish a connection with ipc server.
func NewClient(addr string) (*Client, error) {
	dclient := new(Client)
	conn, err := rpc.DialHTTP("tcp", addr)
	if err != nil {
		logrus.Fatal("dialing:", err)
		return nil, err
	}
	dclient.conn = conn
	return dclient, nil
}

// FetchCfg gets config from ipc server.
func (c *Client) FetchCfg(cfg *app.Config) error {
	req := &Request{
		TaskName:  app.Cfg.TaskName,
		ProcessID: os.Getpid(),
	}
	if err := c.conn.Call("Server.FetchCfg", req, &cfg); err != nil {
		logrus.Error(err)
		return err
	}
	data, _ := json.Marshal(cfg)
	logrus.Info(string(data))
	return nil
}

// UpdateProgress send progress to server.
func (c *Client) UpdateProgress(tag int, value int, speed float64) error {
	req := &ProgressRequest{
		Request: &Request{
			TaskName:  app.Cfg.TaskName,
			ProcessID: os.Getpid(),
		},
		Tag:   tag,
		Value: value,
		Speed: speed,
	}
	if err := c.conn.Call("Server.UpdateProgress", req, nil); err != nil {
		logrus.Error(err)
		return err
	}
	return nil
}

// CommitStats commit the test stats.
func (c *Client) CommitStats(stats *app.PerfStats) error {
	req := &StatsRequest{
		Request: &Request{
			TaskName:  app.Cfg.TaskName,
			ProcessID: os.Getpid(),
		},
		Stats: stats,
	}
	if err := c.conn.Call("Server.CommitStats", req, nil); err != nil {
		logrus.Error(err)
		return err
	}
	return nil
}

// UpdateStatus update process status.
func (c *Client) UpdateStatus(status app.Status) error {
	req := &StatusRequest{
		Request: &Request{
			TaskName:  app.Cfg.TaskName,
			ProcessID: os.Getpid(),
		},
		Status: status,
	}
	if err := c.conn.Call("Server.UpdateStatus", req, nil); err != nil {
		logrus.Error(err)
		return err
	}
	return nil
}
