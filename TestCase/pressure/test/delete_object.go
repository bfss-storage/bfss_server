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

// DeleteObject test DeleteObject rpc.
func DeleteObject(stats *app.PerfStats) error {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	for _, f := range stats.Files {

		logrus.Debugf("DeleteObject=> Name:%s,Size:%v,Hash:%s", f.Name, f.Size, f.Hash)

		t := time.Now()
		r, err := client.DeleteObject(context.Background(), f.Name)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", f.Name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, f.Name)
		}
		stats.TotalDeleteObjectTime += time.Now().Sub(t)
	}
	logrus.Infof("DeleteObject=> Files:%v, TotalTime:%s", len(stats.Files), stats.TotalDeleteObjectTime.String())
	return nil
}
