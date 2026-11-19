# Daily stock briefing: instructions for Claude Code

You are running unattended, usually as a Claude Code cloud scheduled task
at 08:00 Pacific/Auckland. Nobody is watching, so do not ask questions:
make sensible decisions, note them in the briefing, and finish the whole
procedure.

## Step 1: Build the C program

From the repository root:

```bash
make check-deps || bash scripts/cloud-setup.sh
make
make test
```

If the build still fails, skip to step 5 and report the error in the
notification and final message instead of writing a briefing.

## Step 2: Run it and read its output

```bash
mkdir -p output
./stock-briefing --output output/research-input.md
```

Read `output/research-input.md` in full. It contains:

1. **Market snapshot**: indices, NZD/USD and commodities.
2. **Watchlist snapshot**: delayed prices, day and 1-month moves and the
   52-week range for every tracked stock. The watchlist is
   `config/stocks.txt`, so do not add or drop stocks.
3. **Data quality notes**: any symbol marked STALE, UNAVAILABLE or SKIPPED
   needs its price found by web search.
4. **Research brief**: the content, rules and format of the briefing.
   Follow it exactly.

If every quote is UNAVAILABLE, the cloud environment is probably blocking
`query1.finance.yahoo.com`. Carry on with web search and add one line to
the briefing saying the price snapshot was unavailable.

## Step 3: Research

Use web search (and web fetch where useful) to cover everything the
research brief asks for, including the five additional stocks to watch.
Run independent searches in parallel. Prefer primary and reputable
sources: NZX announcements, company releases, the RBNZ, Reuters, the NZ
Herald / BusinessDesk, MarketScreener, broker notes that are quoted
publicly. Never invent analyst ratings, counts or price targets. If you
cannot find them, say so.

## Step 4: Write the briefing

Write it in Markdown to `briefings/YYYY-MM-DD.md`, using today's date in
New Zealand (shown at the top of the research pack). Keep source links
inline.

Then save it to the repository's briefing history. Only add the briefing
file:

```bash
cp briefings/YYYY-MM-DD.md /tmp/briefing.md
git fetch origin claude/briefings 2>/dev/null \
  && git checkout -B claude/briefings origin/claude/briefings \
  || git checkout -B claude/briefings
mkdir -p briefings && cp /tmp/briefing.md briefings/YYYY-MM-DD.md
git add briefings/YYYY-MM-DD.md
git commit -m "Daily stock briefing YYYY-MM-DD"
git push origin claude/briefings
```

Do not push to `main`. If the push fails, mention it in the final message
and continue.

## Step 5: Notify

Send a push notification to the Claude mobile app with the
`PushNotification` tool. Keep it short:

- Title/first line: `Stock briefing ready - <NZ date>`
- Body: the single most important development, in under 150 characters.

If the build failed, the notification should say
`Stock briefing FAILED - <short reason>`.

If the `PushNotification` tool is not available in this session, do not
use any other notification service. Say so at the top of your final
message.

## Step 6: Final message

End the session by printing the complete briefing as your final message,
so it can be read directly in the Claude app. After it, add a short "Run
notes" list: quote coverage (from the pack header), whether the push
notification was sent, and whether the briefing branch was pushed.
