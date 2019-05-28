package ipc

import (
	"encoding/json"
	"fmt"
	"log"
	"net"
	"net/http"
	"net/rpc"
	"pressure/app"

	"github.com/sirupsen/logrus"
)

// Request for test.
type Request struct {
	TaskName  string
	ProcessID int
}

// ProgressRequest for updating progress.
type ProgressRequest struct {
	*Request
	Tag   int
	Value int
	Speed float64
}

// StatsRequest for stats.
type StatsRequest struct {
	*Request
	Stats *app.PerfStats
}

// StatusRequest for stats.
type StatusRequest struct {
	*Request
	Status app.Status
}

// Server for ipc
type Server struct {
}

// StartServer starts ipc server.
func StartServer() string {
	s := new(Server)
	rpc.Register(s)
	rpc.HandleHTTP()

	l, e := net.Listen("tcp", ":0")
	if e != nil {
		log.Fatal("listen error:", e)
	}
	addr := l.Addr().(*net.TCPAddr).String()
	logrus.Infof("Start Ipc=> %s", addr)

	go http.Serve(l, nil)

	return addr
}

func validateRequest(req *Request) (*app.Task, *app.Process, error) {
	// Validate request.
	t := app.FindTask(req.TaskName)
	if t == nil {
		return nil, nil, fmt.Errorf("Illegal TaskName: %s", req.TaskName)
	}
	p := t.FindProcess(req.ProcessID)
	if p == nil {
		return nil, nil, fmt.Errorf("Illegal Process Id=> %d", req.ProcessID)
	}
	return t, p, nil
}

// FetchCfg gets task config from server.
func (s *Server) FetchCfg(req *Request, cfg *app.Config) error {
	t, p, err := validateRequest(req)
	if err != nil {
		logrus.Info(err)
		return err
	}

	*cfg = t.Cfg
	cfg.ExpectedWrites = p.ExpectedWrites
	cfg.Sample = p.Sample

	c, _ := json.Marshal(t)
	logrus.Debug(string(c))
	return nil
}

// UpdateProgress update task running progress.
func (s *Server) UpdateProgress(req *ProgressRequest, updated *bool) error {
	t, p, err := validateRequest(req.Request)
	if err != nil {
		logrus.Info(err)
		return err
	}
	logrus.Debugf("Task: %s, Process: %d, Tag: %v, Progress: %v", req.TaskName, p.ID, req.Tag, req.Value)

	r := app.Routine{
		Progress: req.Value,
		Speed:    req.Speed,
	}
	t.UpdateProgress(p.ID, req.Tag, r)
	return nil
}

// CommitStats commit test stats.
func (s *Server) CommitStats(req *StatsRequest, updated *bool) error {
	_, p, err := validateRequest(req.Request)
	if err != nil {
		logrus.Info(err)
		return err
	}
	p.Stats = req.Stats
	return nil
}

// UpdateStatus update process status.
func (s *Server) UpdateStatus(req *StatusRequest, updated *bool) error {
	t, p, err := validateRequest(req.Request)
	if err != nil {
		logrus.Info(err)
		return err
	}
	p.Status = req.Status
	t.UpdateStatus()
	logrus.Debugf("Task: %s, Process: %d, Status: %s", t.Name, p.ID, p.Status)
	return nil
}
