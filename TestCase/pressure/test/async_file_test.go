package test

import (
	"math/rand"
	"pressure/app"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func Test_newAsyncFile(t *testing.T) {
	var files []*sliceFile
	files = append(files, &sliceFile{Name: "test1", Size: 1024, Data: make([]byte, 1024)})
	files = append(files, &sliceFile{Name: "test2", Size: 1024, Data: make([]byte, 1024)})

	for _, f := range files {
		rand.Read(f.Data)
		f.Head = make([]byte, 16)
		copy(f.Head, f.Data[:16])
	}

	app.Cfg.Server = "10.0.1.119:30000"
	//client, err := rpc.NewClient()
	//require.Nil(t, err, err)

	t.Run("newAsyncFile", func(t *testing.T) {
		f := newAsyncFile(files)
		defer f.done()

		fs := f.read()
		assert.NotNil(t, fs)
		assert.Equal(t, len(files), len(fs))

		f.create(fs[0])
		time.Sleep(5 * time.Second)
		fs2 := f.read()
		assert.Equal(t, 1, len(fs2))
		// f.done()
	})
	time.Sleep(time.Second)
}
