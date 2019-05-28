package test

import (
	"context"
	"crypto/md5"
	"fmt"
	"pressure/app"
	"pressure/rpc"
	"pressure/thrift/bfss"
	"time"

	"github.com/sirupsen/logrus"
)

func hashValue(data []byte) string {
	h := md5.New()
	h.Write(data)
	return fmt.Sprintf("%x", h.Sum(nil))
}

// CreateObjectLink test CreateObjectLink rpc.
func CreateObjectLink(stats *app.PerfStats) error {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	tag := fmt.Sprintf("[Perf_CreateObjectLink][%s]", time.Now().String())
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
		logrus.Debugf("CreateObjectLink=> Name:%s,Size:%v,Strategy:%s", name, size, strategy.String())

		t := time.Now()
		r, err := client.CreateObjectLink(context.Background(), name, hashValue([]byte(name)), size, []byte(name)[:16], 0, tag)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, name)
		}
		fs.CreateObjectLinkTime = time.Now().Sub(t)
		logrus.Debugf("CreateObjectLink=> Name:%s,Size:%v,CreateLinkTime:%s", name, size, fs.CreateObjectLinkTime.String())

		stats.Add(fs)
	}
	return nil
}
