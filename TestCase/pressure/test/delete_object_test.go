package test

import (
	"pressure/app"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/stretchr/testify/require"
)

func Test_deleteObject(t *testing.T) {
	stats := &app.PerfStats{
		ExpectedWrites: 1024 * 1024 * 10,
		Sample:         app.S1M,
	}

	app.Cfg.Server = "10.0.1.119:30000"
	err := Write(stats)
	require.Nil(t, err, err)

	t.Run("deleteObject", func(t *testing.T) {
		err := DeleteObject(stats)
		assert.Nil(t, err, err)
	})
}
