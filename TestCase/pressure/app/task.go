package app

import (
	"fmt"
	"os"
	"os/exec"
	"sync"
	"time"

	"github.com/sirupsen/logrus"
)

// Mode for task
type Mode int

// Mode for task
const (
	MRoutine Mode = iota
	MProcess
)

// Status for task
type Status int

// Ready when task created
const (
	Ready Status = iota
	Running
	Done
	Waiting
)

func (s Status) String() string {
	switch s {
	case Ready:
		return "Ready"
	case Running:
		return "Running"
	case Done:
		return "Done"
	case Waiting:
		return "Waiting"
	}
	return "Unknown"
}

// Task saves test task.
type Task struct {
	mutex        sync.Mutex
	Name         string     `json:"name"`
	Units        []*Process `json:"units,omitempty"`
	Cfg          Config     `json:"cfg"`
	ProcessCount int        `json:"process_count"`
	Status       Status     `json:"status"`
	Progress     int        `json:"progress"`
	Speed        float64    `json:"speed"`
	CreatedAt    int64      `json:"created_at"`
	StartedAt    int64      `json:"started_at"`
	DoneAt       int64      `json:"done_at"`
}

// Routine saves all info of a routine running in a process.
type Routine struct {
	Progress int     `json:"progress"`
	Speed    float64 `json:"speed"`
}

// Process as a running unit of task.
type Process struct {
	ID             int             `json:"id"`
	ExpectedWrites int64           `json:"expected_writes"`
	Sample         Strategy        `json:"sample"`
	ProcInfo       *exec.Cmd       `json:"-"`
	Routines       map[int]Routine `json:"routines"`
	Stats          *PerfStats      `json:"stats,omitempty"`
	Status         Status          `json:"status"`
}

var tasks []*Task
var mutex sync.Mutex
var taskChan chan *Task

func startLoop() {
	logrus.Info("Task schedule Starting...")
	taskChan = make(chan *Task, 1000)
	for {
		t := <-taskChan
		if t.Status != Waiting {
			logrus.Warnf("Invalid Task status=> %s", t.Status)
			continue
		}
		logrus.Infof("Start Task=> %s", t.Name)
		t.Start()
		t.WaitAll()
	}
}

func init() {
	go startLoop()
}

// NewTask create a new task.
func NewTask(name string, cfg *Config, p int) (*Task, error) {
	mutex.Lock()
	defer mutex.Unlock()
	for _, t := range tasks {
		if t.Name == name {
			return nil, fmt.Errorf("Duplicate Task name=> %s", name)
		}
	}

	t := &Task{
		Name:         name,
		Cfg:          *cfg,
		ProcessCount: p,
		CreatedAt:    time.Now().Unix(),
	}

	tasks = append(tasks, t)

	return t, nil
}

// AllTasks returns all tasks.
func AllTasks() []*Task {
	return tasks
}

// FindTask get the name's task.
func FindTask(name string) *Task {
	mutex.Lock()
	defer mutex.Unlock()
	for _, t := range tasks {
		if t.Name == name {
			return t
		}
	}
	return nil
}

// AddSchedule add a task into schedule queue.
func AddSchedule(t *Task) {
	t.mutex.Lock()
	t.Status = Waiting
	t.mutex.Unlock()

	taskChan <- t
}

// FindProcess gets a process by id.
func (t *Task) FindProcess(id int) *Process {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	for _, p := range t.Units {
		if p.ID == id {
			return p
		}
	}
	return nil
}

// Start start the task
func (t *Task) Start() error {
	t.mutex.Lock()
	defer t.mutex.Unlock()
	if t.Status != Ready && t.Status != Waiting {
		return fmt.Errorf("Task status error=> Status: %s", t.Status)
	}
	p := t.ProcessCount
	if p <= 0 {
		p = 1
	}

	expectedWrites := (t.Cfg.ExpectedWrites / int64(p)) + int64(1024)
	sample := t.Cfg.Sample

	for i := 0; i < p; i++ {
		pro := &Process{}
		bfssExe, err := os.Executable()
		if err != nil {
			logrus.Error(err)
			return err
		}
		cmd := exec.Command(bfssExe, "-ipc-addr", Cfg.IpcAddr, "-task", t.Name, "-ipc-client", "-fast")
		if err := cmd.Start(); err != nil {
			logrus.Errorf("Exe:%s, err:%v", bfssExe, err)
			return err
		}
		pro.ProcInfo = cmd
		pro.ID = cmd.Process.Pid
		pro.ExpectedWrites = expectedWrites
		pro.Sample = sample
		pro.Routines = make(map[int]Routine)

		t.Units = append(t.Units, pro)
	}
	t.Status = Running
	t.StartedAt = time.Now().Unix()
	return nil
}

// WaitAll wait all processes exited.
func (t *Task) WaitAll() {
	for _, u := range t.Units {
		u.ProcInfo.Wait()
	}
}

// GetStats gets task stats.
func (t *Task) GetStats() []*PerfStats {
	t.mutex.Lock()
	defer t.mutex.Unlock()
	if t.Status != Done {
		logrus.Errorf("Waiting for task done.")
		return nil
	}

	var statsList []*PerfStats

	for _, u := range t.Units {
		if u.Stats != nil {
			statsList = append(statsList, u.Stats)
		}
	}
	return statsList
}

// UpdateProgress update process progress.
func (t *Task) UpdateProgress(pid int, tag int, r Routine) {
	p := t.FindProcess(pid)

	t.mutex.Lock()
	defer t.mutex.Unlock()
	p.Routines[tag] = r
}

// UpdateStatus update tast status.
func (t *Task) UpdateStatus() {
	t.mutex.Lock()
	defer t.mutex.Unlock()

	done := true
	if len(t.Units) == 0 {
		return
	}
	pweight := float32(1) / float32(t.ProcessCount)
	rweight := float32(0)
	if t.Cfg.EnableConcurrent && t.Cfg.Routines > 0 {
		rweight = pweight / float32(t.Cfg.Routines)
	}
	var progress float32
	var speed float64
	for _, u := range t.Units {
		if u.Status != Done {
			done = false
		}
		if t.Cfg.EnableConcurrent && t.Cfg.Routines > 0 {
			for _, v := range u.Routines {
				progress += float32(v.Progress) * rweight
				speed += v.Speed
			}
		} else {
			progress += float32(u.Routines[0].Progress) * pweight
			speed += u.Routines[0].Speed
		}
	}
	t.Progress = int(progress)
	t.Speed = speed
	if done {
		t.Status = Done
		t.Progress = 100
		t.DoneAt = time.Now().Unix()
	}
}
