# Local Minikube deployment

A separate, local-only deployment of the existing Java app. Nothing here changes
application source, UI, Dockerfiles, or Compose. Run commands from the repository
root (`reverseproxy_final/`), unless a section says otherwise. Commands use literal
names; no secrets, registry credentials, shell substitutions, or environment setup
are required.

## Layout and constraints

| Manifest | Workload | Existing Docker build context | Java | Local image |
| --- | --- | --- | --- | --- |
| `namespace.yaml` | Namespace `reverseproxy-demo` | — | — | — |
| `proxy.yaml` | Deployment + ClusterIP Service `proxy` | `loadbalancerReverse` | 17 | `reverseproxy-proxy:local` |
| `backend1.yaml` | Deployment + ClusterIP Service `backend1` | `loadbalancer-demo/server1` | 25 | `reverseproxy-backend1:local` |
| `backend2.yaml` | Deployment + ClusterIP Service `backend2` | `loadbalancer-demo/server2` | 25 | `reverseproxy-backend2:local` |
| `backend3.yaml` | Deployment + ClusterIP Service `backend3` | `loadbalancer-demo/server3` | 25 | `reverseproxy-backend3:local` |

- Dedicated Minikube profile/context **`reverseproxy-demo`**, namespace
  **`reverseproxy-demo`**. Every Kubernetes command pins the context explicitly.
  Do not reuse this namespace/profile for unrelated work.
- Exactly one replica per Deployment. `Recreate` avoids transient extra replicas
  during updates, at the cost of downtime. No HPA, autoscaling, Ingress, NodePort,
  LoadBalancer Service, host ports, host networking, or dynamic backend discovery.
- All containers and Services use port **8080**, `PORT=8080`, and
  `SERVER_ADDRESS=0.0.0.0`. The proxy's three fixed URLs are
  `http://backend1:8080`, `http://backend2:8080`, `http://backend3:8080`;
  `UPSTREAM_TIMEOUT_MS=90000`. Kubernetes DNS resolves these Service names within
  the namespace; it does not add backends to the app's fixed list.
- Images are built **inside the named Minikube profile** and use
  `imagePullPolicy: Never`. No registry push, host Docker build, or Docker-env
  switching is needed. Builds still need network access for base images, Maven
  dependencies, and the existing Dockerfiles' package installation.
- Each container requests 100m CPU / 256Mi and limits 1 CPU / **512Mi**.
  Existing Dockerfiles set a 50% maximum JVM heap (about 256Mi at that limit),
  leaving space for native JVM memory. Four containers can use up to 2Gi total;
  the Minikube node and image builds require additional capacity.
- Runtime UID/GID **10001**, non-root required, all capabilities dropped,
  privilege escalation disabled, `RuntimeDefault` seccomp, and no mounted
  ServiceAccount token. Writable container filesystems are retained for JVM
  temporary files; there are no host mounts or persistent volumes.
- Kubernetes HTTP probes explicitly use fast **`/health`**, not the delayed
  `/hello`. Startup allows about **180 seconds** (36 checks, every 5 seconds),
  suppressing readiness/liveness until it succeeds. Afterwards readiness checks
  every 5 seconds and liveness every 10 seconds, with 2-second probe timeouts.
  Dockerfile `HEALTHCHECK` is not used by Kubernetes.
- `/health` reports only the individual Java process; a healthy proxy does **not**
  prove its upstreams work. Check all rollouts and the routing tests below.
- The namespace provides resource/name separation, **not a network security
  boundary**. ClusterIP Services remain reachable inside the cluster. Only the
  documented host port-forward is exposed, bound to `127.0.0.1:18081`.
  Existing Compose on **18080** remains independent and untouched.

## 1. Install prerequisites yourself (not performed by this setup)

Required: Linux, a working **non-sudo Docker daemon connection** for your login,
Minikube, a compatible kubectl (within one minor version of the cluster), curl,
Python 3, and GNU `timeout` from coreutils. Python's standard library suffices for
the request tests. PyYAML is optional for offline YAML parsing. Reserve at least
4 CPUs and 4Gi for the Minikube node, plus host/build headroom (8Gi host RAM is a
reasonable starting point). No host JDK or Maven is required.

Use the official installation instructions appropriate to your OS:

- Docker: https://docs.docker.com/engine/install/
- Non-root Docker access: https://docs.docker.com/engine/install/linux-postinstall/
- Minikube: https://minikube.sigs.k8s.io/docs/start/
- kubectl: https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/

For **Arch Linux only**, these are explicit, bounded package-install/service
commands to run yourself after reviewing them. They are not executed by this
repository. `sudo -n` requires already-authorized sudo and fails instead of
prompting; authorize sudo separately if needed.

```sh
timeout 600s sudo -n pacman -S --needed --noconfirm docker kubectl minikube curl python python-yaml coreutils
timeout 60s sudo -n systemctl enable --now docker
```

Daemon startup alone does not grant your login Docker access. Follow Docker's
post-install guide or ask your administrator to grant the appropriate access,
then log out/in as instructed. Docker-group membership effectively grants root
privileges; do not make the socket world-writable. Do **not** solve access errors
by running Minikube or this workflow as root/with sudo.

Preflight (stop if any required command is missing or `docker info` fails):

```sh
command -v docker minikube kubectl curl python3 timeout
timeout 20s docker info
timeout 10s minikube version
timeout 10s kubectl version --client
```

## 2. Start the dedicated profile

```sh
timeout 600s minikube start --profile=reverseproxy-demo --driver=docker --cpus=4 --memory=4096 --keep-context --wait=all --wait-timeout=5m
timeout 20s minikube status --profile=reverseproxy-demo
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s get nodes
```

`--keep-context` avoids changing your current kubectl context. A timed-out command
is a failure to investigate, not evidence of readiness. The outer `timeout`
commands bound client runtime; they may not cancel work already accepted by a
cluster or builder. Check status before retrying.

## 3. Build the four existing images in this profile

Run in order; stop on the first failure. Each build is capped at 15 minutes and
uses the context's existing Dockerfile, including its Maven build/tests. No other
profile or host image store is used.

```sh
timeout 900s minikube --profile=reverseproxy-demo image build --tag=reverseproxy-proxy:local loadbalancerReverse
timeout 900s minikube --profile=reverseproxy-demo image build --tag=reverseproxy-backend1:local loadbalancer-demo/server1
timeout 900s minikube --profile=reverseproxy-demo image build --tag=reverseproxy-backend2:local loadbalancer-demo/server2
timeout 900s minikube --profile=reverseproxy-demo image build --tag=reverseproxy-backend3:local loadbalancer-demo/server3
timeout 30s minikube --profile=reverseproxy-demo image ls
```

Confirm all four tags before applying. `ErrImageNeverPull` means an image is
missing from this profile; build it there rather than changing the pull policy.

## 4. Apply and wait for every workload

Namespace creation is intentionally first; no reliance on directory file order.

```sh
timeout 30s kubectl --context=reverseproxy-demo --request-timeout=20s apply -f k8s/namespace.yaml
timeout 60s kubectl --context=reverseproxy-demo --request-timeout=20s --namespace=reverseproxy-demo apply -f k8s/backend1.yaml -f k8s/backend2.yaml -f k8s/backend3.yaml -f k8s/proxy.yaml
timeout 330s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo rollout status deployment/backend1 --timeout=300s
timeout 330s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo rollout status deployment/backend2 --timeout=300s
timeout 330s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo rollout status deployment/backend3 --timeout=300s
timeout 330s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo rollout status deployment/proxy --timeout=300s
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s --namespace=reverseproxy-demo get deployments,pods,services -o wide
```

Expect four ready Deployments (1/1), four pods, and four ClusterIP Services.
Stop and diagnose if any rollout fails; do not treat proxy readiness alone as
success. For example:

```sh
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s --namespace=reverseproxy-demo get events --sort-by=.metadata.creationTimestamp
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s --namespace=reverseproxy-demo describe deployment/proxy
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s --namespace=reverseproxy-demo logs deployment/proxy --tail=100
```

For a later image rebuild with the same `:local` tag, repeat the relevant build,
then explicitly restart the affected Deployment and wait for its rollout. An
unchanged manifest alone will not replace an already-running image. Example:

```sh
timeout 30s kubectl --context=reverseproxy-demo --request-timeout=20s --namespace=reverseproxy-demo rollout restart deployment/proxy
timeout 330s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo rollout status deployment/proxy --timeout=300s
```

Proxy restarts reset its in-memory counts and WRR position; backend restarts also
interrupt requests. Recreate is intended for local demos, not zero-downtime use.

## 5. Expose the proxy on loopback only

In a **separate terminal**, from the repository root:

```sh
timeout 1800s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo port-forward --address=127.0.0.1 service/proxy 18081:8080 --pod-running-timeout=60s
```

Wait for `Forwarding from 127.0.0.1:18081 -> 8080`. This foreground command stops
with Ctrl-C or after 30 minutes; rerun it if needed, including after a proxy pod
replacement. Do not use `--address=0.0.0.0`, `minikube service`, or `minikube tunnel`.
No backend host port-forwards are needed. Compose's port 18080 is not used here.

## 6. Health, six WRR requests, concurrent least-connections, and stats

Keep the port-forward running. Use another terminal at the repository root.
Check the external proxy health and each backend's fast health **through its
Service DNS from the proxy pod**, without exposing additional host ports:

```sh
curl --fail --silent --show-error --connect-timeout 2 --max-time 5 http://127.0.0.1:18081/health
timeout 15s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo exec deployment/proxy -- curl --fail --silent --show-error --connect-timeout 2 --max-time 5 http://backend1:8080/health
timeout 15s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo exec deployment/proxy -- curl --fail --silent --show-error --connect-timeout 2 --max-time 5 http://backend2:8080/health
timeout 15s kubectl --context=reverseproxy-demo --namespace=reverseproxy-demo exec deployment/proxy -- curl --fail --silent --show-error --connect-timeout 2 --max-time 5 http://backend3:8080/health
curl --fail --silent --show-error --connect-timeout 2 --max-time 5 http://127.0.0.1:18081/stats
```

Expected health bodies: `loadbalancerReverse is UP` and `SERVER-1/2/3 is UP`.
`/stats` returns the existing HTML page, not JSON; it is also viewable at
http://127.0.0.1:18081/stats. No UI changes are made.

### Six sequential weighted-round-robin requests

Each backend deliberately sleeps **5 seconds** for `/hello`, so this normally
takes about 30 seconds. The client timeout is 95 seconds per request, just above
the proxy's 90-second upstream timeout. The outer deadline is 600 seconds.
This Python standard-library test sends exactly six requests and verifies the
existing **3:2:1** weights and backend port. Run without other `/hello` traffic
or proxy restarts; probes and stats do not advance WRR.

```sh
timeout 600s python3 - <<'PY'
from collections import Counter
from urllib.request import urlopen

counts = Counter()
for number in range(1, 7):
    with urlopen("http://127.0.0.1:18081/hello?algorithm=wrr", timeout=95) as response:
        body = response.read().decode().strip()
    print(number, body, flush=True)
    assert body.endswith("running on port 8080"), body
    counts[body.split()[2]] += 1
print("WRR counts:", dict(counts), flush=True)
assert counts == {"SERVER-1": 3, "SERVER-2": 2, "SERVER-3": 1}, counts
PY
```

On a fresh proxy the order is SERVER-1 three times, SERVER-2 twice, SERVER-3
once. Any uninterrupted six-request WRR cycle still has the same counts,
regardless of its starting position. A connection error, HTTP error, or failed
assertion makes this command fail.

### Six concurrent least-connections requests, with stats during/after

Sequential least-connections calls would normally all select backend1 because
idle ties favor the first backend. The test below releases six workers together
so the 5-second sleeps overlap; it samples stats while they are in flight and
checks all six replies. No WRR weights are applied to `algorithm=least`.

```sh
timeout 120s python3 - <<'PY'
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from threading import Barrier
from time import sleep
from urllib.request import urlopen

start = Barrier(7, timeout=10)

def get(path, seconds):
    with urlopen("http://127.0.0.1:18081" + path, timeout=seconds) as response:
        return response.read().decode().strip()

def request():
    start.wait()
    return get("/hello?algorithm=least", 95)

with ThreadPoolExecutor(max_workers=6) as pool:
    pending = [pool.submit(request) for number in range(6)]
    start.wait()
    sleep(1)
    print("Health during requests:", get("/health", 5), flush=True)
    print("Stats during requests (HTML):", get("/stats", 5), flush=True)
    replies = [future.result(timeout=100) for future in pending]

counts = Counter()
for body in replies:
    print(body, flush=True)
    assert body.endswith("running on port 8080"), body
    counts[body.split()[2]] += 1
assert sum(counts.values()) == 6, counts
assert set(counts) <= {"SERVER-1", "SERVER-2", "SERVER-3"}, counts
print("Least-connections counts:", dict(counts), flush=True)
print("Stats after requests (HTML):", get("/stats", 5), flush=True)
PY
```

With all backends initially idle and the requests overlapping, expect roughly
2:2:2, not 3:2:1. Scheduling, other traffic, and response timing can affect the
observed counts; this is not an exact-distribution guarantee. In-flight stats
should show active connections when sampled early enough; after completion they
should return to zero. If the proxy was fresh and only these tests ran, total
requests should be 12 and backend totals 5, 4, 3 for the usual 2:2:2 least run.
Stats count attempted forwards, not only successful responses, and reset when
the proxy restarts. Inspect routing/reservations in bounded logs if needed:

```sh
timeout 20s kubectl --context=reverseproxy-demo --request-timeout=10s --namespace=reverseproxy-demo logs deployment/proxy --tail=100
```

## 7. Scoped cleanup

Stop the port-forward with Ctrl-C first. Remove **only this demo namespace**;
this deletes all objects in it, including any extra objects you placed there:

```sh
timeout 150s kubectl --context=reverseproxy-demo --request-timeout=130s delete namespace reverseproxy-demo --ignore-not-found=true --wait=true --timeout=120s
```

Optionally stop the dedicated profile while retaining its local images:

```sh
timeout 180s minikube stop --profile=reverseproxy-demo
```

Or permanently remove **only this profile**, including its local images and
cluster data (namespace deletion is optional if deleting the entire profile):

```sh
timeout 300s minikube delete --profile=reverseproxy-demo
```

Do not use `minikube delete --all`, unscoped Kubernetes deletion, Docker prune,
or Compose teardown. Other profiles, namespaces, host images, and Compose on
18080 are outside this workflow.

## Offline validation and validation status

If PyYAML is already installed, this bounded command parses every YAML document
without contacting a cluster (it is **not Kubernetes schema validation**):

```sh
timeout 10s python3 - <<'PY'
from pathlib import Path
import yaml

for path in sorted(Path("k8s").glob("*.yaml")):
    documents = list(yaml.safe_load_all(path.read_text()))
    assert all(isinstance(document, dict) for document in documents), path
    print(path, len(documents), "YAML document(s)")
PY
```

At authoring time, PyYAML parsing and offline assertions covering resource
structure, selectors, image tags/pull policy, environment, probes, resource
budgets, and runtime security passed for the nine objects in five YAML files.
This does not substitute for API-server validation or runtime tests.
**No cluster run, image build, rollout, or HTTP smoke test was performed**:
`kubectl` and `minikube` were absent, and `docker info` was denied access to the
Docker socket (privileged access/access setup would be required). No tools were
installed and no sudo commands were run. Run sections 1–6 yourself to validate
on a live cluster; `kubectl apply` then performs API-server validation.
