package test

import (
	"pressure/rpc"
	"sync"
	"time"

	"github.com/sirupsen/logrus"
)

const loops = 4

// asyncFile gen test files asynchronously.
type asyncFile struct {
	readChan   chan *sliceFile
	createChan chan *sliceFile
	doneChan   chan bool

	totalReadTime   time.Duration
	totalCreateTime time.Duration
	wg              sync.WaitGroup
}

// newAsyncFile new an instance of asyncFile with files and a slice cache.
func newAsyncFile(files []*sliceFile) *asyncFile {
	clen := 1
	if len(files) > 0 {
		clen = len(files)
	}
	af := new(asyncFile)
	af.readChan = make(chan *sliceFile, clen)
	af.createChan = make(chan *sliceFile, clen)
	af.doneChan = make(chan bool, loops)

	for _, f := range files {
		af.readChan <- f
	}

	af.wg.Add(loops)
	for i := 0; i < loops; i++ {
		go af.loop()
	}

	return af
}

// read gets all created sliceFiles
func (a *asyncFile) read() []*sliceFile {
	t := time.Now()
	var files []*sliceFile
	for {
		select {
		case f := <-a.readChan:
			files = append(files, f)
		default:
			d := time.Since(t)
			a.totalReadTime += d
			logrus.Debugf("ReadChan=> Time:%s", d.String())
			return files
		}
	}
}

func (a *asyncFile) create(file *sliceFile) {
	a.createChan <- file
}

func (a *asyncFile) done() {
	for i := 0; i < loops; i++ {
		a.doneChan <- true
	}

	a.wg.Wait()

	close(a.readChan)
	close(a.createChan)
	close(a.doneChan)
}

func (a *asyncFile) loop() {
	defer a.wg.Done()
	client, err := rpc.NewClient()
	if err != nil {
		logrus.Error(err)
		return
	}

	for {
		select {
		case f := <-a.createChan:
			t := time.Now()
			err := genFile(client, f)
			if err != nil {
				continue
			}
			logrus.Debugf("Created:%s,%v", f.Name, f.Size)
			a.readChan <- f
			d := time.Since(t)
			a.totalCreateTime += d
			logrus.Debugf("CreateChan=> Time:%s", d.String())
		case <-a.doneChan:
			logrus.Infof("asyncFile Done=> TotalCreateTime:%s, TotalReadTime:%s",
				a.totalCreateTime.String(), a.totalReadTime.String())
			return
		}
	}
}
