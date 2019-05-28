package web

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"pressure/app"
	"pressure/web/handler"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

var router *gin.Engine

func init() {
	router = setupRouter()
}

func TestNewTask(t *testing.T) {

	t.Run("InvalidJSON", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"t",
				"process_count":1,
				"sample":"S100K",
				"expected_writes":"10G"
			`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeError, resp["code"])
	})

	t.Run("InvalidParams-TaskName", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"",
				"process_count":1,
				"sample":"S100K",
				"expected_writes":"10G"
			}`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeInvalidParam, resp["code"])
	})

	t.Run("InvalidParams-ProcessCount", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"t",
				"process_count":0,
				"sample":"S100K",
				"expected_writes":"10G"
			}`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeInvalidParam, resp["code"])
	})

	t.Run("InvalidParams-Strategy", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"t",
				"process_count":1,
				"sample":"sss",
				"expected_writes":"10G"
			}`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeInvalidParam, resp["code"])
	})

	t.Run("InvalidParams-ExpectedWrites", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"t",
				"process_count":1,
				"sample":"S100K",
				"expected_writes":"0"
			}`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeInvalidParam, resp["code"])
	})

	t.Run("TaskCreated", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodPost, "/api/v1/tasks",
			strings.NewReader(`{
				"name":"t",
				"process_count":1,
				"sample":"S100K",
				"expected_writes":"1G"
			}`))

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeOK, resp["code"])
	})
}

func TestQueryTask(t *testing.T) {

	t.Run("EmptyName", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodGet, "/api/v1/tasks/", nil)

		router.ServeHTTP(w, req)

		require.Equal(t, http.StatusNotFound, w.Code)
	})

	t.Run("InvalidName", func(t *testing.T) {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodGet, "/api/v1/tasks/1", nil)

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp gin.H
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeInvalidParam, resp["code"])
	})

	t.Run("Success", func(t *testing.T) {
		cfg := &app.Config{
			SampleSet:         []app.Strategy{app.S100K},
			ExpectedWritesSet: []int64{1024},
		}
		task, err := app.NewTask("t", cfg, 1)
		require.Nil(t, err, err)
		task.Units = append(task.Units, &app.Process{
			SampleSet:         cfg.SampleSet,
			ExpectedWritesSet: cfg.ExpectedWritesSet,
			TargetProgress:    50,
			RoutineProgress:   100,
			Stats: &app.PerfStats{
				TotalWriteTime: time.Minute * 10,
				TotalWrites:    1024,
			},
		})

		w := httptest.NewRecorder()
		req, _ := http.NewRequest(http.MethodGet, "/api/v1/tasks/t", nil)

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		type respData struct {
			Code string          `json:"code,omitempty"`
			Msg  string          `json:"msg,omitempty"`
			Data json.RawMessage `json:"data,omitempty"`
		}
		var resp respData
		err = json.Unmarshal(w.Body.Bytes(), &resp)
		require.Nil(t, err, err)
		assert.Equal(t, handler.CodeOK, resp.Code)

		var dt app.Task
		err = json.Unmarshal(resp.Data, &dt)
		require.Nil(t, err, err)

		assert.Equal(t, "t", dt.Name)
		assert.Equal(t, 50, dt.Units[0].TargetProgress)
		assert.Equal(t, 100, dt.Units[0].RoutineProgress)
	})
}
