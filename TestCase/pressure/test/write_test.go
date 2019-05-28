package test

import (
	"context"
	"fmt"
	"pressure/app"
	"pressure/rpc"
	"pressure/thrift/bfss"
	"testing"
	"time"

	"github.com/mongodb/mongo-go-driver/mongo/options"
	"github.com/sirupsen/logrus"
	"github.com/stretchr/testify/require"
	"go.mongodb.org/mongo-driver/mongo"
)

func init() {
	setup()
}

func setup() {
	logrus.SetReportCaller(true)
	logrus.SetLevel(logrus.DebugLevel)

	app.Cfg.Server = "127.0.0.1:9092"
	app.Cfg.MaxSlice = 1024 * 1024
}

func TestDeleteAllDatabases(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	mclient, err := mongo.Connect(ctx, "mongodb://localhost:27017", options.Client())
	require.Nil(t, err)
	cancel()

	if err := mclient.Database("BKeyDb").Drop(context.Background()); err != nil {
		t.Error(err)
	}
	if err := mclient.Database("ObjectDb").Drop(context.Background()); err != nil {
		t.Error(err)
	}
	if err := mclient.Database("VolumeDb").Drop(context.Background()); err != nil {
		t.Error(err)
	}
}

func TestWrite(t *testing.T) {
	stats := &app.PerfStats{
		ExpectedWrites: 1024 * 1024 * 20,
		Sample:         app.S1M,
	}

	Write(stats)
	logrus.Info("Count:", len(stats.Files), ", TotalWrites:", stats.TotalWrites)
}

func BenchmarkWrite(b *testing.B) {
	stats := &app.PerfStats{
		ExpectedWrites: 1024 * 1024 * 2000,
		Sample:         app.S1M,
	}
	startTime := time.Now()
	b.Run("Write", func(b *testing.B) {
		Write(stats)
	})
	logrus.Info("RunningTime:", time.Now().Sub(startTime))
	logrus.Info("TotalCreateObjectTime:", stats.TotalCreateObjectTime,
		", TotalWriteTime:", stats.TotalWriteTime,
		", TotalCompleteObjectTime:", stats.TotalCompleteObjectTime,
		", TotalWrites:", stats.TotalWrites, ", Files:", len(stats.Files))
}

func BenchmarkWrite_Parallel(b *testing.B) {
	stats := &app.PerfStats{
		ExpectedWrites: 1024 * 1024 * 2000,
		Sample:         app.S10M,
	}
	app.Cfg.Server = "10.0.1.119:30000"
	startTime := time.Now()
	b.RunParallel(func(b *testing.PB) {
		for b.Next() {
			Write(stats)
		}
	})
	logrus.Info("RunningTime:", time.Now().Sub(startTime))
	logrus.Info("TotalCreateObjectTime:", stats.TotalCreateObjectTime,
		", TotalWriteTime:", stats.TotalWriteTime,
		", TotalCompleteObjectTime:", stats.TotalCompleteObjectTime,
		", TotalWrites:", stats.TotalWrites, ", Files:", len(stats.Files))
}

func Test_writeFile(t *testing.T) {
	stats := &app.PerfStats{
		Sample: app.S10M,
	}

	app.Cfg.Server = "10.0.1.119:30000"

	client, err := rpc.NewClient()
	require.Nil(t, err, err)

	tag := fmt.Sprintf("[Test_writeFile][%s]", time.Now().String())
	slice := newSliceCache(int64(stats.MaxSlice))

	t.Run("write", func(t *testing.T) {

		size := genFileSize(stats.Sample)
		name := UUIDFileName()
		logrus.Debugf("Name:%s,Size:%v,Sample:%v", name, size, stats.Sample)

		fs := &app.FileStats{
			Name:    name,
			Size:    int64(size),
			StartAt: time.Now(),
		}

		t1 := time.Now()
		r, err := client.CreateObject(context.Background(), name, size, 0, tag)
		require.Nil(t, err, err)
		require.Equalf(t, bfss.BFSS_RESULT_BFSS_SUCCESS, r, "CreateObject=> %s, FileName:%s", r.String(), name)

		fs.CreateObjectTime = time.Now().Sub(t1)

		// write file in the way of fixed slice.
		r, err = writeFile(client, fs, slice)
		require.Nil(t, err, err)
		require.Equalf(t, bfss.BFSS_RESULT_BFSS_SUCCESS, r, "writeFile=> %s, FileName:%s", r.String(), name)

		t3 := time.Now()
		r, err = client.CompleteObject(context.Background(), name)
		require.Nil(t, err, err)
		require.Equalf(t, bfss.BFSS_RESULT_BFSS_SUCCESS, r, "CompleteObject=> %s, FileName:%s", r.String(), name)

		fs.CompleteObjectTime = time.Now().Sub(t3)
	})
}

func Test_genCacheFiles(t *testing.T) {
	stats := &app.PerfStats{
		Sample: app.S10M,
	}

	app.Cfg.Server = "10.0.1.119:30000"

	client, err := rpc.NewClient()
	require.Nil(t, err, err)
	//slice := newSliceCache(int64(1024 * 1024 * 100))

	t.Run("genCacheFiles", func(t *testing.T) {
		files, err := genCacheFiles(client, stats)
		require.Nil(t, err, err)
		require.NotEmpty(t, files)
		logrus.Infof("Files:%v", len(files))
	})
}

func Test_fastWriteFile(t *testing.T) {
	stats := &app.PerfStats{
		Sample:    app.S10M,
		MaxSlice:  1024 * 1024,
		CacheSize: 1024 * 1024 * 10,
	}

	app.Cfg.Server = "10.0.1.119:30000"

	client, err := rpc.NewClient()
	require.Nil(t, err, err)
	//slice := newSliceCache(int64(1024 * 1024 * 100))

	t.Run("fastWriteFile", func(t *testing.T) {
		files, err := genCacheFiles(client, stats)
		require.Nil(t, err, err)
		require.NotEmpty(t, files)
		logrus.Infof("Files:%v", len(files))

		for _, f := range files {
			fs := &app.FileStats{
				Name:             f.Name,
				Size:             f.Size,
				Hash:             f.Hash,
				CreateObjectTime: f.CreateObjectTime,
				StartAt:          time.Now(),
			}

			logrus.Infof("fastWriteFile:%s", f.Name)
			r, err := fastWriteFile(client, fs, f, stats.MaxSlice)
			require.Nil(t, err, err)
			require.Equal(t, bfss.BFSS_RESULT_BFSS_SUCCESS, r, r)
		}
	})
}

func TestFastWrite(t *testing.T) {
	stats := &app.PerfStats{
		Sample:         app.S1M,
		CacheSize:      1024 * 1024 * 10,
		ExpectedWrites: 1024 * 1024 * 40,
		MaxSlice:       1024 * 1024,
	}
	// logrus.SetLevel(logrus.InfoLevel)
	app.Cfg.Server = "10.0.1.119:30000"

	t.Run("FastWrite", func(t *testing.T) {
		err := FastWrite(stats)
		require.Nil(t, err, err)
	})
}
