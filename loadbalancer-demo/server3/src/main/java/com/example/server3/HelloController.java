package com.example.server3;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
public class HelloController {

    @Value("${server.port}")
    private String port;

    private static final String SERVER_NAME = "SERVER-3";

    @GetMapping("/hello")
    public String hello() throws InterruptedException {
        Thread.sleep(5000);
        return "Hello from " + SERVER_NAME + " running on port " + port;
    }

    @GetMapping("/health")
    public String health() {
        return SERVER_NAME + " is UP";
    }
}
