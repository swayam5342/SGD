// Package sgd talks to a running SyscallGuard supervisor over its control socket,
// the same way sgctl does (src/sgctl.c, src/lib/proto.h).
package sgd

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
	"time"
)

// ctlMagic is SG_CTL_MAGIC, "SGC1".
const ctlMagic uint32 = 0x53474331

// Cmd is enum sg_ctl_cmd.
type Cmd int32

const (
	CmdStatus Cmd = iota + 1
	CmdLoans
	CmdMetrics
	CmdSetPrio
	CmdSetRate
	CmdKill
)

// Priority bounds, SG_PRIO_MIN and SG_PRIO_MAX.
const (
	PrioMin = 1
	PrioMax = 10
)

// ErrSgd is an "error: ..." reply from sgd (sgctl exits 1 on these).
var ErrSgd = errors.New("sgd")

// DefaultSock is sg_default_sock: /tmp/syscallguard-<uid>.sock.
func DefaultSock() string {
	return fmt.Sprintf("/tmp/syscallguard-%d.sock", os.Getuid())
}

// Client sends one command per connection, as sgd expects.
type Client struct {
	Sock    string
	Timeout time.Duration
}

// Do sends struct sg_ctl { u32 magic; i32 cmd; u32 job; i32 reserved; i64 value } in the host's
// byte order and returns sgd's text reply. sgd answers and closes the connection.
func (c Client) Do(cmd Cmd, job uint32, value int64) (string, error) {
	timeout := c.Timeout
	if timeout == 0 {
		timeout = 2 * time.Second
	}
	conn, err := net.DialTimeout("unix", c.Sock, timeout)
	if err != nil {
		return "", fmt.Errorf("cannot reach sgd at %s: %w", c.Sock, err)
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(timeout))

	var req bytes.Buffer
	for _, v := range []any{ctlMagic, int32(cmd), job, int32(0), value} {
		_ = binary.Write(&req, binary.NativeEndian, v)
	}
	if _, err := conn.Write(req.Bytes()); err != nil {
		return "", fmt.Errorf("write to sgd: %w", err)
	}
	reply, err := io.ReadAll(conn)
	if err != nil {
		return "", fmt.Errorf("read from sgd: %w", err)
	}
	text := string(reply)
	if msg, ok := strings.CutPrefix(text, "error:"); ok {
		return "", fmt.Errorf("%w: %s", ErrSgd, strings.TrimSpace(msg))
	}
	return text, nil
}

// Status fetches and parses sgctl status.
func (c Client) Status() (Status, error) {
	text, err := c.Do(CmdStatus, 0, 0)
	if err != nil {
		return Status{}, err
	}
	return ParseStatus(text)
}

// Loans fetches and parses sgctl loans.
func (c Client) Loans() ([]Loan, error) {
	text, err := c.Do(CmdLoans, 0, 0)
	if err != nil {
		return nil, err
	}
	return ParseLoans(text)
}

// SetPrio is sgctl set-prio JOB PRIO.
func (c Client) SetPrio(job uint32, prio int) (string, error) {
	return c.Do(CmdSetPrio, job, int64(prio))
}

// SetRate is sgctl set-rate JOB RATE; 0 returns the job to a priority share.
func (c Client) SetRate(job uint32, rate int64) (string, error) {
	return c.Do(CmdSetRate, job, rate)
}

// Kill is sgctl kill JOB [SIGNUM]; signal 0 means SIGKILL.
func (c Client) Kill(job uint32, signal int) (string, error) {
	return c.Do(CmdKill, job, int64(signal))
}
