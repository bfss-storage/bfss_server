package test

import (
	"context"
	"fmt"
	"math/rand"
	"pressure/app"
	"pressure/rpc"
	"pressure/thrift/bfss"
	"time"

	"github.com/sirupsen/logrus"
)

func genFileSize(s app.Strategy) int32 {
	sample := app.Samples(s)
	len := len(sample)
	if len <= 0 {
		return 0
	}
	for _, v := range sample {
		if v <= 0 {
			logrus.Info("Invalid sample")
			return 0
		}
	}

	f := rand.Intn(len)
	return sample[f]
}

// CreateObject test CreateObject rpc.
func CreateObject(stats *app.PerfStats) error {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	tag := fmt.Sprintf("[Perf_CreateObject][%s]", time.Now().String())
	for {
		if stats.TotalPreparedFiles >= stats.ExpectedFiles {
			break
		}
		stats.AddPreparedFiles(1)

		strategy := app.RandomStrategy()

		name := UUIDFileName()
		size := genFileSize(strategy)

		fs := &app.FileStats{
			Name: name,
			Size: int64(size),
		}
		logrus.Debugf("CreateObject=> Name:%s,Size:%v,Strategy:%s", name, size, strategy.String())

		t := time.Now()
		r, err := client.CreateObject(context.Background(), name, size, 0, tag)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, name)
		}
		fs.CreateObjectTime = time.Now().Sub(t)
		logrus.Debugf("CreateObject=> Name:%s,Size:%v,CreateTime:%s", name, size, fs.CreateObjectTime.String())

		stats.Add(fs)
	}
	return nil
}
