package test

import (
	"pressure/app"
	"testing"

	"github.com/sirupsen/logrus"

	"github.com/stretchr/testify/assert"
)

func Test_genFileSize(t *testing.T) {
	size := genFileSize(app.S100K)
	assert.Greaterf(t, size, int32(0), "genFileSize=> %v", size)
}

func Test_createObject(t *testing.T) {
	stats := &app.PerfStats{
		ExpectedFiles: 100,
	}
	t.Run("ShouldLogError", func(t *testing.T) {
		err := CreateObject(stats)
		assert.NotNil(t, err, "")
	})

	t.Run("ShouldSucceed", func(t *testing.T) {
		logrus.SetLevel(logrus.DebugLevel)
		app.Cfg.Server = "127.0.0.1:9092"
		err := CreateObject(stats)
		assert.Nilf(t, err, "createObject=> %v", err)

		logrus.Infof("CreateTime:%s, Files:%v", stats.TotalCreateObjectTime, len(stats.Files))
	})

}
