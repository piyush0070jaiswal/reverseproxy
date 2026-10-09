# Load Balancer Demo — 3 Spring Boot Servers

Three identical, minimal Spring Boot apps. Each exposes `/hello` and `/health`,
and only differs by its port and the text it returns — so you can clearly see
which server answered a request when you put them behind a load balancer.

| App     | Port |
|---------|------|
| server1 | 8081 |
| server2 | 8082 |
| server3 | 8083 |

## 1. Run each server

You need Java 17+ and Maven installed. Open 3 terminals:

```bash
cd server1 && mvn spring-boot:run
```
```bash
cd server2 && mvn spring-boot:run
```
```bash
cd server3 && mvn spring-boot:run
```

Test each one directly:

```bash
curl http://localhost:8081/hello
curl http://localhost:8082/hello
curl http://localhost:8083/hello
```

Each should return a different message identifying itself (SERVER-1, SERVER-2, SERVER-3).

## 2. Put them behind a load balancer

### Option A — Nginx (easiest for testing)
A ready-made config is included: `nginx-loadbalancer.conf`.

```bash
sudo cp nginx-loadbalancer.conf /etc/nginx/conf.d/loadbalancer.conf
sudo nginx -s reload
```

Now hit the load balancer instead of the servers directly:

```bash
curl http://localhost:8080/hello
curl http://localhost:8080/hello
curl http://localhost:8080/hello
```

You should see the response rotate between SERVER-1, SERVER-2, and SERVER-3
(round robin by default).

### Option B — Build your own load balancer in Spring Boot
If the goal of the project is to *write* the load balancer yourself (not just
use Nginx), you can build a 4th Spring Boot app that:
1. Keeps a list of backend URLs (`localhost:8081`, `8082`, `8083`).
2. On each incoming request, picks the next backend (round robin — just keep
   an `AtomicInteger` counter and mod it by the list size).
3. Forwards the request using `RestTemplate` or `WebClient` and returns the
   response to the caller.

Say the word and I can scaffold that 4th "load-balancer" Spring Boot service
too, with round-robin or least-connections logic.

## Project structure

```
loadbalancer-demo/
├── server1/   (port 8081)
├── server2/   (port 8082)
├── server3/   (port 8083)
├── nginx-loadbalancer.conf
└── README.md
```
