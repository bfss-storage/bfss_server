package handler

import (
	"fmt"
	"net/http"
	"pressure/app"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/sirupsen/logrus"
)

// EOK for response
const (
	CodeOK            = "E_OK"
	CodeError         = "E_Error"
	CodeInvalidParam  = "E_Invalid_Params"
	CodeInvalidStatus = "E_Invalid_Status"
	CodeNoTask        = "E_No_Task"
	CodeNoReadyTask   = "E_No_Ready_Task"
)

type taskConfig struct {
	Name           string `json:"name,omitempty"`
	ProcessCount   int    `json:"process_count,omitempty"`
	Sample         string `json:"sample,omitempty"`
	ExpectedWrites string `json:"expected_writes,omitempty"`
	Server         string `json:"server,omitempty"`
	LogLevel       string `json:"log_level,omitempty"`
	CacheSize      string `json:"cache_size,omitempty"`
}

func validateConfig(tc *taskConfig) (app.Strategy, int64, error) {
	if len(strings.TrimSpace(tc.Name)) == 0 {
		msg := fmt.Errorf("Task name empty")
		return app.SMax, 0, msg
	}

	if tc.ProcessCount <= 0 {
		msg := fmt.Errorf("Invalid ProcessCount=> %v", tc.ProcessCount)
		return app.SMax, 0, msg
	}

	s, err := app.StrategyFromString(tc.Sample)
	if err != nil {
		return app.SMax, 0, err
	}

	expectedWrites := app.ParseBytes(tc.ExpectedWrites)
	if expectedWrites <= 0 {
		msg := fmt.Errorf("Invalid ExpectedWrites=> %v", tc.ExpectedWrites)
		return app.SMax, 0, msg
	}

	cs := app.ParseBytes(tc.CacheSize)
	if cs <= 0 {
		msg := fmt.Errorf("Invalid CacheSize=> %v", tc.CacheSize)
		return app.SMax, 0, msg
	}

	if len(strings.TrimSpace(tc.Server)) == 0 {
		msg := fmt.Errorf("Invalid server")
		return app.SMax, 0, msg
	}

	return s, expectedWrites, nil
}

// NewTask handles a new task request.
func NewTask(c *gin.Context) {
	var tc taskConfig
	if err := c.ShouldBindJSON(&tc); err != nil {
		logrus.Error(err)
		c.JSON(http.StatusOK, gin.H{
			"code": CodeError,
			"msg":  fmt.Sprintf("%s", err),
		})
		return
	}
	sample, expectedWrites, err := validateConfig(&tc)
	if err != nil {
		logrus.Error(err)
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidParam,
			"msg":  fmt.Sprintf("%s", err),
		})
		return
	}
	cfg := &app.Config{}
	cfg.Sample = sample
	cfg.ExpectedWrites = expectedWrites
	cfg.Server = tc.Server
	cfg.LogLevel = tc.LogLevel
	cfg.CacheSize = app.ParseBytes(tc.CacheSize)

	_, err = app.NewTask(strings.TrimSpace(tc.Name), cfg, tc.ProcessCount)
	if err != nil {
		logrus.Error(err)
		c.JSON(http.StatusOK, gin.H{
			"code": CodeError,
			"msg":  fmt.Sprintf("%s", err),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code": CodeOK,
		"msg":  "success",
	})
}

// AllTasks gets all tasks.
func AllTasks(c *gin.Context) {
	t := app.AllTasks()

	if t == nil {
		t = make([]*app.Task, 0)
	}
	c.JSON(http.StatusOK, gin.H{
		"code": CodeOK,
		"msg":  "success",
		"data": t,
	})
}

// QueryTask gets a task status
func QueryTask(c *gin.Context) {
	name, ok := c.GetQuery("name")
	if !ok {
		AllTasks(c)
		return
	}
	name = strings.TrimSpace(name)
	t := app.FindTask(name)
	if t == nil {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidParam,
			"msg":  "Invalid Task name",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code": CodeOK,
		"msg":  "success",
		"data": t,
	})
}

func startAllTasks(c *gin.Context) {
	tasks := app.AllTasks()

	if tasks == nil || len(tasks) == 0 {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeNoTask,
			"msg":  "No Schedule Task",
		})
		return
	}

	var runs int
	for _, t := range tasks {
		if t.Status != app.Ready {
			continue
		}
		runs++
		app.AddSchedule(t)
	}
	if runs == 0 {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeNoReadyTask,
			"msg":  "No Ready Task",
		})
	} else {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeOK,
			"msg":  "success",
			"data": runs,
		})
	}
}

// StartTask starts a task by name.
func StartTask(c *gin.Context) {
	name, ok := c.GetQuery("name")
	if !ok {
		startAllTasks(c)
		return
	}
	name = strings.TrimSpace(name)

	if len(name) == 0 {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidParam,
			"msg":  "Task name is empty",
		})
		return
	}

	t := app.FindTask(name)
	if t == nil {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidParam,
			"msg":  "Invalid Task name",
		})
		return
	}
	if err := t.Start(); err != nil {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeError,
			"msg":  fmt.Sprintf("%s", err),
		})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"code": CodeOK,
		"msg":  "success",
	})
}

func writeStats(c *gin.Context, name string, stats []*app.PerfStats) {
	downloadName := fmt.Sprintf("perf[%s]-", name) + time.Now().Format("20060102150405") + ".xlsx"
	header := c.Writer.Header()
	header["Content-type"] = []string{"application/octet-stream"}
	header["Content-Disposition"] = []string{"attachment; filename= " + downloadName}
	if err := app.ExportStatsToWriter(stats, c.Writer); err != nil {
		logrus.Error(err)
		c.JSON(http.StatusOK, gin.H{
			"code": CodeError,
			"msg":  fmt.Sprintf("%s", err),
		})
	}
}

func exportAll(c *gin.Context) {
	tasks := app.AllTasks()
	var statsList []*app.PerfStats
	for _, t := range tasks {
		if t.Status != app.Done {
			continue
		}
		statsList = append(statsList, t.GetStats()...)
	}
	writeStats(c, "ALL", statsList)
}

// ExportStats export stats
func ExportStats(c *gin.Context) {
	name, ok := c.GetQuery("name")
	if !ok {
		exportAll(c)
		return
	}
	name = strings.TrimSpace(name)

	t := app.FindTask(name)
	if t == nil {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidParam,
			"msg":  "Invalid Task name",
		})
		return
	}
	if t.Status != app.Done {
		c.JSON(http.StatusOK, gin.H{
			"code": CodeInvalidStatus,
			"msg":  "Task not done",
		})
		return
	}
	writeStats(c, name, t.GetStats())
}

type taskSummary struct {
	Name           string       `json:"name"`
	ProcessCount   int          `json:"process_count"`
	Speed          float64      `json:"speed"`
	ExpectedWrites int64        `json:"expected_writes"`
	Sample         app.Strategy `json:"sample"`
	CreatedAt      int64        `json:"created_at"`
	StartedAt      int64        `json:"started_at"`
	DoneAt         int64        `json:"done_at"`

	TotalFiles      int           `json:"total_files"`
	TotalWriteTime  time.Duration `json:"total_write_time"`
	TotalWrites     int64         `json:"total_writes"`
	TotalWriteCount int64         `json:"total_write_count"`
}

// QueryTaskSummary gets summaries of tasks.
func QueryTaskSummary(c *gin.Context) {
	tasks := app.AllTasks()
	var summaries []*taskSummary
	for _, t := range tasks {
		if t.Status != app.Done {
			continue
		}
		var stats app.PerfStats
		allStats := t.GetStats()
		app.MergePerfStats(&stats, allStats)

		s := &taskSummary{
			Name:           t.Name,
			ProcessCount:   t.ProcessCount,
			Speed:          t.Speed,
			ExpectedWrites: t.Cfg.ExpectedWrites,
			Sample:         t.Cfg.Sample,
			CreatedAt:      t.CreatedAt,
			StartedAt:      t.StartedAt,
			DoneAt:         t.DoneAt,

			TotalFiles:      len(stats.Files),
			TotalWriteTime:  stats.TotalWriteTime,
			TotalWrites:     stats.TotalWrites,
			TotalWriteCount: stats.TotalWriteCount,
		}
		summaries = append(summaries, s)
	}
	if summaries == nil {
		summaries = make([]*taskSummary, 0)
	}
	c.JSON(http.StatusOK, gin.H{
		"code": CodeOK,
		"msg":  "success",
		"data": summaries,
	})
}
