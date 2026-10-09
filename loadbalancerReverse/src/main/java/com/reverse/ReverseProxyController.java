package com.reverse;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.reactive.function.client.WebClient;

import java.util.concurrent.atomic.AtomicInteger;

@RestController
public class ReverseProxyController {

        private final WebClient webClient = WebClient.create();

        private final String[] servers = {
            "http://localhost:8081",
            "http://localhost:8082",
            "http://localhost:8083"
        };

        // Weights used for Weighted Round Robin
        private final int[] weights = {
            3, 2, 1
        };

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

        if (algorithm.equalsIgnoreCase("least")) {
            selectedServer = selectLeastConnectionServer();
        } else {
            selectedServer = selectWeightedServer();
        }

        // Increase request count
        int serverRequestNumber =
                serverRequests[selectedServer].incrementAndGet();

        // Increase active connection count
        int active =
                activeConnections[selectedServer].incrementAndGet();

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

        weightedIndex++;

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
                                SERVER-1 (8081)
                            </span>

                            <span>
                                Requests: %d |
                                Active: %d
                            </span>
                        </div>

                        <div class="server">
                            <span class="server-name">
                                SERVER-2 (8082)
                            </span>

                            <span>
                                Requests: %d |
                                Active: %d
                            </span>
                        </div>

                        <div class="server">
                            <span class="server-name">
                                SERVER-3 (8083)
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