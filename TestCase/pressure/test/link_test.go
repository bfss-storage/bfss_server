package test

import (
	"pressure/app"
	"testing"

	"github.com/stretchr/testify/assert"
)

func Test_createObjectLink(t *testing.T) {
	stats := &app.PerfStats{
		ExpectedFiles: 10,
	}

	app.Cfg.Server = "10.0.1.119:30000"
	t.Run("createObjectLink", func(t *testing.T) {
		err := CreateObjectLink(stats)
		assert.Nil(t, err, err)
	})
}
