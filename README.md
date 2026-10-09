# Reverse Proxy Load Balancer

A Spring Boot reverse proxy with Weighted Round Robin (WRR) and Least Connections (LC), plus an NS-3 simulation with 20 clients.

## Show the NS-3 README

Paste this command in the terminal:

```bash
cd "/home/piyush/Downloads/Reverse-Proxy-Load-Balancer-master (1)/Reverse-Proxy-Load-Balancer-master" && less ns-3-simulation/README.md
```

Press `q` to exit.

## Run NS-3

From the NS-3 installation directory:

```bash
cd /home/piyush/Downloads/ns-3
cp "/home/piyush/Downloads/Reverse-Proxy-Load-Balancer-master (1)/Reverse-Proxy-Load-Balancer-master/ns-3-simulation/scratch/reverse-proxy-load-balancer.cc" scratch/
```

### WRR

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=wrr --requests=100 --simulationTime=6"
```

### Least Connections

```bash
./ns3 run "scratch/reverse-proxy-load-balancer --algorithm=least --requests=100 --interval=0 --simulationTime=6"
```

The simulation shows 20 clients and prints the requests handled by each server.

## Open NetAnim

Run one simulation first, then open the generated animation:

```bash
netanim /home/piyush/Downloads/ns-3/reverse-proxy-animation.xml
```

Click **Play** in NetAnim. It shows Client-1 through Client-20, the load balancer, and Server-1 through Server-3.

Install NetAnim on Arch/Omarchy:

```bash
yay -S netanim
```

Install NetAnim on Ubuntu/Debian:

```bash
sudo apt update
sudo apt install netanim
```

## More documentation

- [NS-3 instructions](ns-3-simulation/README.md)
- [Spring Boot demo](loadbalancer-demo/README.md)

GitHub: https://github.com/piyush0070jaiswal/reverseproxy
