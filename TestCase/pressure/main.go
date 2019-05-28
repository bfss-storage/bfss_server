package main

import (
	"fmt"
	"os"
	"path"
	"pressure/app"
	"pressure/ipc"
	"pressure/test"
	"pressure/web"
	"runtime"
	"runtime/pprof"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"

	_ "net/http/pprof"
)

var wg sync.WaitGroup

func init() {
	log.SetReportCaller(true)
	f, _ := os.Create(fmt.Sprintf("pressure-%d.log", os.Getpid()))
	log.SetOutput(f)
}

func main() {
	if err := app.ParseArgs(); err != nil {
		log.Fatal(err)
	}
	if app.Cfg.Ipc {
		app.Cfg.IpcAddr = ipc.StartServer()
	}
	if app.Cfg.Web {
		if err := web.Start(); err != nil {
			log.Fatal(err)
		}
	} else {
		if app.Cfg.IpcClient {
			ipc.DoInit()
		}

		doTest()
	}
}

func doTest() {
	var statsList []*app.PerfStats

	log.Info("Testing ...")

	if app.Cfg.IpcClient {
		time.Sleep(time.Second * 5)
		ipc.DClient().FetchCfg(&app.Cfg)
		ipc.DClient().UpdateStatus(app.Ready)
	}

	level, err := log.ParseLevel(app.Cfg.LogLevel)
	if err != nil {
		level = log.InfoLevel
	}
	log.SetLevel(level)

	stats := &app.PerfStats{
		Sample:         app.Cfg.Sample,
		MaxSlice:       app.Cfg.MaxSlice,
		ExpectedWrites: app.Cfg.ExpectedWrites,
		CacheSize:      app.Cfg.CacheSize,
	}
	if app.Cfg.EnableConcurrent {
		stats.Routines = app.Cfg.Routines
	}

	log.Infof("ExpectedWrites:%v,Sample:%v", app.Cfg.ExpectedWrites, app.Cfg.Sample)
	handleTest(stats)

	statsList = append(statsList, stats)
	log.Info("Complete Test.")

	log.Info("Exporting ...")
	app.ExportChartStats(statsList)

	if app.Cfg.IpcClient {
		ipc.DClient().UpdateStatus(app.Done)
	}
}

func handleTest(stats *app.PerfStats) {
	// cpuprofile
	if app.Cfg.Profile {

		fileName := time.Now().Format("2006-01-02_15.04.05.prof")
		fileName = fmt.Sprintf("cpu[ExpectedWrites:%v,Sample:%s]-%s",
			app.ByteCountDecimal(stats.ExpectedWrites), stats.Sample.String(), fileName)
		fullPath := path.Join(app.Cfg.DataDir, fileName)
		f, err := os.Create(fullPath)
		if err != nil {
			log.Fatal("could not create CPU profile: ", err)
		}
		defer f.Close()
		if err := pprof.StartCPUProfile(f); err != nil {
			log.Fatal("could not start CPU profile: ", err)
		}
		defer pprof.StopCPUProfile()
	}

	if app.Cfg.EnableConcurrent {
		parallelTest(stats)
	} else {
		oneshotTest(stats)
	}

	if app.Cfg.VerifyHash {
		test.CompleteObject(stats)
		test.VerifyHash(stats)
	}

	if app.Cfg.IpcClient {
		ipc.DClient().CommitStats(stats)
	}

	runtime.GC() // get up-to-date statistics
	// memprofile
	if app.Cfg.Profile {

		fileName := time.Now().Format("2006-01-02_15.04.05.prof")
		fileName = fmt.Sprintf("mem[ExpectedWrites:%v,Sample:%s]-%s",
			app.ByteCountDecimal(stats.ExpectedWrites), stats.Sample.String(), fileName)
		fullPath := path.Join(app.Cfg.DataDir, fileName)
		f, err := os.Create(fullPath)
		if err != nil {
			log.Fatal("could not create memory profile: ", err)
		}
		defer f.Close()
		if err := pprof.WriteHeapProfile(f); err != nil {
			log.Fatal("could not write memory profile: ", err)
		}
	}
}

func oneshotTest(stats *app.PerfStats) {
	startTime := time.Now()
	if app.Cfg.Fast {
		test.FastWrite(stats)
	} else {
		test.Write(stats)
	}

	stats.TestRunTime = time.Now().Sub(startTime)

	log.Infof(`TestRunTime:%s, TotalCreateObjectTime:%s, TotalWriteTime:%s,
		TotalCompleteObjectTime:%s, TotalWrites:%v, TotalWriteCount:%v, Files:%v`,
		stats.TestRunTime, stats.TotalCreateObjectTime.String(), stats.TotalWriteTime.String(),
		stats.TotalCompleteObjectTime.String(), stats.TotalWrites, stats.TotalWriteCount, len(stats.Files))
}

func parallelTest(stats *app.PerfStats) {

	list := app.SplitPerfStats(stats)
	startTime := time.Now()

	for i := 0; i < stats.Routines; i++ {
		wg.Add(1)
		go func(stats *app.PerfStats, i int) {
			stats.Tag = i
			defer wg.Done()
			if app.Cfg.Fast {
				test.FastWrite(stats)
			} else {
				test.Write(stats)
			}
		}(list[i], i)
	}
	wg.Wait()

	stats.TestRunTime = time.Now().Sub(startTime)
	if err := app.MergePerfStats(stats, list); err != nil {
		log.Error(err)
	}

	log.Infof(`TestRunTime:%s, TotalCreateObjectTime:%s, TotalWriteTime:%s,
		TotalCompleteObjectTime:%s, TotalWrites:%v, TotalWriteCount:%v, Files:%v`,
		stats.TestRunTime, stats.TotalCreateObjectTime.String(), stats.TotalWriteTime.String(),
		stats.TotalCompleteObjectTime.String(), stats.TotalWrites, stats.TotalWriteCount, len(stats.Files))
}
