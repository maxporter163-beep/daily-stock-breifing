# Daily Stock Briefing

A daily NZ-focused stock research briefing. A C program gathers the
market data, and Claude Code does the research and writing.

- **No Anthropic API key and no Twilio.** The C program makes no AI or
  SMS calls.
- **Claude Code does the research.** It runs as a Claude Code cloud
  scheduled task on your Claude subscription.
- **Notification** is sent to the Claude mobile app with Claude Code's
  `PushNotification` tool.

## How it works

```
08:00 Pacific/Auckland - Claude Code cloud scheduled task starts
        |
        |  clones this repo, reads prompts/daily-briefing.md
        v
  make && make test                       (compile the C program)
        |
        v
  ./stock-briefing --output output/research-input.md
        |   reads config/stocks.txt + config/market.txt
        |   fetches delayed prices (Yahoo Finance, no key needed)
        |   prints a Markdown "research input pack":
        |     1. market snapshot   2. watchlist snapshot
        |     3. data-quality notes   4. research brief (the instructions)
        v
  Claude reads the pack, researches with web search, and writes
  briefings/YYYY-MM-DD.md   -> pushed to branch claude/briefings
        |
        v
  PushNotification -> Claude mobile app;
  the full briefing is the session's final message
```

## Project layout

| Path | Purpose |
|---|---|
| `src/main.c` | Orchestration: arguments, load lists, fetch quotes, write the pack |
| `src/watchlist.c/.h` | `read_stocks()`: parses the config files |
| `src/http.c/.h` | `Response` buffer, `write_callback` and `http_get()` (libcurl) |
| `src/quotes.c/.h` | `fetch_quote()`: Yahoo Finance chart API, parsed with cJSON |
| `src/report.c/.h` | Writes the research input pack, including the research brief |
| `config/stocks.txt` | **Your watchlist.** One Yahoo symbol per line, plus an optional `# note` |
| `config/market.txt` | Indices, FX and commodities for the market overview |
| `prompts/daily-briefing.md` | The procedure Claude Code follows each morning |
| `scripts/cloud-setup.sh` | Installs the build dependencies on Ubuntu/Debian |
| `.github/workflows/daily-briefing.yml` | Build-and-test check on every push (no secrets) |

### Editing the watchlist

Use Yahoo Finance symbols so the exchange is unambiguous:

```
FPH.NZ      # NZX listing
SMI.AX      # ASX listing
RKLB        # US listing (no suffix)
```

To change what the briefing covers, edit `config/stocks.txt`. To change
the content or format of the briefing, edit the research brief in
`src/report.c` (`print_research_brief`).

## Running it manually

### 1. Install the build dependencies

```bash
bash scripts/cloud-setup.sh
```

On macOS, use Homebrew instead:

```bash
brew install curl cjson pkg-config
```

### 2. Build and test

```bash
make
```

```bash
make test
```

`make test` runs offline. It checks that the pack has every section, has
one table row per configured symbol, and that a missing watchlist exits
with code 1.

### 3. Generate the research input pack

```bash
make run
```

This writes `output/research-input.md` and prints it to the terminal.
Other options:

```bash
./stock-briefing --help
```

```bash
./stock-briefing --offline
```

The program prints progress to stderr and the pack to stdout. It exits
with code 0 if the pack was written, even when some quotes failed. Those
symbols are marked `STALE` or `UNAVAILABLE` in the pack, and Claude looks
them up by web search instead.

### 4. Produce a briefing with Claude Code

In Claude Code, opened in this repository, ask:

> Follow prompts/daily-briefing.md

Run locally, it will also try to push to `claude/briefings` and send a
push notification. Tell it to skip those steps if you only want the
briefing.

## Setting up the daily scheduled task

The task runs in Anthropic's cloud, so your computer does not need to be
on. You need a Claude Pro, Max, Team or Enterprise plan, with this
repository pushed to GitHub and Claude connected to your GitHub account.
Runs use your subscription's normal usage.

### 1. Cloud environment

Open https://claude.ai/code/routines, then **New task** > **Advanced
settings** > **Cloud environment** > settings icon. Then:

- **Network access:** choose *Limited* and keep *Allow package managers*
  ticked.
- **Allowed domains:** add `query1.finance.yahoo.com`. It is not on the
  default allowlist. Without it, every quote shows `UNAVAILABLE` and the
  briefing relies on web search only.
- **Setup script:** paste the following. It is self-contained because it
  may run before the repository is cloned. If it is missing, the
  briefing procedure runs `scripts/cloud-setup.sh` itself.

  ```bash
  apt-get update -y && apt-get install -y --no-install-recommends build-essential pkg-config libcurl4-openssl-dev libcjson-dev ca-certificates
  ```

- No environment variables or secrets are needed.

### 2. The task itself

- **Repository:** this repository.
- **Schedule:** custom cron `CRON_TZ=Pacific/Auckland 5 8 * * *`. That is
  08:05 NZ time and follows daylight saving. Runs set exactly on the hour
  can start a few minutes late. After saving, check that the dialog shows
  Auckland time.
- **Prompt:**

  > Follow the instructions in prompts/daily-briefing.md exactly. This is
  > an unattended daily run, so do not ask questions.

You can also create the task by running `/schedule` in the Claude Code CLI.

### 3. Test it before relying on it

Press **Run now** on the task. Then check:

1. The run built the program and the pack shows quotes as `OK` (not all
   `UNAVAILABLE`).
2. A briefing appeared in the session, and `briefings/<date>.md` is on
   the `claude/briefings` branch.
3. Your phone received the push notification. Enable notifications for
   the Claude app on your phone. The docs list `PushNotification` for
   cloud sessions, but its delivery from scheduled runs has to be
   confirmed by this test.

Past runs are listed under the task's **History**.

## Limitations

- Prices come from Yahoo Finance's free, unofficial endpoint. It is
  delayed and may change without notice. Some symbols return stale data:
  `ETPMAG.AX` currently returns a 2019 price. The program flags these so
  Claude can verify them.
- The briefing is research, not personal financial advice.
