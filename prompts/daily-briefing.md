# Daily stock briefing: instructions for Claude Code

You are running unattended, usually as a Claude Code cloud scheduled task
at 08:05 Pacific/Auckland. Nobody is watching, so do not ask questions:
make sensible decisions, note them in the briefing, and finish the whole
procedure. The reader will read your final message on a phone.

## Step 1: Build the C program

From the repository root:

```bash
make check-deps || bash scripts/cloud-setup.sh
make
make test
```

If the build still fails, skip to step 6 and report the error in the
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
`query1.finance.yahoo.com`. Carry on, find prices by web search, and give
each price's date.

## Step 3: Research thoroughly

This is the most important step. Do not cut it short. A thin briefing is
a failed run. Use WebSearch, and use WebFetch to open the most useful
results; search snippets alone are not enough. Run independent searches in
parallel.

Minimum coverage:

- **Market** (at least 3 searches): the latest NZX 50 close and drivers;
  the latest US session (S&P 500, Nasdaq); RBNZ/OCR and NZ macro news;
  plus commodities or FX if they moved notably in the snapshot.
- **Every watchlist stock** (at least 2 searches each):
  1. latest company news and announcements (NZX/ASX announcements,
     results, guidance, deals);
  2. analyst view: Buy/Hold/Sell counts, consensus rating and target,
     recent upgrades/downgrades and target changes with firm and date.
     Try MarketScreener, the company's investor page, broker commentary
     reported by the NZ Herald/BusinessDesk/NBR/Reuters, and FNArena or
     Simply Wall St for ASX/NZX consensus.
  For ETFs (IVW, NPF, ETPMAG.AX), search for the drivers of the
  underlying assets instead of analyst ratings.
- **Stock discovery** (at least 4 searches): NZX broker upgrades, NZX
  stocks with strong recent momentum or new contracts, recent capital
  raisings or IPOs, and notable ASX/US names with NZ relevance. Then
  research each of the five chosen stocks at least once.
- **Upcoming catalysts**: check for results dates, AGMs, dividend dates and
  regulatory decisions in the next four weeks.

Prefer primary and reputable sources: NZX announcements, company releases,
the RBNZ, Reuters, the NZ Herald, BusinessDesk, NBR, Bloomberg and
MarketScreener. Every figure needs a date. If the newest figure you can
find is more than two weeks old, say how old it is.

Never invent analyst ratings, counts or price targets. If you cannot find
them after searching, write "not found" rather than guessing.

Before writing, check that every watchlist stock has at least one source
dated within the last 30 days, or an explicit note that nothing recent was
found. Search again for any stock that falls short.

## Step 4: Write the briefing

Follow the format in the research brief. Write it in Markdown, with inline
source links, and save it to `output/briefing.md` in the session.

Do not commit or push anything to git. The briefing is delivered through
the session's final message, not the repository.

## Step 5: Notify

Send a push notification to the Claude mobile app with the
`PushNotification` tool. Keep it short:

- First line: `Stock briefing ready - <NZ date>`
- Then the single most important development, in under 150 characters.

If the build failed, the notification should say
`Stock briefing FAILED - <short reason>`.

If the `PushNotification` tool is not available in this session, do not
use any other notification service. Say so at the top of your final
message.

## Step 6: Final message

End the session by printing the complete briefing as your final message,
so it can be read directly in the Claude app. After it, add a short "Run
notes" list:

- quote coverage (from the pack header);
- roughly how many searches and page fetches you ran;
- any stock where recent information could not be found;
- whether the push notification was sent.
