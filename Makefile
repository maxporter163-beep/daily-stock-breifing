
CC = gcc

CFLAGS = -std=gnu11 -Wall -Wextra -O2

# libcurl fetches market data; libcjson parses the JSON responses.
DEPS = libcurl libcjson

LIBS = $(shell pkg-config --libs $(DEPS))

CFLAGS += $(shell pkg-config --cflags $(DEPS))

TARGET = stock-briefing

SRCS = src/main.c src/watchlist.c src/http.c src/quotes.c src/report.c
HDRS = src/watchlist.h src/http.h src/quotes.h src/report.h

PACK = output/research-input.md

all: check-deps $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

# Fail early with a helpful message if a library is missing.
check-deps:
	@pkg-config --exists $(DEPS) || { \
	  echo "Missing build dependencies. On Ubuntu/Debian run:"; \
	  echo "  sudo apt-get install -y build-essential pkg-config libcurl4-openssl-dev libcjson-dev"; \
	  echo "On macOS run: brew install curl cjson pkg-config"; \
	  exit 1; }

# Fetch live data and write the research input pack.
run: $(TARGET)
	@mkdir -p output
	./$(TARGET) --output $(PACK)

# Offline smoke test: no network needed; checks the pack's structure.
test: $(TARGET)
	@mkdir -p output
	./$(TARGET) --offline --output output/test-pack.md > /dev/null
	@for section in "## 1. Market snapshot" "## 2. Watchlist snapshot" \
	                "## 3. Data quality notes" "## 4. Research brief" \
	                "ADDITIONAL STOCK DISCOVERY"; do \
	  grep -q "$$section" output/test-pack.md || \
	    { echo "FAIL: missing '$$section'"; exit 1; }; \
	done
	@rows=$$(grep -c '| SKIPPED |' output/test-pack.md); \
	 expected=$$(grep -cv '^[[:space:]]*\(#\|$$\)' config/stocks.txt config/market.txt | \
	   awk -F: '{s+=$$2} END {print s}'); \
	 [ "$$rows" -eq "$$expected" ] || \
	   { echo "FAIL: $$rows table rows, expected $$expected"; exit 1; }
	@./$(TARGET) --stocks /nonexistent 2>/dev/null; \
	 [ $$? -eq 1 ] || { echo "FAIL: missing watchlist should exit 1"; exit 1; }
	@echo "PASS: offline research pack is well-formed"

clean:
	rm -f $(TARGET)
	rm -rf output

.PHONY: all check-deps run test clean
