package app

import (
	"encoding/json"
	"flag"
	"fmt"
	"net"
	"os"
	"runtime"
	"strconv"
	"strings"

	"github.com/sirupsen/logrus"
)

// Config saves app config.
type Config struct {
	Routines         int      `json:"routines,omitempty"`
	EnableConcurrent bool     `json:"enable_concurrent,omitempty"`
	Sample           Strategy `json:"sample"`
	ExpectedWrites   int64    `json:"expected_writes,omitempty"`
	DataDir          string   `json:"data_dir,omitempty"`
	Server           string   `json:"server,omitempty"`
	MaxSlice         int32    `json:"max_slice,omitempty"`
	CacheSize        int64    `json:"cache_size,omitempty"`
	VerifyHash       bool     `json:"verify_hash,omitempty"`
	Fast             bool     `json:"fast,omitempty"`
	Profile          bool     `json:"profile,omitempty"`
	Ipc              bool     `json:"ipc,omitempty"`
	IpcAddr          string   `json:"ipc_addr,omitempty"`
	TaskName         string   `json:"task_name,omitempty"`
	IpcClient        bool     `json:"ipc_client,omitempty"`
	Web              bool     `json:"web,omitempty"`
	WebServer        string   `json:"web_server,omitempty"`
	LogLevel         string   `json:"log_level,omitempty"`
}

// Cfg as global Config
var Cfg Config

var routines int
var enableConcurrent bool
var sample string
var expectedWrites string
var dataDir string
var server string
var maxSlice string
var loglevel string
var cacheSize string
var verifyHash bool
var enableFast bool
var enableProfile bool
var ipcClient bool
var ipc bool
var ipcAddr string
var taskName string
var web bool
var webServer string

func init() {
	flag.BoolVar(&enableConcurrent, "enable-concurrent", false, "Whether enable concurrent for test.")
	flag.BoolVar(&verifyHash, "verify-hash", false, "Whether verify hash of files written server.")
	flag.BoolVar(&enableFast, "fast", false, "Whether use FastWrite method.")
	flag.BoolVar(&enableProfile, "profile", false, "Whether enable golang pprof.")
	flag.BoolVar(&ipc, "ipc", false, "Whether enable ipc server.")
	flag.BoolVar(&ipcClient, "ipc-client", false, "Whether enable ipc.")
	flag.BoolVar(&web, "web", false, "Whether enable web server.")
	flag.IntVar(&routines, "routines", 0, "Number of concurrent routines.")
	flag.StringVar(&sample, "sample", "S1M", "Sample of files for test.")
	flag.StringVar(&expectedWrites, "expected-writes", "100M", "Bytes need to be written to BFSS.")
	flag.StringVar(&dataDir, "data-dir", "", "Location for exported xlsx")
	flag.StringVar(&server, "server", "", "RPC server")
	flag.StringVar(&maxSlice, "max-slice", "1M", "Max slice size for one write.")
	flag.StringVar(&loglevel, "log-level", "info", "Log level[panic,fatal,error,warn,info,debug,trace].")
	flag.StringVar(&cacheSize, "cache-size", "1G", "Cache size for pre-allocated files.")
	flag.StringVar(&ipcAddr, "ipc-addr", "", "Ipc server address.")
	flag.StringVar(&taskName, "task", "", "Ipc task name.")
	flag.StringVar(&webServer, "web-server", ":9527", "Web server address.")
}

// ParseBytes convert string to bytes
func ParseBytes(str string) int64 {
	str = strings.TrimSpace(str)
	if len(str) == 0 {
		return 0
	}
	value := str[:len(str)-1]
	unit := str[len(str)-1]

	if unit >= '0' && unit <= '9' {
		value = str
	}
	logrus.Infof("%s,%c", value, unit)
	var scale int64
	switch unit {
	case 'k', 'K':
		scale = 1024
	case 'm', 'M':
		scale = 1024 * 1024
	case 'g', 'G':
		scale = 1024 * 1024 * 1024
	default:
		scale = 1
	}
	nvalue, err := strconv.ParseInt(value, 10, 64)
	if err != nil {
		return 0
	}
	return nvalue * scale
}
func validateParams() error {
	expectedBytes := ParseBytes(expectedWrites)
	if expectedBytes <= 0 {
		return fmt.Errorf("invalid expected-writes,%s", expectedWrites)
	}
	Cfg.ExpectedWrites = expectedBytes

	s, err := StrategyFromString(sample)
	if err != nil {
		logrus.Error(err)
		return err
	}
	Cfg.Sample = s

	if routines <= 0 {
		routines = runtime.NumCPU()
	}
	Cfg.Routines = routines

	if len(dataDir) > 0 {
		err := os.MkdirAll(dataDir, os.ModeDir)
		if err != nil {
			return err
		}
		Cfg.DataDir = dataDir
	}

	if len(server) > 0 {
		_, _, err := net.SplitHostPort(server)
		if err != nil {
			return err
		}
		Cfg.Server = server
	}

	Cfg.MaxSlice = int32(ParseBytes(maxSlice))

	Cfg.CacheSize = ParseBytes(cacheSize)
	if Cfg.CacheSize <= 0 {
		return fmt.Errorf("Invalid CacheSize=> %s", cacheSize)
	}

	// verify-hash
	Cfg.VerifyHash = verifyHash

	// fast mode
	Cfg.Fast = enableFast

	// golang pprof
	Cfg.Profile = enableProfile

	Cfg.Ipc = ipc
	Cfg.IpcAddr = ipcAddr
	Cfg.IpcClient = ipcClient
	Cfg.TaskName = taskName

	Cfg.Web = web
	Cfg.WebServer = webServer

	// log-level
	level, err := logrus.ParseLevel(loglevel)
	if err != nil {
		level = logrus.InfoLevel
	}
	logrus.SetLevel(level)

	data, _ := json.Marshal(&Cfg)
	logrus.Info(string(data))

	return nil
}

// ParseArgs parse command line.
func ParseArgs() error {
	flag.Parse()
	Cfg.EnableConcurrent = enableConcurrent
	if err := validateParams(); err != nil {
		logrus.Error(err)
		return err
	}
	return nil
}
