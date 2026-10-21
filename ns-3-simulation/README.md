# NS-3 Reverse Proxy Load Balancer Simulation

This directory contains the NS-3 model for the existing three-backend reverse proxy. It preserves both supported strategies:

- `wrr`: weighted round robin with weights `3, 2, 1`
- `least`: least active connections

The topology has 20 clients, one load balancer, and three backends. Each point-to-point link can use configurable bandwidth and propagation delay. Request/response packets are echoed through the selected backend and visualized in NetAnim.

## Requirements and setup

Install NS-3 separately (3.39 or newer recommended). Open a terminal and change to the NS-3 installation root—the directory containing the `ns3` command and `scratch/` folder. Replace `/path/to/ns-3` and `/path/to/reverseproxy` with the locations on your computer:

```bash
cd /path/to/ns-3
cp "/path/to/reverseproxy/ns-3-simulation/scratch/reverse-proxy-load-balancer.cc" scratch/
```

Run every `./ns3 run ...` command below from that same NS-3 directory, not from the repository directory.

## Standard simulation

The default remains WRR with a 10 Mbps link rate and 2 ms channel delay:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=100 --simulationTime=6"
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=least --requests=100 --interval=0 --simulationTime=6"
```

## Network congestion and delay

Set `dataRate` and `delay` to vary link capacity and propagation delay. `packetSize` controls the echoed application payload, and `requests`/`interval` control offered load:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=1000 --interval=0.001 --packetSize=1200 --dataRate=1Mbps --delay=30ms --simulationTime=8"
```

These are applied to the point-to-point links in the topology. The terminal reports actual application response throughput, round-trip delay, completed requests, packet delivery ratio, and IP-level packet counts/loss from FlowMonitor.

## Backend link failure and recovery

The example below drops packets in both directions on the Server-2 link from 2 s until 4 s, then restores the link:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=400 --interval=0.02 --simulateFailure=1 --failureAt=2 --recoveryAt=4 --simulationTime=7"
```

Options:

- `failureBackend=1|2|3` selects the backend (default: `2`).
- `failureAt` and `recoveryAt` specify event times in seconds.
- Failure and restoration are printed in the terminal and shown by changing the selected server's NetAnim color. Lost requests are not retried; recovery time is measured from restoration to the first successfully returned response observed after restoration. It is reported as unavailable if no response arrives before the simulation ends.

## Compare load-balancing strategies

Run both existing strategies with the same workload and settings, and print their measured throughput, average request/response RTT, and delivery ratio:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --compare=1 --requests=400 --interval=0.02 --packetSize=1200 --dataRate=2Mbps --delay=10ms --simulationTime=7"
```

Comparison mode writes separate animation files:

- `reverse-proxy-animation-wrr.xml`
- `reverse-proxy-animation-least.xml`

## View topology and packet movement in NetAnim

A normal single-strategy run writes `reverse-proxy-animation.xml` (or the path supplied with `animationFile`). Open it after the run:

```bash
netanim reverse-proxy-animation.xml
```

NetAnim displays all clients, the load balancer, the backends, and packet movement. In a failure run, the selected backend changes color at the scheduled failure and restoration times. For comparison runs, open either strategy-specific XML file listed above.

Install NetAnim on Arch/Omarchy:

```bash
yay -S netanim
```

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install netanim
```

## Metric definitions

- **Application throughput:** successfully returned response payload bits divided by the client measurement interval.
- **Average delay:** mean client-observed request-to-response round-trip time for completed requests.
- **Delivery ratio:** completed request/response transactions divided by requests actually sent.
- **Request/response loss:** sent transactions that did not complete before simulation end.
- **IP packet loss:** FlowMonitor's lost IP packets across the simulated flows, reported alongside transmitted/received counts.
- **Recovery time:** time from backend-link restoration until the first successful response received afterward.

The Java Spring Boot application is separate; this simulation does not start or modify it.
