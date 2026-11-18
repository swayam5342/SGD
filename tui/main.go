// sgtop: a live terminal view of a running sgd.
//
//	sgtop [-s sock] [-i interval]
//
// It polls sgd's control socket (the same commands as sgctl status and sgctl loans) and can change a job's
// priority or rate, or signal it, the way sgctl set-prio, set-rate and kill do.
package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"time"

	tea "github.com/charmbracelet/bubbletea"

	"github.com/swayam5342/SGD/tui/sgd"
)

func main() {
	sock := flag.String("s", sgd.DefaultSock(), "sgd control socket")
	interval := flag.Duration("i", time.Second, "refresh interval")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "usage: sgtop [-s sock] [-i interval]\n\n")
		flag.PrintDefaults()
	}
	flag.Parse()
	if *interval < 100*time.Millisecond {
		*interval = 100 * time.Millisecond
	}

	m := newModel(sgd.Client{Sock: *sock}, *interval)
	if _, err := tea.NewProgram(m, tea.WithAltScreen()).Run(); errors.Is(err, tea.ErrInterrupted) || errors.Is(err, tea.ErrProgramKilled) {
		os.Exit(130) // stopped by a signal, like any interrupted command
	} else if err != nil {
		fmt.Fprintln(os.Stderr, "sgtop:", err)
		os.Exit(1)
	}
}
