# Demo Guide

**Workflow:** Wake backends → Public `/hello` → Weighted Round Robin → Concurrent least connections → Original `/stats` → Local Kubernetes.

This demonstrates the existing Java Spring Boot reverse proxy and three backend services. No new website or UI was created.

## 1. Wake the three backends first

Open each link in a separate browser tab and wait until all three respond:

| Link | Expected response |
| --- | --- |
| <https://reverseproxy-backend1.onrender.com/health> | `SERVER-1 is UP` |
| <https://reverseproxy-backend2.onrender.com/health> | `SERVER-2 is UP` |
| <https://reverseproxy-backend3.onrender.com/health> | `SERVER-3 is UP` |

Render Free services sleep after inactivity. Allow **1–2 minutes** for cold starts and reload if necessary. Proceed only when all three checks pass. These are manual pre-demo checks, not an automated keepalive.

## 2. Open the public reverse proxy

<https://reverseproxy-proxy.onrender.com/hello>

Expected response, for example:

```text
Hello from SERVER-1 running on port 10000
```

The selected server can differ. The client sends a request to the proxy, the proxy selects a backend, and the response returns through the proxy. The backend intentionally takes about five seconds to respond. `/hello` defaults to Weighted Round Robin.

## 3. Test Weighted Round Robin

<https://reverseproxy-proxy.onrender.com/hello?algorithm=wrr>

Count the first response, then refresh **five more times**, waiting for each response: **six requests total**.

| Backend | Weight | Requests per complete six-request cycle |
| --- | ---: | ---: |
| SERVER-1 | 3 | 3 |
| SERVER-2 | 2 | 2 |
| SERVER-3 | 1 | 1 |

These are configured proportions, not measured server capacities. The starting position depends on earlier requests. Other users sending requests or a proxy restart during the test can affect observations.

## 4. Test Least Connections

Endpoint:

<https://reverseproxy-proxy.onrender.com/hello?algorithm=least>

For a meaningful test, send six simultaneous requests from **your computer's terminal**, not Render. No project directory is required for this public HTTP test:

```sh
printf '%s\n' 1 2 3 4 5 6 | xargs -P 6 -I '{}' \
  curl --fail --silent --show-error --connect-timeout 10 --max-time 120 --write-out '\n' \
  'https://reverseproxy-proxy.onrender.com/hello?algorithm=least'
```

The proxy selects the backend with the fewest active requests. Sequential browser refreshes may find all servers idle, so they do not clearly demonstrate this algorithm. Our concurrent tests produced 2:2:2, but an exact split is not guaranteed. Response order is not fixed, and concurrent terminal output can interleave.

## 5. View the original statistics

<https://reverseproxy-proxy.onrender.com/stats>

Refresh this page to view:

- Total requests.
- Requests assigned to each backend.
- Current active requests.

Active counts should return to zero once all requests finish and no other traffic is active. Totals include earlier traffic and reset when the proxy process restarts. This is the existing statistics page, not a new dashboard.

## 6. Run the separate local Kubernetes demo

Render hosts the public deployment. Minikube runs a **separate local deployment of the same application**.

### Enter the project directory

Run in your computer's terminal:

```sh
cd /home/piyush/Downloads/reverseproxy_final
```

The helper commands below use tools and cluster credentials already installed on this computer under ignored `.deployment-local/`. On another computer or a fresh clone, follow the [Minikube setup instructions](../k8s/README.md) first. Local binaries and credentials are not published to GitHub.

### Check the running services

```sh
sh k8s/local-tools.sh kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo get deployments,pods,services
```

Expect four Deployments showing **1/1 Ready**: `proxy`, `backend1`, `backend2`, and `backend3`, plus four running application pods and four ClusterIP Services.

If the cluster is stopped, restart it first. A system authentication dialog may appear for temporary Docker access; enter your password only there:

```sh
sh k8s/local-tools.sh minikube start --profile=reverseproxy-demo --driver=docker --container-runtime=docker --kubernetes-version=v1.37.2 --cpus=4 --memory=4096 --keep-context --wait=all --wait-timeout=5m
sh k8s/local-tools.sh kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo wait --for=condition=Available deployment --all --timeout=300s
```

### Open the local connection

```sh
sh k8s/local-tools.sh kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo port-forward --address=127.0.0.1 service/proxy 18081:8080
```

Wait for:

```text
Forwarding from 127.0.0.1:18081 -> 8080
```

**Leave that terminal open**, then visit:

- <http://127.0.0.1:18081/hello>
- <http://127.0.0.1:18081/stats>

These links work only on the computer running the port-forward. If port 18081 is already forwarded in another terminal, use that connection instead of starting a duplicate.

Kubernetes manages the application through Deployments, Service networking, and health probes. Automatic backend pod replacement and routing after recovery were verified; this is not automatic request-level failover.

Press **Ctrl+C** to stop port-forwarding without stopping the cluster or affecting Render. To release local cluster resources afterward:

```sh
sh k8s/local-tools.sh minikube stop --profile=reverseproxy-demo
```

## If the public proxy returns 502

1. Open all three backend `/health` links in step 1 and wait for successful responses.
2. Retry the public proxy `/hello` link.
3. If it still fails, inspect the Render proxy environment variables and logs. Each `BACKEND_N_URL` must match its backend's public HTTPS origin, without a `/hello` or `/health` suffix.

Render Free cold starts can initially fail requests. This is a demo deployment, not an always-on production availability guarantee. Use only small demonstration tests, not sustained public load tests.

## Deployment summary

| Tool | Purpose |
| --- | --- |
| Docker | Packages the proxy and three backends into four images. |
| Docker Compose | Runs the four services together locally; proxy port 18080. |
| Render Free | Hosts the existing application through public HTTPS endpoints. |
| Minikube / Kubernetes | Demonstrates local orchestration, health probes, and pod replacement; proxy forwarded on port 18081. |

The proxy uses three configured backend addresses. Dynamic backend discovery, autoscaling, and automatic request retries are not included. Both load-balancing algorithms and all 13 proxy regression tests passed during verification.
