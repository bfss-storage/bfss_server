package app

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"strconv"
	"sync"
	"time"

	"github.com/sirupsen/logrus"
	"github.com/tealeg/xlsx"
)

// FileStats saves all statistic data.
type FileStats struct {
	Name                 string
	Size                 int64
	Hash                 string
	Head                 []byte
	HashVerified         string
	CreateObjectTime     time.Duration
	CreateObjectLinkTime time.Duration
	WriteTime            time.Duration
	CompleteObjectTime   time.Duration
	WriteCount           int64
	StartAt              time.Time
}

// PerfStats for cpu profile statistics.
type PerfStats struct {
	mutex                   sync.Mutex
	Files                   []*FileStats  `json:"-"`
	TotalCreateObjectTime   time.Duration `json:"total_create_object_time,omitempty"`
	TotalWriteTime          time.Duration `json:"total_write_time,omitempty"`
	TotalCompleteObjectTime time.Duration `json:"total_complete_object_time,omitempty"`
	TestRunTime             time.Duration `json:"test_run_time,omitempty"`
	TotalGetObjectInfoTime  time.Duration `json:"total_get_object_info_time,omitempty"`
	TotalDeleteObjectTime   time.Duration `json:"total_delete_object_time,omitempty"`
	TotalWrites             int64         `json:"total_writes,omitempty"`
	TotalWriteCount         int64         `json:"total_write_count,omitempty"`

	TotalPreparedWrites int64 `json:"-"`
	TotalPreparedFiles  int32 `json:"-"`

	ExpectedWrites int64    `json:"expected_writes,omitempty"`
	MaxSlice       int32    `json:"max_slice,omitempty"`
	Sample         Strategy `json:"sample,omitempty"`
	ExpectedFiles  int32    `json:"expected_files,omitempty"`
	Routines       int      `json:"routines,omitempty"`
	CacheSize      int64    `json:"cache_size,omitempty"`
	Tag            int      `json:"tag,omitempty"`
}

// Add cumulates all wirtten files.
func (s *PerfStats) Add(fs *FileStats) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	s.Files = append(s.Files, fs)
	s.TotalCompleteObjectTime += fs.CompleteObjectTime
	s.TotalCreateObjectTime += fs.CreateObjectTime
	s.TotalWriteTime += fs.WriteTime
	s.TotalWrites += fs.Size
	s.TotalWriteCount += fs.WriteCount
}

// AddPreparedWrites calculate prepared file size to control bytes written to server in concurrent case.
func (s *PerfStats) AddPreparedWrites(size int64) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	s.TotalPreparedWrites += size
}

// AddPreparedFiles calculate prepared file count to control files written to server in concurrent case.
func (s *PerfStats) AddPreparedFiles(size int32) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	s.TotalPreparedFiles += size
}

// SplitPerfStats split single PerfStats into multi-PerfStats in order to parallel test.
func SplitPerfStats(stats *PerfStats) []*PerfStats {
	var list []*PerfStats
	if stats.Routines <= 0 {
		list = append(list, stats)
		return list
	}

	expectedWrites := stats.ExpectedWrites / int64(stats.Routines)
	cacheSize := stats.CacheSize / int64(stats.Routines)

	for i := 0; i < stats.Routines; i++ {
		s := &PerfStats{
			ExpectedWrites: expectedWrites + 1024,
			MaxSlice:       stats.MaxSlice,
			Sample:         stats.Sample,
			ExpectedFiles:  stats.ExpectedFiles,
			Routines:       stats.Routines,
			CacheSize:      cacheSize,
		}
		list = append(list, s)
	}
	return list
}

// MergePerfStats merges a list of PerStats into one stats.
func MergePerfStats(stats *PerfStats, list []*PerfStats) error {
	if len(list) == 0 {
		return errors.New("Empty PerfStats")
	}

	for _, s := range list {
		stats.Files = append(stats.Files, s.Files...)
		stats.TotalCreateObjectTime += s.TotalCreateObjectTime
		stats.TotalWriteTime += s.TotalWriteTime
		stats.TotalCompleteObjectTime += s.TotalCompleteObjectTime
		stats.TotalGetObjectInfoTime += s.TotalGetObjectInfoTime
		stats.TotalDeleteObjectTime += s.TotalDeleteObjectTime
		stats.TotalWrites += s.TotalWrites
		stats.TotalWriteCount += s.TotalWriteCount
	}
	return nil
}

// ByteCountDecimal convert bytes into MB,GB or TB.
func ByteCountDecimal(b int64) string {
	const unit = 1024
	if b < unit {
		return fmt.Sprintf("%d B", b)
	}
	div, exp := int64(unit), 0
	for n := b / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	if exp == 0 {
		return fmt.Sprintf("%.3f %cB", float64(b)/float64(div), "kMGTPE"[exp])
	} else if exp == 1 {
		return fmt.Sprintf("%.6f %cB", float64(b)/float64(div), "kMGTPE"[exp])
	} else {
		return fmt.Sprintf("%.9f %cB", float64(b)/float64(div), "kMGTPE"[exp])
	}

}

// ExportStats export stats into a xlsx file
func ExportStats(stats *PerfStats) {
	var file *xlsx.File
	var sheet *xlsx.Sheet
	var row *xlsx.Row
	var cell *xlsx.Cell
	var err error

	file = xlsx.NewFile()
	sheet, err = file.AddSheet("BFSS - " + Cfg.Server)
	if err != nil {
		logrus.Error(err)
		return
	}
	row = sheet.AddRow()
	cell = row.AddCell()
	cell.Value = "No"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "Name"
	cell = row.AddCell()
	cell.Value = "Size"
	cell = row.AddCell()
	cell.Value = "Hash"
	cell = row.AddCell()
	cell.Value = "HashVerified"
	cell = row.AddCell()
	cell.Value = "CreateObjectTime"
	cell = row.AddCell()
	cell.Value = "WriteTime"
	cell = row.AddCell()
	cell.Value = "CompleteObjectTime"
	cell = row.AddCell()
	cell.Value = "TotalTime"

	for i, v := range stats.Files {
		row = sheet.AddRow()

		cell = row.AddCell()
		cell.SetStyle(newCenterStyle())
		cell.SetInt(i + 1)

		cell = row.AddCell()
		cell.SetString(v.Name)

		cell = row.AddCell()
		//cell.SetInt64(v.Size)
		cell.SetString(ByteCountDecimal(v.Size))

		cell = row.AddCell()
		cell.SetString(v.Hash)

		cell = row.AddCell()
		cell.SetString(v.HashVerified)

		cell = row.AddCell()
		cell.SetString(v.CreateObjectTime.String())

		cell = row.AddCell()
		cell.SetString(v.WriteTime.String())

		cell = row.AddCell()
		cell.SetString(v.CompleteObjectTime.String())

		cell = row.AddCell()
		cell.SetString((v.CreateObjectTime + v.WriteTime + v.CompleteObjectTime).String())
	}
	addSummary(stats, sheet)

	fileName := time.Now().Format("2006-01-02_15.04.05.xlsx")
	fullPath := path.Join(Cfg.DataDir, fileName)
	err = file.Save(fullPath)
	if err != nil {
		logrus.Error(err)
	}
	logrus.Info("Exported At:", fullPath)
}

func newCenterStyle() *xlsx.Style {
	s := xlsx.NewStyle()
	s.Alignment.Horizontal = "center"
	s.ApplyAlignment = true
	return s
}
func newLeftStyle() *xlsx.Style {
	s := xlsx.NewStyle()
	s.Alignment.Horizontal = "left"
	s.ApplyAlignment = true
	return s
}
func newRightStyle() *xlsx.Style {
	s := xlsx.NewStyle()
	s.Alignment.Horizontal = "right"
	s.ApplyAlignment = true
	return s
}

func addSummary(stats *PerfStats, sheet *xlsx.Sheet) {
	sheet.AddRow()

	// Head
	row := sheet.AddRow()
	cell := row.AddCell()
	cell.Value = "Summary"
	cell.SetStyle(newCenterStyle())

	row = sheet.AddRow()
	cell = row.AddCell()
	cell.Value = "TotalFiles"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "TotalCreateObjectTime"

	cell = row.AddCell()
	cell.Value = "CreateObject_Speed"

	cell = row.AddCell()
	cell.Value = "TotalWriteTime"

	cell = row.AddCell()
	cell.Value = "Write_Speed"

	cell = row.AddCell()
	cell.Value = "TotalCompleteObjectTime"

	cell = row.AddCell()
	cell.Value = "CompleteObject_Speed"

	cell = row.AddCell()
	cell.Value = "TotalTime"

	cell = row.AddCell()
	cell.Value = "TestRunTime"

	cell = row.AddCell()
	cell.Value = "TotalWrites"

	cell = row.AddCell()
	cell.Value = "ExpectedWrites"

	cell = row.AddCell()
	cell.Value = "Sample"

	// body
	row = sheet.AddRow()
	cell = row.AddCell()
	cell.SetInt(len(stats.Files))
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.SetString(stats.TotalCreateObjectTime.String())

	var speed float64
	if stats.TotalCreateObjectTime.Seconds() > 0 {
		speed = float64(len(stats.Files)) / stats.TotalCreateObjectTime.Seconds()
	}
	cell = row.AddCell()
	cell.SetString(strconv.FormatFloat(speed, 'f', 1, 32) + " 次/秒")

	cell = row.AddCell()
	cell.SetString(stats.TotalWriteTime.String())

	speed = 0
	if stats.TotalWriteTime.Seconds() > 0 {
		speed = float64(stats.TotalWrites) / stats.TotalWriteTime.Seconds()
	}
	cell = row.AddCell()
	cell.SetString(ByteCountDecimal(int64(speed)) + "/秒")

	cell = row.AddCell()
	cell.SetString(stats.TotalCompleteObjectTime.String())

	speed = 0
	if stats.TotalCompleteObjectTime.Seconds() > 0 {
		speed = float64(len(stats.Files)) / stats.TotalCompleteObjectTime.Seconds()
	}
	cell = row.AddCell()
	cell.SetString(strconv.FormatFloat(speed, 'f', 1, 32) + " 次/秒")

	cell = row.AddCell()
	cell.SetString((stats.TotalCreateObjectTime + stats.TotalWriteTime + stats.TotalCompleteObjectTime).String())

	cell = row.AddCell()
	cell.SetString(stats.TestRunTime.String())

	cell = row.AddCell()
	//cell.SetInt64(stats.TotalWrites)
	cell.SetString(ByteCountDecimal(stats.TotalWrites))

	cell = row.AddCell()
	//cell.SetInt64(stats.ExpectedWrites)
	cell.SetString(ByteCountDecimal(stats.ExpectedWrites))

	cell = row.AddCell()
	cell.SetString(stats.Sample.String())
}

// ExportChartStats exports different policy stats.
func ExportChartStats(stats []*PerfStats) {

	fileName := time.Now().Format("2006-01-02_15.04.05.xlsx")
	fileName = fmt.Sprintf("perf[%v]-%s", os.Getpid(), fileName)
	fullPath := path.Join(Cfg.DataDir, fileName)
	target, err := os.Create(fullPath)
	if err != nil {
		logrus.Error(err)
		return
	}
	err = ExportStatsToWriter(stats, target)
	if err != nil {
		logrus.Error(err)
		return
	}
	err = target.Close()
	if err != nil {
		logrus.Error(err)
		return
	}

	logrus.Info("Perf Exported At:", fullPath)
}

// ExportStatsToWriter writes stats to a writer.
func ExportStatsToWriter(stats []*PerfStats, writer io.Writer) error {
	var file *xlsx.File
	var sheet *xlsx.Sheet
	var row *xlsx.Row
	var cell *xlsx.Cell
	var err error

	file = xlsx.NewFile()
	sheet, err = file.AddSheet("BFSS - " + Cfg.Server)
	if err != nil {
		logrus.Error(err)
		return err
	}
	row = sheet.AddRow()
	cell = row.AddCell()
	cell.Value = "No"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "TotalFiles"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "TotalWrites(MB)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "CreateTime(s)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "CreateSpeed(files/s)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "WriteTime(s)"
	cell.SetStyle(newCenterStyle())
	cell = row.AddCell()
	cell.Value = "WriteSpeed(MB/s)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "CompleteObjectTime(s)"
	cell.SetStyle(newCenterStyle())
	cell = row.AddCell()
	cell.Value = "CompleteObjectSpeed(files/s)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "TotalTime"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "TestRunTime"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "Routines"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "Out(MB/s)"
	cell.SetStyle(newCenterStyle())

	cell = row.AddCell()
	cell.Value = "Sample"
	cell.SetStyle(newCenterStyle())

	for i, v := range stats {
		row = sheet.AddRow()

		cell = row.AddCell()
		cell.SetStyle(newCenterStyle())
		cell.SetInt(i + 1)

		cell = row.AddCell()
		cell.SetInt(len(v.Files))
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetFloat(float64(v.TotalWrites) / (1024 * 1024))
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetFloat(v.TotalCreateObjectTime.Seconds())
		cell.SetStyle(newCenterStyle())

		var speed float64
		if v.TotalCreateObjectTime.Seconds() > 0 {
			speed = float64(len(v.Files)) / v.TotalCreateObjectTime.Seconds()
		}
		cell = row.AddCell()
		cell.SetString(strconv.FormatFloat(speed, 'f', 1, 32))
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetFloat(v.TotalWriteTime.Seconds())
		cell.SetStyle(newCenterStyle())

		var writeSpeed float64
		if v.TotalWriteTime.Seconds() > 0 {
			writeSpeed = float64(v.TotalWrites) / v.TotalWriteTime.Seconds()
		}
		writeSpeed = writeSpeed / (1024 * 1024)
		cell = row.AddCell()
		cell.SetFloat(writeSpeed)
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetFloat(v.TotalCompleteObjectTime.Seconds())
		cell.SetStyle(newCenterStyle())

		speed = 0
		if v.TotalCompleteObjectTime.Seconds() > 0 {
			speed = float64(len(v.Files)) / v.TotalCompleteObjectTime.Seconds()
		}
		cell = row.AddCell()
		cell.SetString(strconv.FormatFloat(speed, 'f', 1, 32))
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetString((v.TotalCreateObjectTime + v.TotalWriteTime + v.TotalCompleteObjectTime).String())
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetString(v.TestRunTime.String())
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetInt(v.Routines)
		cell.SetStyle(newCenterStyle())

		out := writeSpeed
		if v.TestRunTime > 0 {
			out = float64(v.TotalWrites) / float64(v.TestRunTime.Seconds())
			out /= 1024 * 1024
		}

		cell = row.AddCell()
		cell.SetString(strconv.FormatFloat(out, 'f', 3, 64))
		cell.SetStyle(newCenterStyle())

		cell = row.AddCell()
		cell.SetString(v.Sample.String())
		cell.SetStyle(newCenterStyle())
	}

	return file.Write(writer)
}
