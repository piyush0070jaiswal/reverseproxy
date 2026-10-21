# Reverse Proxy Load Balancer

A Spring Boot reverse proxy with Weighted Round Robin (WRR) and Least Connections (LC), plus an NS-3 simulation of 20 clients, one load balancer, and three backends.

## NS-3 simulation

The NS-3 scenario supports configurable bandwidth and delay, backend link failure/recovery, terminal performance metrics, NetAnim output, and measured WRR-versus-least-connections comparison.

### Copy and run from a terminal

These copy/paste commands use the current computer's folders. NS-3 is installed in `/home/piyush/Downloads/ns-3`, and this project is in `/home/piyush/Downloads/reverseproxy-main (2)`. Run the copy command once to place the simulation in NS-3's `scratch/` folder:

```bash
cp "/home/piyush/Downloads/reverseproxy-main (2)/ns-3-simulation/scratch/reverse-proxy-load-balancer.cc" "/home/piyush/Downloads/ns-3/scratch/"
```

#### Run Weighted Round Robin (WRR)

Paste this command to run WRR and print its measured results in the terminal:

```bash
cd "/home/piyush/Downloads/ns-3" && ./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=100 --simulationTime=6"
```

#### Run Least Connections

Paste this separate command to run the least-connections strategy:

```bash
cd "/home/piyush/Downloads/ns-3" && ./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=least --requests=100 --interval=0 --simulationTime=6"
```

Each run writes `reverse-proxy-animation.xml` in the NS-3 folder. Open the most recently generated animation with:

```bash
netanim "/home/piyush/Downloads/ns-3/reverse-proxy-animation.xml"
```

See [the full NS-3 instructions](ns-3-simulation/README.md) for copy/paste commands for congestion, backend failure/recovery, and comparison runs. If your folders are in different locations, change the paths in the commands to match.

## More documentation

- [NS-3 simulation instructions](ns-3-simulation/README.md)
- [Spring Boot demo](loadbalancer-demo/README.md)

GitHub: https://github.com/piyush0070jaiswal/reverseproxy
