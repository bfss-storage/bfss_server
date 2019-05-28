package test

import (
	"context"
	"fmt"
	"pressure/app"
	"pressure/rpc"
	"pressure/thrift/bfss"
	"time"

	"github.com/sirupsen/logrus"
)

// CompleteObject test CompleteObject rpc.
func CompleteObject(stats *app.PerfStats) error {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	for _, f := range stats.Files {

		logrus.Debugf("CompleteObject=> Name:%s,Size:%v,Hash:%s", f.Name, f.Size, f.Hash)

		t := time.Now()
		r, err := client.CompleteObject(context.Background(), f.Name)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", f.Name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, f.Name)
		}
		f.CompleteObjectTime = time.Since(t)
		stats.TotalCompleteObjectTime += f.CompleteObjectTime
	}
	logrus.Infof("CompleteObject=> Files:%v, TotalTime:%s", len(stats.Files), stats.TotalCompleteObjectTime.String())
	return nil
}
