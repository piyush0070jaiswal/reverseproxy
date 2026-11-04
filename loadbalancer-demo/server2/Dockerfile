FROM maven:3.9-eclipse-temurin-25 AS build
WORKDIR /build
COPY pom.xml ./
COPY src ./src
RUN mvn --batch-mode --no-transfer-progress package

FROM eclipse-temurin:25-jre-noble AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 app \
    && useradd --uid 10001 --gid app --no-create-home --home-dir /app --shell /usr/sbin/nologin app
WORKDIR /app
COPY --from=build --chown=10001:10001 /build/target/server2-1.0.0.jar ./app.jar
ENV PORT=8080 \
    SERVER_ADDRESS=0.0.0.0 \
    JAVA_TOOL_OPTIONS="-XX:InitialRAMPercentage=10.0 -XX:MaxRAMPercentage=50.0"
USER 10001:10001
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=5s --start-period=60s --retries=6 \
    CMD curl --fail --silent --show-error --max-time 4 "http://127.0.0.1:${PORT}/health" || exit 1
ENTRYPOINT ["java", "-jar", "/app/app.jar"]
