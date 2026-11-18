package main

import (
	"fmt"
	"math"
	"strings"

	"github.com/charmbracelet/lipgloss"

	"github.com/swayam5342/SGD/tui/sgd"
)

// The project website's palette: near-black ground, bone ink, muted green accent.
var (
	cAccent = lipgloss.Color("#9fc7b8")
	cInk    = lipgloss.Color("#e3e2da")
	cMuted  = lipgloss.Color("#8e948d")
	cWarn   = lipgloss.Color("#e0b97a")
	cBad    = lipgloss.Color("#e08a7a")
	cDark   = lipgloss.Color("#161c1a")

	sTitle  = lipgloss.NewStyle().Foreground(cAccent).Bold(true)
	sInk    = lipgloss.NewStyle().Foreground(cInk)
	sMuted  = lipgloss.NewStyle().Foreground(cMuted)
	sAccent = lipgloss.NewStyle().Foreground(cAccent)
	sWarn   = lipgloss.NewStyle().Foreground(cWarn)
	sBad    = lipgloss.NewStyle().Foreground(cBad)
	sSel    = lipgloss.NewStyle().Foreground(cDark).Background(cAccent)
	sBox    = lipgloss.NewStyle().Border(lipgloss.RoundedBorder()).BorderForeground(cMuted).Padding(0, 1)
	sHead   = lipgloss.NewStyle().Foreground(cMuted).Bold(true)
)

func (m model) View() string {
	w := max(m.width, 60)
	inner := w - 4 // a box's border and padding

	var b strings.Builder
	b.WriteString(m.header(w))
	b.WriteString("\n")

	if m.err != nil {
		b.WriteString(sBad.Render(trunc("  "+m.err.Error()+" (retrying)", w)))
		b.WriteString("\n")
	}
	if !m.fetched {
		b.WriteString(sMuted.Render("\n  waiting for sgd...\n"))
		b.WriteString(m.footer(w))
		return b.String()
	}

	b.WriteString(sBox.Width(w - 2).Render(m.globalView(inner)))
	b.WriteString("\n")
	b.WriteString(sBox.Width(w - 2).Render(m.jobsView(inner)))
	b.WriteString("\n")
	if j, _, ok := m.selected(); ok {
		b.WriteString(sBox.Width(w - 2).Render(m.detailView(j, inner)))
		b.WriteString("\n")
	}
	b.WriteString(sBox.Width(w - 2).Render(m.loansView(inner)))
	b.WriteString("\n")
	b.WriteString(m.footer(w))
	return b.String()
}

func (m model) header(w int) string {
	left := sTitle.Render("sgtop")
	right := sMuted.Render(fmt.Sprintf("sgd at %s   up %.1f s   every %s", m.client.Sock, m.st.Global.UpSeconds, m.interval))
	gap := max(w-lipgloss.Width(left)-lipgloss.Width(right), 1)
	return trunc(left+strings.Repeat(" ", gap)+right, w)
}

func (m model) globalView(w int) string {
	g, t := m.st.Global, m.st.Totals
	borrowing := sMuted.Render("borrowing off")
	if g.Borrowing {
		borrowing = sAccent.Render("borrowing on")
	}
	label := fmt.Sprintf("%.1f / %.1f units   refill %d units/s   ", g.Tokens, g.Capacity, g.Refill)
	barW := max(w-len("global ")-len(label)-len("borrowing off")-2, 10)
	line1 := sInk.Render("global ") + bar(g.Tokens, g.Capacity, barW) + "  " + sInk.Render(label) + borrowing
	line2 := sMuted.Render(fmt.Sprintf("calls %d   allowed %d   waited %d   rejected %d   dropped %d   charged %.0f   loans opened %d, repaid %d, written off %d",
		t.Calls, t.Allowed, t.Waited, t.Rejected, t.Dropped, t.Charged, m.st.Ledger.Opened, m.st.Ledger.Repaid, m.st.Ledger.WrittenOff))
	return trunc(line1, w) + "\n" + trunc(line2, w)
}

// BUDGET is the job's tokens left over its bucket's capacity, in cost units.
const rowFmt = "%-4s %-14s %4s %-5s %7s %14s %8s %5s %7s %7s %7s %7s %-6s %-9s"

// budget is "tokens/capacity" in cost units, one decimal for what is left, as sgctl status prints it.
func budget(tokens, capacity float64) string {
	return fmt.Sprintf("%.1f/%.0f", tokens, capacity)
}

func (m model) jobsView(w int) string {
	var b strings.Builder
	b.WriteString(sHead.Render(trunc(fmt.Sprintf(rowFmt, "JOB", "NAME", "PRIO", "MODE", "RATE/S", "BUDGET",
		"CALLS/S", "HELD", "WAITED", "REJECT", "OWED", "LENT", "FORK", "STATE"), w)))
	if len(m.st.Jobs) == 0 {
		b.WriteString("\n" + sMuted.Render("no jobs: start one with sg-launch"))
		return b.String()
	}
	for _, j := range m.st.Jobs {
		h := m.hist[j.ID]
		state, style := jobState(j, h)
		mode := "share"
		if j.Fixed {
			mode = "fixed"
		}
		callRate := "-"
		if h != nil && len(h.rates) > 0 {
			callRate = fmt.Sprintf("%.0f", h.callRate)
		}
		row := fmt.Sprintf(rowFmt, fmt.Sprint(j.ID), trunc(j.Name, 14), fmt.Sprint(j.Prio), mode, fmt.Sprint(j.Rate),
			budget(j.Tokens, j.Capacity), callRate, fmt.Sprint(j.Waiting), fmt.Sprint(j.Waited),
			fmt.Sprint(j.Rejected), fmt.Sprintf("%.1f", j.Owed), fmt.Sprintf("%.1f", j.Lent), j.Fork, state)
		row = trunc(row, w)
		b.WriteString("\n")
		if j.ID == m.selID {
			b.WriteString(sSel.Render(row + strings.Repeat(" ", max(w-lipgloss.Width(row), 0))))
		} else {
			b.WriteString(style.Render(row))
		}
	}
	return b.String()
}

// jobState names what a job is doing from its own fields: held calls, rejections since the last poll, debt, loans.
func jobState(j sgd.Job, h *jobHist) (string, lipgloss.Style) {
	switch {
	case h != nil && h.rejectRate > 0:
		return "rejecting", sBad
	case j.Waiting > 0:
		return "waiting", sWarn
	case j.Owed > 0:
		return "borrowing", sWarn
	case j.Lent > 0:
		return "lending", sAccent
	default:
		return "running", sInk
	}
}

func (m model) detailView(j sgd.Job, w int) string {
	h := m.hist[j.ID]
	mode := "a priority share"
	if j.Fixed {
		mode = "a fixed rate"
	}
	title := sTitle.Render(fmt.Sprintf("job %d  %s", j.ID, j.Name)) + sMuted.Render(fmt.Sprintf("   priority %d, %s, fork policy %s", j.Prio, mode, j.Fork))
	lines := []string{trunc(title, w)}

	if h != nil && len(h.rates) > 0 {
		peak := 0.0
		for _, r := range h.rates {
			peak = math.Max(peak, r)
		}
		lines = append(lines,
			sAccent.Render(sparkline(h.rates, w-16))+sMuted.Render(fmt.Sprintf("  peak %.0f/s", peak)),
			sInk.Render(fmt.Sprintf("now %.1f calls/s, %.1f waits/s, %.1f rejects/s", h.callRate, h.waitRate, h.rejectRate)))
	} else {
		lines = append(lines, sMuted.Render("call rate appears after the next poll"))
	}
	lines = append(lines,
		sInk.Render(fmt.Sprintf("budget %.1f / %.1f units, refill %d units/s   held now %d", j.Tokens, j.Capacity, j.Rate, j.Waiting)),
		sInk.Render(fmt.Sprintf("calls %d   allowed %d   waited %d   rejected %d   owes %.1f   lent %.1f",
			j.Calls, j.Allowed, j.Waited, j.Rejected, j.Owed, j.Lent)))
	for i := range lines {
		lines[i] = trunc(lines[i], w)
	}
	return strings.Join(lines, "\n")
}

func (m model) loansView(w int) string {
	if len(m.loans) == 0 {
		return sMuted.Render("no outstanding loans")
	}
	lines := []string{sHead.Render(fmt.Sprintf("%d outstanding loan(s)", len(m.loans)))}
	for _, l := range m.loans {
		lines = append(lines, trunc(sInk.Render(fmt.Sprintf("loan %d   job %d -> job %d   %.1f units, repaid %.1f   %.2f s old",
			l.ID, l.From, l.To, l.Amount, l.Repaid, l.AgeSeconds)), w))
	}
	return strings.Join(lines, "\n")
}

func (m model) footer(w int) string {
	j, _, ok := m.selected()
	switch {
	case m.mode == modeRate && ok:
		return sWarn.Render(trunc(fmt.Sprintf("fixed rate for job %d (%s), units/s: %s_   enter set   esc cancel", j.ID, j.Name, m.input), w))
	case m.mode == modeConfirm && ok:
		sig := "SIGKILL"
		if m.signal == 15 {
			sig = "SIGTERM"
		}
		return sBad.Render(trunc(fmt.Sprintf("send %s to job %d (%s), its whole process group? y / n", sig, j.ID, j.Name), w))
	}
	keys := sMuted.Render("up/down select   +/- priority   r fixed rate   s share   k kill   t term   q quit")
	if m.flash == "" {
		return trunc(keys, w)
	}
	msg := sAccent.Render(m.flash)
	if m.isErr {
		msg = sBad.Render(m.flash)
	}
	return trunc(msg, w) + "\n" + trunc(keys, w)
}

// bar draws used budget in the accent and the rest muted.
func bar(v, total float64, w int) string {
	full := filled(v, total, w)
	return sAccent.Render(strings.Repeat("█", full)) + sMuted.Render(strings.Repeat("░", w-full))
}

func filled(v, total float64, w int) int {
	if total <= 0 || w <= 0 {
		return 0
	}
	return int(math.Round(math.Min(math.Max(v/total, 0), 1) * float64(w)))
}

var sparks = []rune("▁▂▃▄▅▆▇█")

// sparkline draws the newest w values, scaled to their peak.
func sparkline(vals []float64, w int) string {
	if w <= 0 {
		return ""
	}
	if len(vals) > w {
		vals = vals[len(vals)-w:]
	}
	peak := 0.0
	for _, v := range vals {
		peak = math.Max(peak, v)
	}
	var b strings.Builder
	for _, v := range vals {
		i := 0
		if peak > 0 {
			i = int(math.Round(v / peak * float64(len(sparks)-1)))
		}
		b.WriteRune(sparks[i])
	}
	return b.String()
}

// trunc cuts a string, styled or not, to w terminal cells.
func trunc(s string, w int) string {
	if lipgloss.Width(s) <= w {
		return s
	}
	return lipgloss.NewStyle().MaxWidth(w).Render(s)
}
