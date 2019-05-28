package test

import (
	"context"
	"crypto/sha256"
	"fmt"
	"math/rand"
	"pressure/app"
	"pressure/ipc"
	"pressure/rpc"
	"pressure/thrift/BFSS_API"
	"pressure/thrift/bfss"
	"time"

	uuid "github.com/satori/go.uuid"

	"github.com/sirupsen/logrus"
)

// UUIDFileName generates a uuid as filename.
func UUIDFileName() string {
	return uuid.NewV4().String()
}

// writeFile break file into slices and write slices to server
func writeFile(
	client *BFSS_API.BFSS_APIDClient,
	fs *app.FileStats, slice *sliceCache) (bfss.BFSS_RESULT, error) {

	// fill head with file name
	genSliceFileBytes(slice)
	copy(slice.Data, []byte(fs.Name))

	// Save head in order to verify hash consistency.
	fs.Head = make([]byte, 16)
	copy(fs.Head, slice.Data[:16])

	h := sha256.New()

	var writes int32
	var writeTime time.Duration
	for {
		offset := slice.Size
		remain := fs.Size - int64(writes)
		if remain < slice.Size && remain > 0 {
			offset = remain
		}
		logrus.Debugf("Remain:%v", remain)

		// calc data hash
		h.Write(slice.Data[:offset])

		t := time.Now()
		r, err := client.Write(context.Background(), fs.Name, writes, slice.Data[:offset])
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			return r, err
		}
		writeTime += time.Now().Sub(t)
		fs.WriteCount++

		logrus.Debug("Slice:", slice.Size, " WriteTime:", writeTime)
		writes += int32(offset)
		if writes >= int32(fs.Size) {
			break
		}

		genSliceFileBytes(slice)
	}
	fs.Hash = fmt.Sprintf("%x", h.Sum(nil))
	fs.WriteTime = writeTime
	return bfss.BFSS_RESULT_BFSS_SUCCESS, nil
}

func doneRoutine(stats *app.PerfStats) {
	speed := 0.0
	if stats.TotalWriteTime > 0 {
		speed = float64(stats.TotalWrites) / float64(stats.TotalWriteTime.Seconds())
	}
	ipc.DClient().UpdateProgress(stats.Tag, 100, speed)
}

// Write writes files to bfss and gives statistics.
func Write(stats *app.PerfStats) error {
	if app.Cfg.IpcClient {
		defer doneRoutine(stats)
	}

	if stats.ExpectedWrites <= 0 {
		logrus.Error("ExpectedWrites must be positive")
		return fmt.Errorf("ExpectedWrites must be positive")
	}
	logrus.Infof("Got ExpectedWrites: %s", app.ByteCountDecimal(stats.ExpectedWrites))

	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}
	logrus.Debug("Start Writing ...")
	tag := fmt.Sprintf("[Write][%s]", time.Now().String())
	slice := newSliceCache(int64(stats.MaxSlice))
	for {
		if stats.TotalWrites >= stats.ExpectedWrites {
			break
		}

		if app.Cfg.IpcClient {
			updateRoutine(stats)
		}

		size := genFileSize(stats.Sample)
		name := UUIDFileName()

		logrus.Debugf("Name:%s,Size:%v,PreparedSize:%v,Sample:%v",
			name, size, stats.TotalPreparedWrites, stats.Sample)

		fs := &app.FileStats{
			Name:    name,
			Size:    int64(size),
			StartAt: time.Now(),
		}

		t1 := time.Now()
		r, err := client.CreateObject(context.Background(), name, size, 0, tag)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, name)
		}
		fs.CreateObjectTime = time.Now().Sub(t1)

		// write file in the way of fixed slice.
		r, err = writeFile(client, fs, slice)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, name)
		}

		t3 := time.Now()
		r, err = client.CompleteObject(context.Background(), name)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Error(r.String(), ", ", err, ", Name:", name)
			return fmt.Errorf("%s,%s,Name:%s", r.String(), err, name)
		}
		fs.CompleteObjectTime = time.Now().Sub(t3)

		stats.Add(fs)
	}
	rpc.CloseClient(client)
	return nil
}

type sliceFile struct {
	Name             string
	Offset           int64
	Size             int64
	Hash             string
	Head             []byte
	Data             []byte
	CreateObjectTime time.Duration
}

func genFile(client *BFSS_API.BFSS_APIDClient, fs *sliceFile) error {
	name := UUIDFileName()
	rand.Seed(time.Now().UnixNano())
	rand.Read(fs.Data)
	fs.Name = name
	if app.Cfg.VerifyHash {
		fs.Hash = hashValue(fs.Data)
		copy(fs.Head, fs.Data[:16])
	}

	logrus.Debugf("Name:%s,Size:%v,Hash:%v",
		name, fs.Size, fs.Hash)

	tag := fmt.Sprintf("[FastWrite][%s]", time.Now().String())
	t1 := time.Now()
	r, err := client.CreateObject(context.Background(), name, int32(fs.Size), 0, tag)
	if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
		msg := fmt.Sprintf("%s,%s,Name:%s,Size:%v", r.String(), err, fs.Name, fs.Size)
		logrus.Error(msg)
		return fmt.Errorf(msg)
	}
	fs.CreateObjectTime = time.Since(t1)
	return nil
}

func genCacheFiles(client *BFSS_API.BFSS_APIDClient, stats *app.PerfStats) ([]*sliceFile, error) {
	var files []*sliceFile

	var genSize int64

	for {
		size := genFileSize(stats.Sample)
		genSize += int64(size)
		if genSize > stats.CacheSize {
			genSize -= int64(size)
			break
		}
		fs := &sliceFile{
			Size: int64(size),
			Data: make([]byte, size),
			Head: make([]byte, 16),
		}
		err := genFile(client, fs)
		if err != nil {
			logrus.Error(err)
			return nil, err
		}
		files = append(files, fs)
	}
	logrus.Infof("genCacheFiles=> TotalSize:%s, Files:%v", app.ByteCountDecimal(genSize), len(files))
	return files, nil
}

func fastWriteFile(client *BFSS_API.BFSS_APIDClient, fs *app.FileStats,
	sfile *sliceFile, maxSlice int32) (bfss.BFSS_RESULT, error) {

	len := maxSlice // 1MB
	var writes int32
	var writeTime time.Duration
	for {
		remain := fs.Size - int64(writes)
		if remain < int64(len) && remain > 0 {
			len = int32(remain)
		}
		logrus.Debugf("File=>%s Size:%v, Writes:%v, Remain:%v", fs.Name, fs.Size, writes, remain)

		t := time.Now()
		r, err := client.Write(context.Background(), fs.Name, writes, sfile.Data[writes:writes+len])
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			logrus.Errorf("Result:%s, err:%v", r, err)
			return r, err
		}
		writeTime += time.Since(t)
		fs.WriteCount++

		logrus.Debugf("File=>%s Slice:%v, WriteTime:%s", fs.Name, len, writeTime)
		writes += len
		if writes >= int32(fs.Size) {
			break
		}
	}
	fs.WriteTime = writeTime
	return bfss.BFSS_RESULT_BFSS_SUCCESS, nil
}

func updateRoutine(stats *app.PerfStats) {
	progress := float64(stats.TotalWrites) / float64(stats.ExpectedWrites)
	speed := 0.0
	if stats.TotalWriteTime > 0 {
		speed = float64(stats.TotalWrites) / float64(stats.TotalWriteTime.Seconds())
	}

	ipc.DClient().UpdateProgress(stats.Tag, int(progress*100), speed)
	ipc.DClient().UpdateStatus(app.Running)
	logrus.Debugf("Routine=> Tag:%v, Progress:%v", stats.Tag, progress)
}

// FastWrite writes files to bfss and gives statistics.
// This function uses pre-generated files to write to server.
func FastWrite(stats *app.PerfStats) error {
	if app.Cfg.IpcClient {
		defer doneRoutine(stats)
	}

	if stats.ExpectedWrites <= 0 {
		logrus.Error("ExpectedWrites must be positive")
		return fmt.Errorf("ExpectedWrites must be positive")
	}

	if stats.CacheSize < 1024*1024*10 {
		logrus.Error("CacheSize is too small")
		return fmt.Errorf("CacheSize is too small")
	}

	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return err
	}

	// sliceFile := newSliceCache(stats.CacheSize)
	files, err := genCacheFiles(client, stats)
	if err != nil {
		logrus.Error(err)
		return err
	}
	af := newAsyncFile(files)
	defer af.done()

	done := false
	for {
		logrus.Debug("begin read ...")
		files = af.read()
		logrus.Debugf("done read=> files:%v", len(files))

		for _, f := range files {
			if stats.TotalWrites >= stats.ExpectedWrites {
				done = true
				break
			}

			if app.Cfg.IpcClient {
				updateRoutine(stats)
			}

			fs := &app.FileStats{
				Name:             f.Name,
				Size:             f.Size,
				Hash:             f.Hash,
				Head:             f.Head,
				CreateObjectTime: f.CreateObjectTime,
				StartAt:          time.Now(),
			}

			// write file in the way of fixed slice.
			r, err := fastWriteFile(client, fs, f, stats.MaxSlice)
			if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
				msg := fmt.Sprintf("%s,%s,Name:%s,Size:%v,Offset:%v", r.String(), err, f.Name, f.Size, f.Offset)
				logrus.Error(msg)
				return fmt.Errorf(msg)
			}

			// Create a new one.
			af.create(f)

			stats.Add(fs)
		}

		if done {
			break
		}
	}

	rpc.CloseClient(client)
	return nil
}

// VerifyHash validates files' consistency with files of remote server.
func VerifyHash(stats *app.PerfStats) {
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return
	}

	for _, v := range stats.Files {
		r, err := client.ObjectLockHasHash(context.Background(), v.Hash, int32(v.Size), v.Head)
		if err != nil || r != bfss.BFSS_RESULT_BFSS_SUCCESS {
			v.HashVerified = fmt.Sprintf("%s %v => OId:%s, Hash:%s, Size:%v, Head:%x",
				r.String(), err, v.Name, v.Hash, v.Size, v.Head)
			logrus.Error(v.HashVerified)
			continue
		}
		v.HashVerified = "true"
	}
}
