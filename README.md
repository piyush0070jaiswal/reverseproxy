# Reverse Proxy Load Balancer

A Spring Boot reverse proxy with Weighted Round Robin (WRR) and Least Connections (LC), plus an NS-3 simulation of 20 clients, one load balancer, and three backends.

## NS-3 simulation

The NS-3 scenario supports configurable bandwidth and delay, backend link failure/recovery, terminal performance metrics, NetAnim output, and measured WRR-versus-least-connections comparison.

See [the NS-3 instructions](ns-3-simulation/README.md) for setup and commands, including:

- Standard WRR and least-connections runs
- Congestion tests using `dataRate`, `delay`, `packetSize`, and offered load
- Backend failure/recovery using `simulateFailure`, `failureAt`, and `recoveryAt`
- Strategy comparison using `compare=1`
- Opening topology and packet movement in NetAnim

## More documentation

- [NS-3 simulation instructions](ns-3-simulation/README.md)
- [Spring Boot demo](loadbalancer-demo/README.md)

GitHub: https://github.com/piyush0070jaiswal/reverseproxy
