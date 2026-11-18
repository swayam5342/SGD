package sgd

import (
	"bufio"
	"fmt"
	"strconv"
	"strings"
)

// Global is the first line of sgctl status:
// "global: %.1f / %.1f units, refill %lld units/s, borrowing %s, up %.1f s"
type Global struct {
	Tokens, Capacity float64
	Refill           int64
	Borrowing        bool
	UpSeconds        float64
}

// Totals is "totals: calls=.. allowed=.. waited=.. rejected=.. dropped=.. charged=.."
type Totals struct {
	Calls, Allowed, Waited, Rejected, Dropped uint64
	Charged                                   float64
}

// Ledger is "loans:  opened=.. repaid=.. written_off=.."
type Ledger struct {
	Opened, Repaid, WrittenOff uint64
}

// Job is one row of the status table (src/sgd/ctl.c write_status).
type Job struct {
	ID       uint32
	Name     string
	Prio     int
	Fixed    bool // MODE "fixed" (sg-launch -r / sgctl set-rate), else "share"
	Rate     int64
	Tokens   float64
	Capacity float64
	Waiting  uint32 // WAIT: calls held right now
	Calls    uint64
	Allowed  uint64
	Waited   uint64
	Rejected uint64
	Owed     float64
	Lent     float64
	Fork     string // brief, wait or reject
}

// Status is the whole sgctl status reply.
type Status struct {
	Global Global
	Totals Totals
	Ledger Ledger
	Jobs   []Job
}

// Loan is one row of sgctl loans: "%-5u %5u %5u %10.1f %10.1f %8.2f".
type Loan struct {
	ID, From, To   uint32
	Amount, Repaid float64
	AgeSeconds     float64
}

// jobCols is the number of columns after NAME in a status row.
const jobCols = 13

// ParseStatus reads the text sgd writes for SG_CTL_STATUS.
func ParseStatus(text string) (Status, error) {
	var st Status
	sc := bufio.NewScanner(strings.NewReader(text))
	inTable := false
	for sc.Scan() {
		line := strings.TrimRight(sc.Text(), " \r")
		switch {
		case line == "" || line == "(no jobs)":
			continue
		case strings.HasPrefix(line, "global:"):
			g, err := parseGlobal(line)
			if err != nil {
				return st, err
			}
			st.Global = g
		case strings.HasPrefix(line, "totals:"):
			kv := keyValues(strings.TrimPrefix(line, "totals:"))
			st.Totals = Totals{
				Calls: u64(kv["calls"]), Allowed: u64(kv["allowed"]), Waited: u64(kv["waited"]),
				Rejected: u64(kv["rejected"]), Dropped: u64(kv["dropped"]), Charged: f64(kv["charged"]),
			}
		case strings.HasPrefix(line, "loans:"):
			kv := keyValues(strings.TrimPrefix(line, "loans:"))
			st.Ledger = Ledger{Opened: u64(kv["opened"]), Repaid: u64(kv["repaid"]), WrittenOff: u64(kv["written_off"])}
		case strings.HasPrefix(line, "JOB "):
			inTable = true
		case inTable:
			j, err := parseJob(line)
			if err != nil {
				return st, err
			}
			st.Jobs = append(st.Jobs, j)
		}
	}
	return st, sc.Err()
}

func parseGlobal(line string) (Global, error) {
	var g Global
	var borrowing string
	_, err := fmt.Sscanf(line, "global: %f / %f units, refill %d units/s, borrowing %s up %f s",
		&g.Tokens, &g.Capacity, &g.Refill, &borrowing, &g.UpSeconds)
	if err != nil {
		return g, fmt.Errorf("parse %q: %w", line, err)
	}
	g.Borrowing = strings.TrimSuffix(borrowing, ",") == "on"
	return g, nil
}

// parseJob splits a row on spaces. NAME is printed with %-14.14s and may itself hold spaces,
// so the name is whatever lies between the job id and the last 13 columns.
func parseJob(line string) (Job, error) {
	f := strings.Fields(line)
	if len(f) < 1+1+jobCols {
		return Job{}, fmt.Errorf("short status row %q", line)
	}
	id, err := strconv.ParseUint(f[0], 10, 32)
	if err != nil {
		return Job{}, fmt.Errorf("job id in %q: %w", line, err)
	}
	c := f[len(f)-jobCols:]
	j := Job{
		ID:       uint32(id),
		Name:     strings.Join(f[1:len(f)-jobCols], " "),
		Prio:     int(i64(c[0])),
		Fixed:    c[1] == "fixed",
		Rate:     i64(c[2]),
		Tokens:   f64(c[3]),
		Capacity: f64(c[4]),
		Waiting:  uint32(u64(c[5])),
		Calls:    u64(c[6]),
		Allowed:  u64(c[7]),
		Waited:   u64(c[8]),
		Rejected: u64(c[9]),
		Owed:     f64(c[10]),
		Lent:     f64(c[11]),
		Fork:     c[12],
	}
	return j, nil
}

// ParseLoans reads the text sgd writes for SG_CTL_LOANS.
func ParseLoans(text string) ([]Loan, error) {
	var loans []Loan
	sc := bufio.NewScanner(strings.NewReader(text))
	for sc.Scan() {
		f := strings.Fields(sc.Text())
		if len(f) == 0 || f[0] == "LOAN" || strings.HasPrefix(f[0], "(") {
			continue
		}
		if len(f) != 6 {
			return loans, fmt.Errorf("loan row %q", sc.Text())
		}
		loans = append(loans, Loan{
			ID: uint32(u64(f[0])), From: uint32(u64(f[1])), To: uint32(u64(f[2])),
			Amount: f64(f[3]), Repaid: f64(f[4]), AgeSeconds: f64(f[5]),
		})
	}
	return loans, sc.Err()
}

func keyValues(s string) map[string]string {
	kv := map[string]string{}
	for _, field := range strings.Fields(s) {
		if k, v, ok := strings.Cut(field, "="); ok {
			kv[k] = v
		}
	}
	return kv
}

func u64(s string) uint64  { v, _ := strconv.ParseUint(s, 10, 64); return v }
func i64(s string) int64   { v, _ := strconv.ParseInt(s, 10, 64); return v }
func f64(s string) float64 { v, _ := strconv.ParseFloat(s, 64); return v }
