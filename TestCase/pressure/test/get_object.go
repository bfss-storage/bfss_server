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

// GetObjectInfo test GetObjectInfo rpc.
func GetObjectInfo(stats *app.PerfStats) error {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	for _, f := range stats.Files {
		logrus.Debugf("Local=> GetObjectInfo=> Name:%s,Size:%v,Hash:%s", f.Name, f.Size, f.Hash)

		t := time.Now()
		r, err := client.GetObjectInfo(context.Background(), f.Name)
		if err != nil || r.GetResult_() != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", f.Name)
			return fmt.Errorf("%s,%s,Name:%s", r.GetResult_().String(), err, f.Name)
		}
		stats.TotalGetObjectInfoTime += time.Now().Sub(t)
		logrus.Debugf("Remote=> GetObjectInfo=> Name:%s,HashCompare:%v,Tag:%s", f.Name, r.ObjectInfo.Hash == f.Hash, r.ObjectInfo.GetObjectTag())
	}
	logrus.Infof("GetObjectInfo=> Files:%v, TotalTime:%s", len(stats.Files), stats.TotalGetObjectInfoTime.String())
	return nil
}
