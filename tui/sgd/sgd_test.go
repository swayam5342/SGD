package sgd

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
	"path/filepath"
	"testing"
)

// Built with the printf formats of src/sgd/ctl.c write_status; the rows are the README's example.
const statusText = `global: 1234.5 / 3000.0 units, refill 3000 units/s, borrowing on, up 12.3 s
totals: calls=5395 allowed=5393 waited=83 rejected=0 dropped=0 charged=8315
loans:  opened=1 repaid=1 written_off=0

JOB  NAME           PRIO  MODE   RATE/S     TOKENS   CAPACITY WAIT    CALLS  ALLOWED  WAITED REJECTED     OWED     LENT   FORK
1    interactive       8 share     3200        0.4     3200.0    1     4467     4466      38        0      0.0      0.0  brief
2    logger            2 share      800        0.8      800.0    1      928      927      45        0      0.0      0.0  brief
3    my batch job      5 fixed      500      250.0      500.0    0       10       10       0        0     12.5     40.0 reject
`

func TestParseStatus(t *testing.T) {
	st, err := ParseStatus(statusText)
	if err != nil {
		t.Fatal(err)
	}
	g := st.Global
	if g.Tokens != 1234.5 || g.Capacity != 3000 || g.Refill != 3000 || !g.Borrowing || g.UpSeconds != 12.3 {
		t.Errorf("global = %+v", g)
	}
	if st.Totals.Calls != 5395 || st.Totals.Waited != 83 || st.Totals.Charged != 8315 {
		t.Errorf("totals = %+v", st.Totals)
	}
	if st.Ledger.Opened != 1 || st.Ledger.Repaid != 1 {
		t.Errorf("ledger = %+v", st.Ledger)
	}
	if len(st.Jobs) != 3 {
		t.Fatalf("got %d jobs, want 3", len(st.Jobs))
	}
	want := Job{ID: 2, Name: "logger", Prio: 2, Rate: 800, Tokens: 0.8, Capacity: 800, Waiting: 1,
		Calls: 928, Allowed: 927, Waited: 45, Fork: "brief"}
	if st.Jobs[1] != want {
		t.Errorf("job 2 = %+v\nwant  %+v", st.Jobs[1], want)
	}
	b := st.Jobs[2]
	if b.Name != "my batch job" || !b.Fixed || b.Owed != 12.5 || b.Lent != 40 || b.Fork != "reject" {
		t.Errorf("job 3 = %+v", b)
	}
}

func TestParseStatusNoJobs(t *testing.T) {
	st, err := ParseStatus("global: 0.0 / 200000.0 units, refill 200000 units/s, borrowing off, up 0.5 s\n" +
		"totals: calls=0 allowed=0 waited=0 rejected=0 dropped=0 charged=0\n" +
		"loans:  opened=0 repaid=0 written_off=0\n\nJOB  NAME PRIO\n(no jobs)\n")
	if err != nil {
		t.Fatal(err)
	}
	if len(st.Jobs) != 0 || st.Global.Borrowing {
		t.Errorf("status = %+v", st)
	}
}

func TestParseLoans(t *testing.T) {
	loans, err := ParseLoans("LOAN   FROM    TO     AMOUNT     REPAID    AGE_S\n" +
		"1         1     2      265.0        0.0     0.53\n")
	if err != nil {
		t.Fatal(err)
	}
	want := Loan{ID: 1, From: 1, To: 2, Amount: 265, AgeSeconds: 0.53}
	if len(loans) != 1 || loans[0] != want {
		t.Errorf("loans = %+v", loans)
	}
	none, err := ParseLoans("LOAN   FROM    TO     AMOUNT     REPAID    AGE_S\n(no outstanding loans)\n")
	if err != nil || len(none) != 0 {
		t.Errorf("empty loans = %+v, %v", none, err)
	}
}

// fakeSgd accepts one connection, checks the request, and answers with reply.
func fakeSgd(t *testing.T, reply string, check func(magic uint32, cmd int32, job uint32, value int64)) string {
	t.Helper()
	sock := filepath.Join(t.TempDir(), "sgd.sock")
	ln, err := net.Listen("unix", sock)
	if err != nil {
		t.Skipf("unix sockets unavailable: %v", err)
	}
	t.Cleanup(func() { ln.Close() })
	go func() {
		conn, err := ln.Accept()
		if err != nil {
			return
		}
		defer conn.Close()
		buf := make([]byte, 24)
		if _, err := io.ReadFull(conn, buf); err != nil {
			t.Errorf("read request: %v", err)
			return
		}
		e := binary.NativeEndian
		check(e.Uint32(buf[0:]), int32(e.Uint32(buf[4:])), e.Uint32(buf[8:]), int64(e.Uint64(buf[16:])))
		_, _ = conn.Write([]byte(reply))
	}()
	return sock
}

func TestClientRequestLayout(t *testing.T) {
	sock := fakeSgd(t, "ok: job 2 (logger) priority 9, rate 900 units/s\n", func(magic uint32, cmd int32, job uint32, value int64) {
		if magic != ctlMagic || Cmd(cmd) != CmdSetPrio || job != 2 || value != 9 {
			t.Errorf("request = %#x %d %d %d", magic, cmd, job, value)
		}
	})
	out, err := Client{Sock: sock}.SetPrio(2, 9)
	if err != nil || out == "" {
		t.Fatalf("SetPrio = %q, %v", out, err)
	}
}

func TestClientError(t *testing.T) {
	sock := fakeSgd(t, "error: no job 7\n", func(uint32, int32, uint32, int64) {})
	_, err := Client{Sock: sock}.Kill(7, 0)
	if !errors.Is(err, ErrSgd) || err.Error() != "sgd: no job 7" {
		t.Fatalf("Kill error = %v", err)
	}
}

func TestClientUnreachable(t *testing.T) {
	_, err := Client{Sock: filepath.Join(t.TempDir(), "missing.sock")}.Status()
	if err == nil {
		t.Fatal("expected an error for a missing socket")
	}
}
