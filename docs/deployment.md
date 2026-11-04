# Docker preparation and deployment guide

## State and scope

Local Docker verification was previously completed for one proxy and exactly three backends, under Compose project `sto-demo`. The local proxy is available at **http://127.0.0.1:18080** while the stack is running. There is **no public URL yet**, no Render deployment, and no new website. Separate Minikube configurations are now prepared in [`../k8s/`](../k8s/README.md), but have not been run against a cluster. Render Free Blueprint files are prepared. They must be present on GitHub before creating the Blueprints. The user has approved publishing these deployment changes and reports being signed in to the Render Dashboard; this editor does not have authenticated Render API access. Dynamic backend discovery is outside this deployment-only scope.

Latest checks on 2026-11-04: Compose validation, Render Blueprint JSON Schema validation, and 13 host proxy regression tests passed. The running local proxy on port 18080 returned successful health, six WRR responses with 3:2:1 counts, six concurrent least-connections responses distributed 2:2:2 in this run, and the original `/stats`. This does not establish Render or Kubernetes success.

Validation passed: Compose configuration, all four Docker image builds, 13 proxy regression tests inside the Java 17 build image, four healthy runtime containers, real WRR and concurrent least-connections requests, correct statistics, HTTP 502 with one backend stopped, and successful routing after that backend restarted. Non-root UID/GID `10001:10001`, 512 MiB limits, dropped capabilities, and loopback-only proxy publication were confirmed on the running containers.

Ordinary Docker access is still denied for this user. Verification used one-time administrator authorization through the system authentication dialog; Docker group membership and socket permissions were not changed. The commands below use `sudo` for your own interactive terminal. Never send your password in chat or make the Docker socket world-writable. If your approved Docker setup already permits unprivileged access, omit `sudo`.

### Application configuration

The Java configuration changes are implemented. Maven builds pass for all four applications, and 13 proxy regression tests pass both on the host JDK and inside the proxy's Java 17 Docker build stage. Packaged-application and Docker HTTP checks verified routing and backend recovery. No Render deployment or Kubernetes verification has occurred.

Implemented contract:

- All four applications honor `PORT` and `SERVER_ADDRESS` and expose a fast `GET /health`.
- Proxy honors `BACKEND_1_URL`, `BACKEND_2_URL`, and `BACKEND_3_URL`, retaining standalone defaults `http://localhost:8081`, `http://localhost:8082`, and `http://localhost:8083`.
- Proxy honors `UPSTREAM_TIMEOUT_MS`, default `90000` milliseconds.
- Proxy preserves `GET /hello?algorithm=wrr` (weights **3:2:1**), `GET /hello?algorithm=least`, and `GET /stats`.
- Each backend's `/hello` retains its default five-second delay. `/health` must not have that delay.

## Image and Compose design

| Service | Module-local build context | Build image | Runtime image | Packaged JAR |
| --- | --- | --- | --- | --- |
| proxy | `loadbalancerReverse` | `maven:3.9-eclipse-temurin-17` | `eclipse-temurin:17-jre-noble` | `target/loadbalancerReverse-1.0-SNAPSHOT.jar` |
| backend1 | `loadbalancer-demo/server1` | `maven:3.9-eclipse-temurin-25` | `eclipse-temurin:25-jre-noble` | `target/server1-1.0.0.jar` |
| backend2 | `loadbalancer-demo/server2` | `maven:3.9-eclipse-temurin-25` | `eclipse-temurin:25-jre-noble` | `target/server2-1.0.0.jar` |
| backend3 | `loadbalancer-demo/server3` | `maven:3.9-eclipse-temurin-25` | `eclipse-temurin:25-jre-noble` | `target/server3-1.0.0.jar` |

The proxy uses Spring Boot 3.5.5 / Java 17; backends use Spring Boot 3.5.16 / Java 25. Every multi-stage build runs `mvn --batch-mode --no-transfer-progress package`, including the Maven test phase without skip flags (modules without tests do not acquire tests from this setup). Host `target/` artifacts are excluded and not reused. The module allowlist in each `.dockerignore` excludes unrelated files, VCS content, `.env` files, and common secret-key files. Do not put credentials into source files or build inputs.

Runtimes contain curl and run Java as UID/GID `10001:10001`. Each image defaults to `PORT=8080`, `SERVER_ADDRESS=0.0.0.0`, and `JAVA_TOOL_OPTIONS=-XX:InitialRAMPercentage=10.0 -XX:MaxRAMPercentage=50.0`. Under Compose's 512 MiB limit this caps the Java heap at approximately 256 MiB, leaving headroom for metaspace, code cache, native memory, and thread stacks. This is not a guarantee against an out-of-memory condition under load.

Compose adds `init: true`, `no-new-privileges`, all capabilities dropped, and `mem_limit: 512m` per container. Curl checks `/health`; proxy startup waits for all three backends to be healthy. Startup ordering does not provide failover or recovery guarantees after startup. Image-level checks also use the runtime `PORT` value.

All containers listen on port 8080. Proxy URLs are `http://backend1:8080`, `http://backend2:8080`, and `http://backend3:8080` on the Compose network. Only proxy port 8080 is published, bound to **127.0.0.1**, with host port `PROXY_PORT` defaulting to 8080. No backend ports are published. Local loopback access is not a public deployment.

| Environment variable | Default outside Docker | Purpose |
| --- | --- | --- |
| `PORT` | Proxy `8080`, backends `8081`/`8082`/`8083` | Spring Boot HTTP port; set to `10000` on Render |
| `SERVER_ADDRESS` | `0.0.0.0` | Bind address; tests can bind loopback |
| `BACKEND_1_URL` | `http://localhost:8081` | Proxy's first backend base URL |
| `BACKEND_2_URL` | `http://localhost:8082` | Proxy's second backend base URL |
| `BACKEND_3_URL` | `http://localhost:8083` | Proxy's third backend base URL |
| `UPSTREAM_TIMEOUT_MS` | `90000` | Positive whole-request upstream timeout in milliseconds |
| `PROXY_PORT` | `8080` | Compose host port only; does not change the container port |

Backend URLs must be HTTP(S) base URLs without embedded credentials, query strings, or fragments. Configure them as trusted operator settings, never from incoming request parameters. Backend failures return HTTP 502; timed-out upstream calls return HTTP 504. Active-request reservations are released on success or failure. This phase does not add automatic retries, health-based failover, or dynamic backend discovery.

The proxy's `/health` is a fast process health check: a 200 response does **not** prove that the backends are available. Verify `/hello` responses separately. Counters are per proxy process and reset when it restarts; they are not durable metrics.

## Local validation and operation

Run from the exact repository directory:

```sh
cd '/home/piyush/Downloads/reverseproxy_final'
docker compose config --quiet
```

No local `.env` file is required. `.env.example` is an optional configuration template; do not add credentials to it. Local `.env` files are ignored by Git. The commands explicitly set `PROXY_PORT=18080`; the Compose default remains 8080. Container ports remain 8080.

The verified stack uses host port 18080 because your original Java apps are using ports 8080–8083. Those apps were left untouched. Backend container ports are not published. Use project name `sto-demo` consistently to manage this stack without creating another project.

Build, start, and inspect the same stack:

```sh
cd '/home/piyush/Downloads/reverseproxy_final'
sudo docker compose -p sto-demo build && sudo env PROXY_PORT=18080 docker compose -p sto-demo up -d --wait --wait-timeout 240
sudo docker compose -p sto-demo ps
curl --fail --silent --show-error --max-time 10 http://127.0.0.1:18080/health
```

`build` downloads Maven/Temurin images, Ubuntu curl packages, and Maven dependencies; it requires network access. Do not treat the stack as verified unless builds (including tests), the health wait, and the following requests succeed. If the wait fails, inspect logs rather than weakening health checks.

### WRR: six sequential requests

With no competing requests and a fresh proxy, expect `SERVER-1, SERVER-1, SERVER-1, SERVER-2, SERVER-2, SERVER-3` over a six-request WRR cycle. Each call takes roughly five seconds because the backend delay is intentionally unchanged.

```sh
for request in 1 2 3 4 5 6; do
  curl --fail --silent --show-error --max-time 120 --write-out '\n' \
    'http://127.0.0.1:18080/hello?algorithm=wrr' || break
done
```

### Least connections: six concurrent requests

Run all six together to exercise overlapping five-second backend requests. Observe successful backend responses and `/stats`; scheduling and tie-breaking can affect distribution, so do not assert a strict response order or exact split.

```sh
printf '%s\n' 1 2 3 4 5 6 | xargs -P 6 -I '{}' \
  curl --fail --silent --show-error --max-time 120 --write-out '\n' \
  'http://127.0.0.1:18080/hello?algorithm=least'
curl --fail --silent --show-error --max-time 10 http://127.0.0.1:18080/stats
```

Inspect logs from the repository directory:

```sh
sudo docker compose -p sto-demo logs --tail=100 proxy backend1 backend2 backend3
```

### Backend stop/restart verification

This was tested on the running stack: stop `backend1`, send a least-connections request with no other requests active, observe HTTP 502, restart `backend1`, and verify its response returns again. This verifies manual container recovery, not automatic failover or Kubernetes Pod replacement. Use these commands from the repository directory if you want to reproduce it:

```sh
sudo docker compose -p sto-demo stop -t 10 backend1
curl --silent --show-error --max-time 120 --write-out '\nHTTP %{http_code}\n' 'http://127.0.0.1:18080/hello?algorithm=least'
sudo env PROXY_PORT=18080 docker compose -p sto-demo up -d --no-deps --wait --wait-timeout 120 backend1
curl --fail --silent --show-error --max-time 120 'http://127.0.0.1:18080/hello?algorithm=least'
```

When you have finished testing, stop and remove only this Compose stack:

```sh
sudo docker compose -p sto-demo down
```

## Render Free deployment

The existing application is the only deployment target; no new frontend or dashboard is created. Local Compose configuration and all 13 proxy tests pass. The Docker builds and runtime checks above were previously verified; current container inspection requires administrator access.

### Recommended: two staged Blueprints

Two configurations use the existing Dockerfiles, explicitly select `plan: free`, and disable automatic redeploys:

- [`../render-backends.yaml`](../render-backends.yaml): exactly three backend Web Services.
- [`../render.yaml`](../render.yaml): the existing proxy Web Service only.

Deployment requires these files and the existing Docker/application changes on `main` at `https://github.com/piyush0070jaiswal/reverseproxy`. Publishing has been approved by the user. Render Dashboard sign-in is available to the user, but Render CLI/API access is not configured in this editor. Never put account credentials or API keys in Git or chat.

1. Obtain approval to commit/push the deployment changes. Publish them without including unrelated files or secrets.
2. Sign in at <https://dashboard.render.com>. Grant the Render GitHub integration access to **only** this repository. Check billing/usage controls and confirm that all resources use Free compute; do not accept paid upgrades.
3. Create a **Blueprint** from this repository and choose Blueprint Path **`render-backends.yaml`**. Review the three service names first: if any already exist, stop and inspect them to avoid changing an unrelated service. Confirm only three Free Docker Web Services and no databases/disks/workers are listed, then deploy.
4. Wait for all three backends to become live. Copy each service's **actual public HTTPS origin** from Render. Verify `/health` and `/hello` on each, with a client timeout of 180 seconds for cold starts. Confirm the server identity; never guess an `onrender.com` hostname.
5. Create a **second Blueprint** from the same repository using Blueprint Path **`render.yaml`**. Review the proxy name for collisions. Supply the three verified public HTTPS origins in the initial `BACKEND_1_URL`, `BACKEND_2_URL`, and `BACKEND_3_URL` prompts, in server order. Use origins only, with no `/hello` or `/health` suffix. Confirm exactly one Free Docker Web Service and deploy it.
6. Copy the proxy's **actual public HTTPS URL**. Verify `/health`, `/hello?algorithm=wrr`, `/hello?algorithm=least`, and the original `/stats`. Repeat the six-request WRR and concurrent least-connections checks above against that actual origin, after waking the backends. Six WRR calls should give counts of 3, 2, and 1 across the backends if no other WRR traffic intervenes; concurrent least-connections response order is not fixed. Only report success after these real public requests pass.

`sync: false` prompts are used only during initial proxy Blueprint creation. For later backend URL changes, edit the proxy environment variables in Render and redeploy. Blueprints use repository-root-relative Dockerfile/context paths and no `rootDir`; no start command override is required. Render uses the existing image entrypoint. Automatic redeploys are off, so future Git changes require an explicit deploy/sync.

Free backends cannot receive private-network traffic. Do not replace their public HTTPS origins with Blueprint `host`/`hostport` properties or Compose DNS names. No actual public URL is known until Render provisions the services.

### Alternative: individual Web Services

Use this instead of Blueprints, not in addition to them, to avoid duplicate services.

1. After approval, push the verified application and Docker files to the user's intended GitHub repository. In Render's GitHub integration, grant access to **only that repository**, not all repositories.
2. Create exactly **four Docker Web Services**, each using the **Free** instance type. These deploy the existing APIs, not a new website. Use the same approved repository/branch and the module settings below. Do not select a paid service type.

   | Service | Root Directory | Dockerfile Path | Docker Build Context Directory |
   | --- | --- | --- | --- |
   | backend1 | `loadbalancer-demo/server1` | `./Dockerfile` | `.` |
   | backend2 | `loadbalancer-demo/server2` | `./Dockerfile` | `.` |
   | backend3 | `loadbalancer-demo/server3` | `./Dockerfile` | `.` |
   | proxy | `loadbalancerReverse` | `./Dockerfile` | `.` |

   Dockerfile and context paths are relative to each selected Root Directory. Render builds these Dockerfiles directly; it does not apply Compose limits, dependency ordering, or security settings.
3. Deploy the three backends first. For every service set `PORT=10000`, `SERVER_ADDRESS=0.0.0.0`, and Health Check Path `/health`. The image's `EXPOSE 8080` is metadata, not a restriction: the application must honor `PORT=10000`. Keep the image's memory-headroom JVM defaults.
4. Copy each backend's **actual public HTTPS origin from its Render Dashboard**. Open or curl each actual URL with `/health` and wait for a successful response, allowing for cold starts. Verify that each URL corresponds to the intended backend. Do not guess hostnames, use Compose names, or substitute private addresses. Free web services cannot receive private-network traffic, so this Free setup uses public HTTPS backends.
5. Create/configure the proxy with `PORT=10000`, `SERVER_ADDRESS=0.0.0.0`, Health Check Path `/health`, and `UPSTREAM_TIMEOUT_MS=90000`. Set `BACKEND_1_URL`, `BACKEND_2_URL`, and `BACKEND_3_URL` to the corresponding **actual HTTPS origins copied and verified in step 4**, without appending `/health` or `/hello`. Never populate these with invented URLs. A 90-second upstream timeout allows some cold-start time but cannot guarantee success when a backend is waking up.
6. Deploy the proxy only after its backends have been verified. Copy the proxy's actual public HTTPS URL from its Dashboard and verify `/health`, `/hello?algorithm=wrr`, `/hello?algorithm=least`, and `/stats` there. Render provides managed HTTPS/TLS. No public URL exists for this project until an actual deployment succeeds and its URL is verified.

Public backend endpoints in this Free layout are directly reachable, unlike the local Compose setup. This is a demo deployment, not an authenticated/private production architecture. Do not send sensitive traffic without a separate security review. Proxy and backends may sleep independently; wake and verify the backends manually before a demo, not with an automated keepalive.

### Render Free limitations and cost caveats

Review the current terms before approving deployment: <https://render.com/docs/free>.

- **750 Free instance hours per workspace per calendar month**, shared across all Free web services, not 750 per service. Four continuously running services would exhaust that allocation; this is not a 24/7 hosting promise.
- Services spin down after **15 minutes without inbound traffic**; a cold start takes **about one minute**. Proxy plus backend cold starts can compound and exceed client/upstream timeouts.
- Free web services cannot receive inbound private-network traffic. Public HTTPS proxy-to-backend traffic counts toward applicable usage limits.
- Outbound bandwidth and build pipeline minutes have workspace quotas. Without a payment method, exhaustion can suspend services or disable builds; with a payment method, supplementary usage can incur charges subject to applicable spending controls. Selecting Free compute does **not** guarantee zero charges.
- Render may suspend a Free service that initiates unusually high public-internet traffic, which is particularly relevant to a proxy workload. Do not run sustained load tests there.
- Local files are ephemeral. Do not add paid persistent disks, databases, or other paid resources for this demo. No automated keepalive/pinger is included or recommended.
- Review usage, billing controls, and current plan terms in the Dashboard. Stop if a paid option is required rather than silently upgrading.

## Local Kubernetes demonstration

See [`../k8s/README.md`](../k8s/README.md) for a separate named Minikube profile, four local image builds, Deployments, ClusterIP Services, startup/readiness/liveness probes, tests, and scoped cleanup. It uses loopback port **18081**, separate from Compose's **18080**. No application code or algorithms are changed.

The manifests passed offline YAML/configuration checks, but `kubectl` and `minikube` are not installed in this environment and Docker requires administrator-authorized access. No Kubernetes deployment or runtime verification is claimed. Successful local Compose verification, Kubernetes execution, and public HTTPS verification are distinct milestones. Dynamic backend discovery and autoscaling are not part of this deployment.
