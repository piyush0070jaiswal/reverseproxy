# Reverse Proxy Load Balancer

A Spring Boot reverse proxy with Weighted Round Robin (WRR) and Least Connections (LC), plus an NS-3 simulation of 20 clients, one load balancer, and three backends.

## NS-3 simulation

The NS-3 scenario supports configurable bandwidth and delay, backend link failure/recovery, terminal performance metrics, NetAnim output, and measured WRR-versus-least-connections comparison.

### Quick start from a terminal

Install NS-3 separately. In the commands below, replace `/path/to/ns-3` with your NS-3 installation directory and `/path/to/reverseproxy` with this repository's directory. The NS-3 directory is the one containing the `ns3` command and `scratch/` folder.

```bash
cd /path/to/ns-3
cp "/path/to/reverseproxy/ns-3-simulation/scratch/reverse-proxy-load-balancer.cc" scratch/
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=100 --simulationTime=6"
```

To run least-connections instead, use this command from the same NS-3 directory:

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=least --requests=100 --interval=0 --simulationTime=6"
```

The simulation prints results in the terminal and writes `reverse-proxy-animation.xml` in the NS-3 directory. Open it from there with:

```bash
netanim reverse-proxy-animation.xml
```

See [the full NS-3 instructions](ns-3-simulation/README.md) for congestion, failure/recovery, and comparison commands.

## More documentation

- [NS-3 simulation instructions](ns-3-simulation/README.md)
- [Spring Boot demo](loadbalancer-demo/README.md)

GitHub: https://github.com/piyush0070jaiswal/reverseproxy
