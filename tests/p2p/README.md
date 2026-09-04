# P2P transport host tests

```sh
./tests/p2p/run.sh
```

Builds and runs natively — no board, no cross toolchain.

## What is covered

`test_ikcp_cong.c` exercises the CUBIC congestion control that KCP uses
(`src/tuya_p2p/base_ice/src/ikcp_cong.c`) against the real `ikcpcb`, driving it
through the situations a live stream actually meets:

- how fast a fresh flow reaches a usable window (ramp)
- the multiplicative decrease on fast retransmit, and recovery afterwards
- the harsher restart on RTO, and recovery from that
- that the peer's advertised window is never exceeded on the wire
- that a flow losing continuously still keeps a minimum window

## Why it exists

Congestion control fails quietly. A window that stops growing, or grows far
slower than intended, still streams perfectly on a desk and only shows up as
stalling video on a link with real loss - by which time it is being blamed on
the network. Two defects were found this way and would not have been found on
hardware:

- the TCP-friendliness estimator and the cubic increase shared one ACK counter,
  so the first consumed the ACKs the second was waiting on and congestion
  avoidance barely grew at all: after a fast retransmit the window sat exactly
  where the loss left it (717) instead of climbing back (819 once separated)
- the TCP-friendliness step discarded the remainder of each increment instead
  of carrying it, which quietly made the flow *less* aggressive than the plain
  Reno it is supposed to keep pace with

Both are invisible to any test that only asks "did video arrive".

## Notes

`stubs.c` stands in for the mbuf and pacing helpers `ikcp.c` links against.
The congestion control under test touches none of those paths; keeping them out
avoids dragging the whole transport into a unit test.

The test drives `kcp->current` forward itself. The cubic curve is a function of
time since the last loss, so a test that holds the clock still measures a
window that cannot move - an earlier revision did exactly that and reported a
failure that was purely its own doing.
