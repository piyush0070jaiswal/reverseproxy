package com.reverse;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.reactive.function.client.WebClient;
import org.springframework.web.reactive.function.client.WebClientException;
import org.springframework.web.server.ResponseStatusException;

import java.net.URI;
import java.time.Duration;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

@RestController
public class ReverseProxyController {

    private final WebClient webClient = WebClient.create();
    private final String[] servers;
    private final Duration requestTimeout;

    // Weights used for Weighted Round Robin
    private final int[] weights = {3, 2, 1};

    public ReverseProxyController(
            @Value("${loadbalancer.backend-1-url}") String backend1Url,
            @Value("${loadbalancer.backend-2-url}") String backend2Url,
            @Value("${loadbalancer.backend-3-url}") String backend3Url,
            @Value("${loadbalancer.request-timeout-ms}") long requestTimeoutMillis) {
        if (requestTimeoutMillis <= 0) {
            throw new IllegalArgumentException("Upstream timeout must be greater than zero");
        }
        servers = new String[]{
                validateBackendUrl(backend1Url),
                validateBackendUrl(backend2Url),
                validateBackendUrl(backend3Url)
        };
        requestTimeout = Duration.ofMillis(requestTimeoutMillis);
    }

    private static String validateBackendUrl(String value) {
        String url = value.trim();
        URI uri = URI.create(url);
        if (!("http".equalsIgnoreCase(uri.getScheme()) ||
                "https".equalsIgnoreCase(uri.getScheme())) ||
                uri.getHost() == null || uri.getUserInfo() != null ||
                uri.getQuery() != null || uri.getFragment() != null) {
            throw new IllegalArgumentException("Backend URLs must be HTTP(S) base URLs without credentials, queries or fragments");
        }
        return url.replaceFirst("/+$", "");
    }

    // Total requests received by reverse proxy
    private final AtomicInteger totalRequests = new AtomicInteger(0);

    // Total requests forwarded to each server
    private final AtomicInteger[] serverRequests = {
            new AtomicInteger(0),
            new AtomicInteger(0),
            new AtomicInteger(0)
    };

    // Current active connections for each server
    private final AtomicInteger[] activeConnections = {
            new AtomicInteger(0),
            new AtomicInteger(0),
            new AtomicInteger(0)
    };

    // Used by Weighted Round Robin
    private int weightedIndex = 0;


    // =========================================================
    // MAIN LOAD BALANCER ENDPOINT
    // =========================================================

    @GetMapping("/hello")
    public String hello(
            @RequestParam(defaultValue = "wrr") String algorithm) {

        int requestNumber = totalRequests.incrementAndGet();

        int selectedServer;
        int active;
        // Reserve the selected backend before another request makes its selection.
        synchronized (this) {
            if (algorithm.equalsIgnoreCase("least")) {
                selectedServer = selectLeastConnectionServer();
            } else {
                selectedServer = selectWeightedServer();
            }
            active = activeConnections[selectedServer].incrementAndGet();
        }

        int serverRequestNumber = serverRequests[selectedServer].incrementAndGet();

        String server = servers[selectedServer];

        System.out.println(
                "Request #" + requestNumber +
                        " -> " + server +
                        " | Algorithm: " + algorithm +
                        " | Total Server Requests: " + serverRequestNumber +
                        " | Active Connections: " + active
        );

        try {
            return webClient
                    .get()
                    .uri(server + "/hello")
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(requestTimeout)
                    .onErrorMap(TimeoutException.class, error -> new ResponseStatusException(
                            HttpStatus.GATEWAY_TIMEOUT, "Backend request timed out", error))
                    .onErrorMap(WebClientException.class, error -> new ResponseStatusException(
                            HttpStatus.BAD_GATEWAY, "Backend request failed", error))
                    .block();
        } finally {
            int remaining = activeConnections[selectedServer].decrementAndGet();
            System.out.println(
                    "Request #" + requestNumber +
                            " completed on " + server +
                            " | Active Connections: " + remaining
            );
        }
    }


    // =========================================================
    // WEIGHTED ROUND ROBIN
    // =========================================================

    private synchronized int selectWeightedServer() {

        int totalWeight = 0;

        for (int weight : weights) {
            totalWeight += weight;
        }

        int position = weightedIndex % totalWeight;

        int selectedServer = 0;

        for (int i = 0; i < servers.length; i++) {

            if (position < weights[i]) {
                selectedServer = i;
                break;
            }

            position -= weights[i];
        }

        weightedIndex = (weightedIndex + 1) % totalWeight;

        return selectedServer;
    }

    // =========================================================
    // LEAST CONNECTIONS
    // =========================================================

    private int selectLeastConnectionServer() {

        int selectedServer = 0;

        int minimumConnections =
                activeConnections[0].get();

        for (int i = 1; i < activeConnections.length; i++) {

            int currentConnections =
                    activeConnections[i].get();

            if (currentConnections < minimumConnections) {

                minimumConnections = currentConnections;
                selectedServer = i;
            }
        }

        return selectedServer;
    }


    // =========================================================
    // STATISTICS
    // =========================================================

    @GetMapping("/health")
    public String health() {
        return "loadbalancerReverse is UP";
    }

    @GetMapping("/stats")
    public String stats() {

        return """
                <html>
                <head>
                    <title>Load Balancer Statistics</title>

                    <style>

                        body {
                            font-family: Arial, sans-serif;
                            background-color: #f4f6f8;
                            padding: 40px;
                        }

                        .container {
                            width: 600px;
                            margin: auto;
                            background: white;
                            padding: 30px;
                            border-radius: 10px;
                            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
                        }

                        h1 {
                            text-align: center;
                            margin-bottom: 30px;
                        }

                        .total {
                            text-align: center;
                            font-size: 24px;
                            font-weight: bold;
                            margin-bottom: 25px;
                        }

                        .server {
                            display: flex;
                            justify-content: space-between;
                            padding: 18px;
                            margin: 12px 0;
                            background-color: #f1f3f5;
                            border-radius: 6px;
                            font-size: 18px;
                        }

                        .server-name {
                            font-weight: bold;
                        }

                    </style>

                </head>

                <body>

                    <div class="container">

                        <h1>Load Balancer Statistics</h1>

                        <div class="total">
                            Total Requests: %d
                        </div>

                        <div class="server">
                            <span class="server-name">
                                SERVER-1
                            </span>

                            <span>
                                Requests: %d |
                                Active: %d
                            </span>
                        </div>

                        <div class="server">
                            <span class="server-name">
                                SERVER-2
                            </span>

                            <span>
                                Requests: %d |
                                Active: %d
                            </span>
                        </div>

                        <div class="server">
                            <span class="server-name">
                                SERVER-3
                            </span>

                            <span>
                                Requests: %d |
                                Active: %d
                            </span>
                        </div>

                    </div>

                </body>

                </html>
                """.formatted(

                totalRequests.get(),

                serverRequests[0].get(),
                activeConnections[0].get(),

                serverRequests[1].get(),
                activeConnections[1].get(),

                serverRequests[2].get(),
                activeConnections[2].get()
        );
    }
}