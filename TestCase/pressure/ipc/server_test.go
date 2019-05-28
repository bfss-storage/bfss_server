package ipc

import (
	"os"
	"pressure/app"
	"testing"

	"github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

var addr string

var client *Client

func setup() error {
	addr = StartServer()

	c, err := NewClient(addr)
	if err != nil {
		logrus.Fatal("dialing:", err)
		return err
	}
	client = c

	// test data
	cfg := &app.Config{
		Fast:      true,
		Server:    "10.0.1.181:30000",
		MaxSlice:  1024 * 1024,
		CacheSize: 1024 * 1024 * 10,
	}
	t1, err := app.NewTask("t1", cfg, 1)
	if err != nil {
		logrus.Error(err)
		return err
	}
	t1.Units = append(t1.Units, &app.Process{
		ID:             os.Getpid(),
		Sample:         app.S100K,
		ExpectedWrites: 1024 * 1024 * 10,
	})

	return nil
}

func TestFetchCfg(t *testing.T) {
	setup()

	t.Run("FetchCfg", func(t *testing.T) {
		var cfg app.Config
		app.Cfg.TaskName = "t1"
		err := client.FetchCfg(&cfg)
		require.Nil(t, err, err)
		assert.Greater(t, cfg.ExpectedWrites, 0)
		assert.Equal(t, 1024*1024, int(cfg.MaxSlice))
	})

	t.Run("FetchCfgError", func(t *testing.T) {
		var cfg app.Config
		app.Cfg.TaskName = "t0"
		err := client.FetchCfg(&cfg)
		require.NotNil(t, err, err)
	})
}

func TestUpdateProgress(t *testing.T) {
	setup()

	t.Run("Routine", func(t *testing.T) {
		app.Cfg.TaskName = "t1"
		err := client.UpdateProgress(0, 50)
		require.Nil(t, err, err)

		task := app.FindTask(app.Cfg.TaskName)
		require.NotNilf(t, task, "Wrong Taskname=> %s", app.Cfg.TaskName)

		p := task.FindProcess(os.Getpid())
		require.NotNilf(t, p, "Wrong pid=> %v", os.Getpid())

		assert.Equal(t, 50, p.Progress[0])
	})
}
