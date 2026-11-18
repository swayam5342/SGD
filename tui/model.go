package main

import (
	"strconv"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"

	"github.com/swayam5342/SGD/tui/sgd"
)

// historyLen is how many polls the call-rate sparkline keeps per job.
const historyLen = 120

type mode int

const (
	modeNormal  mode = iota
	modeRate         // typing a fixed rate for set-rate
	modeConfirm      // confirming a signal for kill
)

// jobHist turns sgd's running counters into per-second rates between two polls.
type jobHist struct {
	calls, waited, rejected uint64
	at                      time.Time
	callRate, waitRate      float64
	rejectRate              float64
	rates                   []float64 // calls/s, oldest first
}

type model struct {
	client   sgd.Client
	interval time.Duration

	st      sgd.Status
	loans   []sgd.Loan
	err     error
	fetched bool
	hist    map[uint32]*jobHist

	selID         uint32 // the selected job, kept by id so it survives jobs coming and going
	width, height int

	mode   mode
	input  string
	signal int // 0 = SIGKILL (sgd's default), else the signal number
	flash  string
	isErr  bool
}

type tickMsg time.Time

type snapMsg struct {
	st    sgd.Status
	loans []sgd.Loan
	err   error
	at    time.Time
}

type actMsg struct {
	text string
	err  error
}

func newModel(c sgd.Client, interval time.Duration) model {
	return model{client: c, interval: interval, hist: map[uint32]*jobHist{}, width: 100, height: 30}
}

func (m model) Init() tea.Cmd { return tea.Batch(m.fetch(), m.tick()) }

func (m model) tick() tea.Cmd {
	return tea.Tick(m.interval, func(t time.Time) tea.Msg { return tickMsg(t) })
}

func (m model) fetch() tea.Cmd {
	c := m.client
	return func() tea.Msg {
		st, err := c.Status()
		if err != nil {
			return snapMsg{err: err, at: time.Now()}
		}
		loans, err := c.Loans()
		return snapMsg{st: st, loans: loans, err: err, at: time.Now()}
	}
}

func (m model) act(f func() (string, error)) tea.Cmd {
	return func() tea.Msg {
		text, err := f()
		return actMsg{text: strings.TrimSpace(text), err: err}
	}
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width, m.height = msg.Width, msg.Height
	case tickMsg:
		return m, tea.Batch(m.fetch(), m.tick())
	case snapMsg:
		m.err = msg.err
		if msg.err == nil {
			m.fetched = true
			m.record(msg.st, msg.at)
			m.st, m.loans = msg.st, msg.loans
			m.keepSelection()
		}
	case actMsg:
		m.flash, m.isErr = msg.text, msg.err != nil
		if msg.err != nil {
			m.flash = msg.err.Error()
		}
		return m, m.fetch()
	case tea.KeyMsg:
		return m.key(msg)
	}
	return m, nil
}

// record updates each job's rates from the counters' growth since the last poll.
func (m *model) record(st sgd.Status, at time.Time) {
	seen := map[uint32]bool{}
	for _, j := range st.Jobs {
		seen[j.ID] = true
		h, ok := m.hist[j.ID]
		if !ok {
			m.hist[j.ID] = &jobHist{calls: j.Calls, waited: j.Waited, rejected: j.Rejected, at: at}
			continue
		}
		if dt := at.Sub(h.at).Seconds(); dt > 0 {
			h.callRate = rate(j.Calls, h.calls, dt)
			h.waitRate = rate(j.Waited, h.waited, dt)
			h.rejectRate = rate(j.Rejected, h.rejected, dt)
			h.rates = append(h.rates, h.callRate)
			if len(h.rates) > historyLen {
				h.rates = h.rates[len(h.rates)-historyLen:]
			}
		}
		h.calls, h.waited, h.rejected, h.at = j.Calls, j.Waited, j.Rejected, at
	}
	for id := range m.hist {
		if !seen[id] {
			delete(m.hist, id)
		}
	}
}

func rate(now, before uint64, dt float64) float64 {
	if now < before { // a new job reused the id
		return 0
	}
	return float64(now-before) / dt
}

func (m *model) keepSelection() {
	for _, j := range m.st.Jobs {
		if j.ID == m.selID {
			return
		}
	}
	m.selID = 0
	if len(m.st.Jobs) > 0 {
		m.selID = m.st.Jobs[0].ID
	}
}

func (m model) selected() (sgd.Job, int, bool) {
	for i, j := range m.st.Jobs {
		if j.ID == m.selID {
			return j, i, true
		}
	}
	return sgd.Job{}, -1, false
}

func (m model) key(k tea.KeyMsg) (tea.Model, tea.Cmd) {
	if k.String() == "ctrl+c" {
		return m, tea.Quit
	}
	switch m.mode {
	case modeRate:
		return m.rateKey(k)
	case modeConfirm:
		return m.confirmKey(k)
	}

	j, i, ok := m.selected()
	switch k.String() {
	case "q", "esc":
		return m, tea.Quit
	case "up":
		if ok && i > 0 {
			m.selID = m.st.Jobs[i-1].ID
		}
	case "down":
		if ok && i < len(m.st.Jobs)-1 {
			m.selID = m.st.Jobs[i+1].ID
		}
	case "home":
		if len(m.st.Jobs) > 0 {
			m.selID = m.st.Jobs[0].ID
		}
	case "end":
		if len(m.st.Jobs) > 0 {
			m.selID = m.st.Jobs[len(m.st.Jobs)-1].ID
		}
	case "+", "=":
		if ok && j.Prio < sgd.PrioMax {
			return m, m.act(func() (string, error) { return m.client.SetPrio(j.ID, j.Prio+1) })
		}
	case "-", "_":
		if ok && j.Prio > sgd.PrioMin {
			return m, m.act(func() (string, error) { return m.client.SetPrio(j.ID, j.Prio-1) })
		}
	case "r":
		if ok {
			m.mode, m.input, m.flash = modeRate, "", ""
		}
	case "s":
		if ok && j.Fixed {
			return m, m.act(func() (string, error) { return m.client.SetRate(j.ID, 0) })
		}
	case "k":
		if ok {
			m.mode, m.signal, m.flash = modeConfirm, 0, ""
		}
	case "t":
		if ok {
			m.mode, m.signal, m.flash = modeConfirm, 15, ""
		}
	}
	return m, nil
}

func (m model) rateKey(k tea.KeyMsg) (tea.Model, tea.Cmd) {
	switch k.Type {
	case tea.KeyEsc:
		m.mode = modeNormal
	case tea.KeyBackspace:
		if m.input != "" {
			m.input = m.input[:len(m.input)-1]
		}
	case tea.KeyEnter:
		m.mode = modeNormal
		j, _, ok := m.selected()
		v, err := strconv.ParseInt(m.input, 10, 64)
		if !ok || err != nil {
			return m, nil
		}
		return m, m.act(func() (string, error) { return m.client.SetRate(j.ID, v) })
	case tea.KeyRunes:
		for _, r := range k.Runes {
			if r >= '0' && r <= '9' && len(m.input) < 12 {
				m.input += string(r)
			}
		}
	}
	return m, nil
}

func (m model) confirmKey(k tea.KeyMsg) (tea.Model, tea.Cmd) {
	m.mode = modeNormal
	j, _, ok := m.selected()
	if !ok || (k.String() != "y" && k.String() != "Y") {
		return m, nil
	}
	sig := m.signal
	return m, m.act(func() (string, error) { return m.client.Kill(j.ID, sig) })
}
