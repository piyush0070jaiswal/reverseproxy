# Reverse Proxy Load Balancer

A Spring Boot reverse proxy with Weighted Round Robin (WRR) and Least Connections (LC), plus an NS-3 simulation of 20 clients, one load balancer, and three backends.

## Java application: Docker and deployment

Dockerfiles and a four-service Compose configuration are available for the existing Java proxy and backends. No new website or UI was created; the existing `/stats` page is preserved. The application is deployed and verified at **https://reverseproxy-proxy.onrender.com/hello**, with original statistics at **https://reverseproxy-proxy.onrender.com/stats**. Render Free backends may need to wake up before requests succeed.

The four Docker images have been built and the local `sto-demo` stack has been verified, including WRR, concurrent least-connections, and backend stop/restart recovery. The following commands reproduce that setup on port **18080** without interfering with your original Java proxy on port 8080:

```bash
cd "/home/piyush/Downloads/reverseproxy_final"
docker compose config --quiet && sudo docker compose -p sto-demo build && sudo env PROXY_PORT=18080 docker compose -p sto-demo up -d --wait --wait-timeout 240
```

This computer requires administrator-authorized Docker access. Enter your password only in your own terminal or the system authentication dialog, never in chat. These commands do not change Docker group membership or socket permissions. If your Docker setup already allows unprivileged access, omit `sudo`.

After the containers are healthy, test the proxy:

```bash
curl --fail http://127.0.0.1:18080/health
curl --fail --max-time 120 "http://127.0.0.1:18080/hello?algorithm=wrr"
curl --fail --max-time 120 "http://127.0.0.1:18080/hello?algorithm=least"
```

View the existing statistics at **http://127.0.0.1:18080/stats**. Backend responses intentionally take about five seconds. A sequential least-connections request does not demonstrate load distribution; use the concurrent test in the guide.

See [the deployment guide](docs/deployment.md) for separate WRR/least-connections checks, environment variables, logs, shutdown, Render Free settings, limitations, and current validation status. The Compose URLs above are loopback-only; the verified public Render URLs are listed at the top. Do not make the Docker socket world-writable.

- **Render Free:** deploy [`render-backends.yaml`](render-backends.yaml) first, then [`render.yaml`](render.yaml) with the three actual public HTTPS backend origins. These deploy only the existing Java services. See the guide for the two-Blueprint workflow. Public routing was verified for WRR 3:2:1 and concurrent least connections.
- **Local Kubernetes:** separate [Minikube manifests and instructions](k8s/README.md) were verified in a dedicated profile with four ready Deployments, WRR, concurrent least connections, and automatic backend pod replacement. Use `sh k8s/local-tools.sh kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo port-forward --address=127.0.0.1 service/proxy 18081:8080` to reopen the local demo on port **18081**.

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
