package main

import (
	"encoding/binary"
	"io"
	"net"
	"path/filepath"
	"testing"
	"time"

	tea "github.com/charmbracelet/bubbletea"

	"github.com/swayam5342/SGD/tui/sgd"
)

type request struct {
	cmd   sgd.Cmd
	job   uint32
	value int64
}

// recorder is a stand-in sgd that records every control request and answers "ok".
func recorder(t *testing.T) (sgd.Client, chan request) {
	t.Helper()
	sock := filepath.Join(t.TempDir(), "sgd.sock")
	ln, err := net.Listen("unix", sock)
	if err != nil {
		t.Skipf("unix sockets unavailable: %v", err)
	}
	t.Cleanup(func() { ln.Close() })
	got := make(chan request, 8)
	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				return
			}
			buf := make([]byte, 24)
			if _, err := io.ReadFull(conn, buf); err == nil {
				e := binary.NativeEndian
				got <- request{sgd.Cmd(int32(e.Uint32(buf[4:]))), e.Uint32(buf[8:]), int64(e.Uint64(buf[16:]))}
			}
			_, _ = conn.Write([]byte("ok\n"))
			conn.Close()
		}
	}()
	return sgd.Client{Sock: sock, Timeout: time.Second}, got
}

// loaded returns a model that has seen one status with two jobs, job 1 selected.
func loaded(c sgd.Client) model {
	m := newModel(c, time.Second)
	st := sgd.Status{Jobs: []sgd.Job{
		{ID: 1, Name: "build", Prio: 8, Rate: 2400, Capacity: 2400, Fork: "brief"},
		{ID: 2, Name: "logger", Prio: 2, Rate: 600, Capacity: 600, Fixed: true, Fork: "brief"},
	}}
	next, _ := m.Update(snapMsg{st: st, at: time.Now()})
	return next.(model)
}

func press(m model, keys ...string) (model, tea.Cmd) {
	var cmd tea.Cmd
	for _, k := range keys {
		var msg tea.KeyMsg
		switch k {
		case "enter":
			msg = tea.KeyMsg{Type: tea.KeyEnter}
		case "down":
			msg = tea.KeyMsg{Type: tea.KeyDown}
		default:
			msg = tea.KeyMsg{Type: tea.KeyRunes, Runes: []rune(k)}
		}
		var next tea.Model
		next, cmd = m.Update(msg)
		m = next.(model)
	}
	return m, cmd
}

// run executes a command the model returned and waits for the stand-in to see the request.
func run(t *testing.T, cmd tea.Cmd, got chan request) request {
	t.Helper()
	if cmd == nil {
		t.Fatal("no command was returned")
	}
	if msg, ok := cmd().(actMsg); !ok || msg.err != nil {
		t.Fatalf("action result = %#v", msg)
	}
	select {
	case r := <-got:
		return r
	case <-time.After(2 * time.Second):
		t.Fatal("sgd saw no request")
		return request{}
	}
}

func TestPriorityKeys(t *testing.T) {
	c, got := recorder(t)
	m := loaded(c)
	if r := run(t, second(press(m, "+")), got); r != (request{sgd.CmdSetPrio, 1, 9}) {
		t.Errorf("+ sent %+v", r)
	}
	if r := run(t, second(press(m, "-")), got); r != (request{sgd.CmdSetPrio, 1, 7}) {
		t.Errorf("- sent %+v", r)
	}
}

func TestSelectionAndShare(t *testing.T) {
	c, got := recorder(t)
	m, _ := press(loaded(c), "down")
	if m.selID != 2 {
		t.Fatalf("selected job %d after down, want 2", m.selID)
	}
	if r := run(t, second(press(m, "s")), got); r != (request{sgd.CmdSetRate, 2, 0}) {
		t.Errorf("s sent %+v", r)
	}
}

func TestRateInput(t *testing.T) {
	c, got := recorder(t)
	m, _ := press(loaded(c), "r", "5", "x", "0", "0")
	if m.input != "500" {
		t.Fatalf("input = %q, want digits only", m.input)
	}
	if r := run(t, second(press(m, "enter")), got); r != (request{sgd.CmdSetRate, 1, 500}) {
		t.Errorf("rate sent %+v", r)
	}
}

func TestKillNeedsConfirmation(t *testing.T) {
	c, got := recorder(t)
	m := loaded(c)
	if _, cmd := press(m, "k", "n"); cmd != nil {
		t.Fatal("kill without y sent a command")
	}
	if r := run(t, second(press(m, "k", "y")), got); r != (request{sgd.CmdKill, 1, 0}) {
		t.Errorf("k y sent %+v", r)
	}
	if r := run(t, second(press(m, "t", "y")), got); r != (request{sgd.CmdKill, 1, 15}) {
		t.Errorf("t y sent %+v", r)
	}
}

func TestRates(t *testing.T) {
	m := newModel(sgd.Client{}, time.Second)
	t0 := time.Now()
	m.record(sgd.Status{Jobs: []sgd.Job{{ID: 1, Calls: 100, Waited: 10}}}, t0)
	m.record(sgd.Status{Jobs: []sgd.Job{{ID: 1, Calls: 300, Waited: 30}}}, t0.Add(2*time.Second))
	h := m.hist[1]
	if h.callRate != 100 || h.waitRate != 10 || len(h.rates) != 1 {
		t.Errorf("rates = %+v", h)
	}
	m.record(sgd.Status{}, t0.Add(3*time.Second))
	if _, ok := m.hist[1]; ok {
		t.Error("history kept for a job that ended")
	}
}

func second(_ model, cmd tea.Cmd) tea.Cmd { return cmd }
