package com.reverse;

import com.sun.net.httpserver.HttpServer;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;
import org.springframework.http.HttpStatus;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.web.server.ResponseStatusException;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

import static org.assertj.core.api.Assertions.assertThat;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

class ReverseProxyControllerTest {
    private final List<StubBackend> backends = new ArrayList<>();
    private ExecutorService backendExecutor;
    private ExecutorService clients;

    @BeforeEach
    void setUp() throws IOException {
        backendExecutor = Executors.newCachedThreadPool();
        clients = Executors.newFixedThreadPool(2);
        for (int i = 1; i <= 3; i++) {
            backends.add(new StubBackend("SERVER-" + i));
        }
    }

    @AfterEach
    void tearDown() throws InterruptedException {
        for (StubBackend backend : backends) {
            backend.release.countDown();
            backend.server.stop(0);
        }
        clients.shutdownNow();
        backendExecutor.shutdownNow();
        clients.awaitTermination(5, TimeUnit.SECONDS);
        backendExecutor.awaitTermination(5, TimeUnit.SECONDS);
    }

    @Test
    void weightedRoundRobinPreservesThreeTwoOneWeightsAndWraps() {
        ReverseProxyController controller = controller(5000);
        List<String> responses = new ArrayList<>();
        for (int i = 0; i < 12; i++) {
            responses.add(controller.hello("wrr"));
        }
        assertThat(responses).containsExactly(
                "SERVER-1", "SERVER-1", "SERVER-1", "SERVER-2", "SERVER-2", "SERVER-3",
                "SERVER-1", "SERVER-1", "SERVER-1", "SERVER-2", "SERVER-2", "SERVER-3");
        assertThat(controller.stats()).contains("Total Requests: 12");
        assertNoActiveRequests(controller);
    }

    @Test
    void leastConnectionsUsesBackendsWithoutOutstandingRequests() throws Exception {
        ReverseProxyController controller = controller(10000);
        backends.get(0).release = new CountDownLatch(1);
        backends.get(1).release = new CountDownLatch(1);

        var first = clients.submit(() -> controller.hello("least"));
        assertThat(backends.get(0).entered.await(5, TimeUnit.SECONDS)).isTrue();
        var second = clients.submit(() -> controller.hello("LEAST"));
        assertThat(backends.get(1).entered.await(5, TimeUnit.SECONDS)).isTrue();
        assertThat(controller.hello("least")).isEqualTo("SERVER-3");

        backends.get(0).release.countDown();
        backends.get(1).release.countDown();
        assertThat(first.get(5, TimeUnit.SECONDS)).isEqualTo("SERVER-1");
        assertThat(second.get(5, TimeUnit.SECONDS)).isEqualTo("SERVER-2");
        assertNoActiveRequests(controller);
    }

    @Test
    void backendHttpFailureReturnsBadGatewayAndReleasesReservation() {
        ReverseProxyController controller = controller(5000);
        backends.get(0).responseStatus = 503;
        ResponseStatusException failure = assertThrows(
                ResponseStatusException.class, () -> controller.hello("least"));
        assertThat(failure.getStatusCode()).isEqualTo(HttpStatus.BAD_GATEWAY);
        assertNoActiveRequests(controller);

        backends.get(0).responseStatus = 200;
        assertThat(controller.hello("least")).isEqualTo("SERVER-1");
        assertNoActiveRequests(controller);
    }

    @Test
    void disconnectedBackendReturnsBadGatewayAndReleasesReservation() {
        ReverseProxyController controller = controller(5000);
        backends.get(0).server.stop(0);
        ResponseStatusException failure = assertThrows(
                ResponseStatusException.class, () -> controller.hello("least"));
        assertThat(failure.getStatusCode()).isEqualTo(HttpStatus.BAD_GATEWAY);
        assertNoActiveRequests(controller);
    }

    @Test
    void upstreamTimeoutReturnsGatewayTimeoutAndReleasesReservation() {
        ReverseProxyController controller = controller(250);
        backends.get(0).release = new CountDownLatch(1);
        ResponseStatusException failure = assertThrows(
                ResponseStatusException.class, () -> controller.hello("least"));
        assertThat(failure.getStatusCode()).isEqualTo(HttpStatus.GATEWAY_TIMEOUT);
        assertNoActiveRequests(controller);
    }

    @Test
    void healthIsFastAndDoesNotCallOrCountBackendRequests() throws Exception {
        ReverseProxyController controller = controller(5000);
        MockMvc mvc = MockMvcBuilders.standaloneSetup(controller).build();
        mvc.perform(get("/health"))
                .andExpect(status().isOk())
                .andExpect(content().string("loadbalancerReverse is UP"));
        assertThat(controller.stats()).contains("Total Requests: 0");
        assertThat(backends).allSatisfy(backend -> assertThat(backend.entered.getCount()).isEqualTo(1));
    }

    @Test
    void configuredTrailingSlashAndUnknownAlgorithmKeepExistingFallback() {
        ReverseProxyController controller = new ReverseProxyController(
                backends.get(0).url() + "/", backends.get(1).url(), backends.get(2).url(), 5000);
        assertThat(controller.hello("unknown")).isEqualTo("SERVER-1");
    }

    @ParameterizedTest
    @ValueSource(strings = {"file:///etc/passwd", "http://", "https://user:password@example.com",
            "https://example.com?query=value", "https://example.com#fragment"})
    void invalidBackendConfigurationFailsAtStartup(String invalidUrl) {
        assertThrows(IllegalArgumentException.class, () -> new ReverseProxyController(
                invalidUrl, backends.get(1).url(), backends.get(2).url(), 5000));
    }

    @Test
    void nonPositiveTimeoutFailsAtStartup() {
        assertThrows(IllegalArgumentException.class, () -> controller(0));
        assertThrows(IllegalArgumentException.class, () -> controller(-1));
    }

    private ReverseProxyController controller(long timeoutMillis) {
        return new ReverseProxyController(backends.get(0).url(), backends.get(1).url(),
                backends.get(2).url(), timeoutMillis);
    }

    private static void assertNoActiveRequests(ReverseProxyController controller) {
        List<String> activeCounts = Pattern.compile("Active:\\s*(\\d+)")
                .matcher(controller.stats()).results().map(result -> result.group(1)).toList();
        assertThat(activeCounts).containsExactly("0", "0", "0");
    }

    private class StubBackend {
        private final HttpServer server;
        private final CountDownLatch entered = new CountDownLatch(1);
        private volatile CountDownLatch release = new CountDownLatch(0);
        private volatile int responseStatus = 200;

        private StubBackend(String name) throws IOException {
            server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
            server.setExecutor(backendExecutor);
            server.createContext("/hello", exchange -> {
                entered.countDown();
                try {
                    if (!release.await(10, TimeUnit.SECONDS)) {
                        throw new IOException("Test backend was not released");
                    }
                    byte[] body = name.getBytes(StandardCharsets.UTF_8);
                    exchange.sendResponseHeaders(responseStatus, body.length);
                    exchange.getResponseBody().write(body);
                } catch (InterruptedException exception) {
                    Thread.currentThread().interrupt();
                    throw new IOException("Test backend interrupted", exception);
                } finally {
                    exchange.close();
                }
            });
            server.start();
        }

        private String url() {
            return "http://127.0.0.1:" + server.getAddress().getPort();
        }
    }
}
