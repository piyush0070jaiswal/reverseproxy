# NS-3 Reverse Proxy Load Balancer Simulation

This directory is separate from the Java Spring Boot project. It does not modify or replace the original load balancer code.

The simulation models the same three backend servers and two algorithms used by the Java controller:

- `wrr`: weighted round robin with weights `3, 2, 1`
- `least`: least active connections

The simulated topology is:

```text
Client ---- Proxy / Load Balancer ---- SERVER-1
                                  \--- SERVER-2
                                   \-- SERVER-3
```

## Requirements

Install NS-3 separately. The source is intended to be copied into an NS-3 release's `scratch/` directory. A recent NS-3 release such as 3.39 or newer is recommended.

## Run

From the root of an NS-3 installation, copy the simulation into `scratch/`:

```bash
cp "/path/to/Reverse-Proxy-Load-Balancer-master/ns-3-simulation/scratch/reverse-proxy-load-balancer.cc" scratch/
```

The simulation creates **20 clients**, one load balancer, and three backend servers.

### Weighted Round Robin (WRR)

The weights are SERVER-1 = 3, SERVER-2 = 2, and SERVER-3 = 1:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=100 --simulationTime=6"
```

Expected output is close to:

```text
Clients: 20
Requests completed: 100
SERVER-1 handled: 50
SERVER-2 handled: 33
SERVER-3 handled: 17
```

### Least Connections (LC)

Run the least-active-connections algorithm with simultaneous requests:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=least --requests=100 --interval=0 --simulationTime=6"
```

Expected output is close to an even distribution:

```text
Clients: 20
Requests completed: 100
SERVER-1 handled: 34
SERVER-2 handled: 33
SERVER-3 handled: 33
```

The exact counts can vary slightly depending on event timing.

## View the animation with NetAnim

Each simulation creates or updates `reverse-proxy-animation.xml` in the NS-3 root directory.
Run the simulation first, then open the XML file:

```bash
netanim reverse-proxy-animation.xml
```

In NetAnim, click **Play**. The window shows Client-1 through Client-20, the load balancer, and the three backend servers. Run the WRR or LC command again before opening NetAnim to view that algorithm's latest animation.

On Arch/Omarchy, install NetAnim from the AUR:

```bash
yay -S netanim
```

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install netanim
```

The final terminal output reports completed requests and the number forwarded to each backend. NetAnim shows the packet movement; it does not replace the terminal statistics.

## Important distinction

NS-3 simulates the network traffic and load-balancing decisions. It does not start the Java Spring Boot services, and it does not alter the Java project. The original Java demo can still be run separately using its existing Maven instructions.
