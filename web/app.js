/*
 * Trading Arena - browser side, built with Vue 3 (no build step).
 *
 * This file has NO game rules. Every action is sent to the C server
 * (src/server.c), which answers with the whole game state as JSON.
 * Vue then redraws whatever changed.
 *
 * How it fits together:
 *   - `store` holds the latest game state from C plus a few page settings.
 *   - Each component below matches one <script type="text/x-template"> in index.html.
 *   - Money arrives in cents (5000 = $50.00), the same as in the C code.
 */
"use strict";

const { createApp, reactive, ref, computed, watch, onMounted, onBeforeUnmount, nextTick } = Vue;

/* ---------- Small helpers (used in every template) ---------- */

const MONEY = new Intl.NumberFormat("en-US", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
const COUNT = new Intl.NumberFormat("en-US");
const MAX_ORDER_QTY = 1000000;   /* same limit as portfolio.h */
const MAX_SEED = 2147483647;     /* same limit as server.c */

const money = (cents) => MONEY.format(cents / 100);
const signedMoney = (cents) => (cents > 0 ? "+" : cents < 0 ? "-" : "") + money(Math.abs(cents));
const count = (n) => COUNT.format(n);
/* Basis points -> "+2.35%". Math.trunc matches C's integer division. */
const percent = (bp) => (bp > 0 ? "+" : bp < 0 ? "-" : "") + MONEY.format(Math.abs(bp) / 100) + "%";
const changeBp = (from, to) => (from > 0 ? Math.trunc(((to - from) * 10000) / from) : 0);
const direction = (n) => (n > 0 ? "up" : n < 0 ? "down" : "flat");

const helpers = { money, signedMoney, count, percent, changeBp, direction };

/* Round axis numbers, e.g. 52.00, 56.00, 60.00 instead of 53.21, 57.37 ... */
function niceScale(low, high, wantedTicks) {
  if (high === low) {
    const pad = Math.max(1, Math.round(low * 0.05));
    low -= pad;
    high += pad;
  }
  const roughStep = (high - low) / wantedTicks;
  const magnitude = 10 ** Math.floor(Math.log10(roughStep));
  const ratio = roughStep / magnitude;
  const step = (ratio < 1.5 ? 1 : ratio < 3 ? 2 : ratio < 7 ? 5 : 10) * magnitude;
  const min = Math.floor(low / step) * step;
  const max = Math.ceil(high / step) * step;
  const ticks = [];
  for (let t = min; t <= max + step / 2; t += step) ticks.push(t);
  return { min, max, ticks };
}

/* "M x y L x y ..." for an SVG line through the points. */
const pathFrom = (points) => points.map(([px, py], i) => (i === 0 ? "M" : "L") + px.toFixed(1) + " " + py.toFixed(1)).join(" ");

/* The width of an element, kept up to date when the window changes size. */
function useWidth(elRef, fallback) {
  const width = ref(fallback);
  let observer = null;
  onMounted(() => {
    observer = new ResizeObserver((entries) => {
      width.value = Math.max(280, Math.round(entries[0].contentRect.width));
    });
    watch(elRef, (el) => { if (el) observer.observe(el); }, { immediate: true });
  });
  onBeforeUnmount(() => observer && observer.disconnect());
  return width;
}

/* Your name, remembered in this browser only (it can fail in private mode: that's fine). */
const savedName = () => { try { return localStorage.getItem("arena-name") || ""; } catch (e) { return ""; } };
const rememberName = (name) => { try { localStorage.setItem("arena-name", name); } catch (e) { /* ignore */ } };

/* ---------- The store: the game state from C, plus page settings ---------- */

const store = reactive({
  state: null,       /* the latest game state from the server */
  selected: 0,       /* which stock is selected (0-4) */
  busy: false,       /* true while waiting for the server */
  offline: false,    /* true when the server can't be reached */
  tradeMsg: null,    /* { ok, text } after a trade */
  recap: null,       /* what happened on the day you just finished */
});

async function callServer(path, method = "POST") {
  if (store.busy) return null;
  store.busy = true;
  try {
    const response = await fetch(path, { method });
    if (!response.ok) throw new Error("HTTP " + response.status);
    const answer = await response.json();
    store.offline = false;
    store.state = answer.state;
    return answer;
  } catch (error) {
    store.offline = true;
    return null;
  } finally {
    store.busy = false;
  }
}

/* Every component gets the helpers plus `s` (the game state) and `store`. */
const component = (id, extra = {}) => ({
  template: "#" + id,
  ...extra,
  setup(props, ctx) {
    const own = extra.setup ? extra.setup(props, ctx) : {};
    return { store, s: computed(() => store.state), ...helpers, ...own };
  },
});

/* ---------- Small pieces ---------- */

/* A "+2.35%" text that is green/red AND has a sign, so color is never the only clue. */
const ChangeText = {
  template: "#change-text-tpl",
  props: { bp: { type: Number, required: true }, suffix: { type: String, default: "" } },
  setup(props) {
    return {
      dir: computed(() => direction(props.bp)),
      text: computed(() => percent(props.bp) + props.suffix),
    };
  },
};

const DayRecap = component("day-recap-tpl", { props: ["recap"], emits: ["close"] });

/* A tiny line with no axes, for the stock list. */
const Sparkline = {
  template: "#sparkline-tpl",
  props: { history: { type: Array, required: true } },
  setup(props) {
    const path = computed(() => {
      const h = props.history, width = 96, height = 28, pad = 3;
      const low = Math.min(...h), high = Math.max(...h);
      const last = Math.max(1, h.length - 1);
      return pathFrom(h.map((p, day) => [
        pad + (day / last) * (width - 2 * pad),
        high === low ? height / 2 : pad + ((high - p) / (high - low)) * (height - 2 * pad),
      ]));
    });
    return { path };
  },
};

/* ---------- Page sections ---------- */

const selectStock = (i) => { store.selected = i; store.tradeMsg = null; };

const Standings = component("standings-tpl");

const StockList = component("stock-list-tpl", {
  setup() {
    const dayBp = (stock) => {
      const h = stock.history;
      return h.length >= 2 ? changeBp(h[h.length - 2], stock.price) : 0;
    };
    return { select: selectStock, dayBp };
  },
});

const PriceChart = component("price-chart-tpl", {
  setup() {
    const wrap = ref(null);
    const svg = ref(null);
    const width = useWidth(wrap, 640);
    const height = 280;
    const m = { top: 12, right: 12, bottom: 28, left: 64 };
    const hover = ref(null);

    const stock = computed(() => store.state.stocks[store.selected]);
    const history = computed(() => stock.value.history);
    const numDays = computed(() => store.state.numDays);
    /* The market index, scaled to start at this stock's day-0 price, so you can
       see whether the stock did better or worse than the whole market. */
    const marketLine = computed(() =>
      store.state.indexHistory.map((v) => (history.value[0] * v) / store.state.indexStart));
    const scale = computed(() => niceScale(
      Math.min(...history.value, ...marketLine.value), Math.max(...history.value, ...marketLine.value), 4));

    const plotW = computed(() => width.value - m.left - m.right);
    const plotH = height - m.top - m.bottom;
    const x = (day) => m.left + (day / numDays.value) * plotW.value;
    const y = (price) => m.top + ((scale.value.max - price) / (scale.value.max - scale.value.min)) * plotH;

    const points = computed(() => history.value.map((p, day) => [x(day), y(p)]));
    const linePath = computed(() => pathFrom(points.value));
    const areaPath = computed(() => {
      const pts = points.value, bottom = m.top + plotH;
      return linePath.value + ` L${pts[pts.length - 1][0].toFixed(1)} ${bottom} L${pts[0][0].toFixed(1)} ${bottom} Z`;
    });
    const marketPath = computed(() =>
      marketLine.value.length > 1 ? pathFrom(marketLine.value.map((v, day) => [x(day), y(v)])) : "");
    const dayTicks = computed(() => [0, 5, 10, 15, 20, 25, 30].filter((d) => d <= numDays.value));
    const marks = computed(() => store.state.trades.filter((t) => t.stock === store.selected));
    const sinceStartBp = computed(() => changeBp(history.value[0], stock.value.price));
    const reversedDays = computed(() => history.value.map((p, d) => d).reverse());
    const label = computed(() =>
      `${stock.value.name} price chart, day 0 to day ${history.value.length - 1}. Now ${money(stock.value.price)}.`);

    function onMove(event) {
      const box = svg.value.getBoundingClientRect();
      const mouseX = ((event.clientX - box.left) / box.width) * width.value;
      const day = Math.round(((mouseX - m.left) / plotW.value) * numDays.value);
      hover.value = day >= 0 && day < history.value.length ? day : null;
    }
    /* Keep the tooltip inside the chart: flip it to the left on the right half. */
    const tipStyle = computed(() => {
      if (hover.value === null) return {};
      const left = (x(hover.value) / width.value) * 100;
      const top = Math.max(0, y(history.value[hover.value]) - 70) + "px";
      return left > 60
        ? { right: (100 - left) + "%", top, marginRight: "12px" }
        : { left: left + "%", top, marginLeft: "12px" };
    });

    return { wrap, svg, width, height, m, hover, stock, history, marketLine, scale, x, y, linePath, areaPath,
             marketPath, dayTicks, marks, sinceStartBp, reversedDays, label, onMove, tipStyle };
  },
});

const RaceChart = component("race-chart-tpl", {
  setup() {
    const wrap = ref(null);
    const width = useWidth(wrap, 640);
    const height = 240;
    /* Phones: thinner margins for the name labels, and day labels every 10 days. */
    const narrow = computed(() => width.value < 480);
    const m = computed(() => (narrow.value
      ? { top: 12, right: 84, bottom: 28, left: 64 }
      : { top: 12, right: 112, bottom: 28, left: 72 }));

    const race = computed(() => store.state.race);
    const scale = computed(() => {
      const all = race.value.flatMap((r) => r.worth);
      return niceScale(Math.min(...all), Math.max(...all), 4);
    });
    const plotW = computed(() => width.value - m.value.left - m.value.right);
    const plotH = computed(() => height - m.value.top - m.value.bottom);
    const x = (day) => m.value.left + (day / store.state.numDays) * plotW.value;
    const y = (v) => m.value.top + ((scale.value.max - v) / (scale.value.max - scale.value.min)) * plotH.value;
    const dayTicks = computed(() => (narrow.value ? [0, 10, 20, 30] : [0, 5, 10, 15, 20, 25, 30]));

    /* Bots first, you last, so your line is drawn on top. */
    const lines = computed(() => race.value
      .map((r, i) => ({ r, i }))
      .sort((a, b) => Number(a.r.isYou) - Number(b.r.isYou))
      .map(({ r, i }) => ({
        key: i,
        cls: r.isYou ? "is-you" : "bot-" + i,
        d: pathFrom(r.worth.map((v, day) => [x(day), y(v)])),
      })));

    /* Name each line at its end, nudged apart so labels never overlap. */
    const labels = computed(() => {
      const maxChars = narrow.value ? 9 : 12;
      const list = race.value.map((r, i) => {
        const last = r.worth.length - 1;
        return {
          key: i, isYou: r.isYou, x: x(last), y: y(r.worth[last]),
          text: r.name.length > maxChars ? r.name.slice(0, maxChars - 1) + "..." : r.name,
        };
      }).sort((a, b) => a.y - b.y);
      for (let i = 1; i < list.length; i++) list[i].y = Math.max(list[i].y, list[i - 1].y + 15);
      return list;
    });
    const label = computed(() =>
      `Net worth of all players from day 0 to day ${store.state.day}. Leading: ${store.state.leaderboard[0].name}.`);

    return { wrap, width, height, m, scale, x, y, dayTicks, lines, labels, label };
  },
});

const TradePanel = component("trade-panel-tpl", {
  setup() {
    const side = ref("buy");
    const qty = ref("10");

    const stock = computed(() => store.state.stocks[store.selected]);
    /* Worked out by the C server (portfolio.c), so the fee rule lives in one place. */
    const maxBuy = computed(() => Math.min(MAX_ORDER_QTY, store.state.you.maxAffordable[store.selected]));
    const owned = computed(() => store.state.you.shares[store.selected]);

    const qtyNum = computed(() => {
      if (!/^\d+$/.test(qty.value)) return null;
      const n = Number(qty.value);
      return n >= 1 && n <= MAX_ORDER_QTY ? n : null;
    });
    const value = computed(() => (qtyNum.value === null ? 0 : qtyNum.value * stock.value.price));
    /* For showing an estimate only: the C server charges the real fee.
       Same rule as trade_fee() in portfolio.c: feeBp of the value, rounded up. */
    const fee = computed(() => (value.value > 0 ? Math.ceil((value.value * store.state.feeBp) / 10000) : 0));

    const error = computed(() => {
      if (qty.value === "" || store.state.gameOver) return "";
      if (qtyNum.value === null) return "Shares must be a whole number from 1 to 1,000,000.";
      if (side.value === "buy" && qtyNum.value > maxBuy.value) return "You can afford at most " + count(maxBuy.value) + " (fee included).";
      if (side.value === "sell" && qtyNum.value > owned.value) return "You only own " + count(owned.value) + ".";
      return "";
    });
    const canSubmit = computed(() =>
      !store.busy && !store.state.gameOver && qtyNum.value !== null && error.value === "");

    const chips = computed(() => {
      const part = (n, pct) => Math.floor((n * pct) / 100);
      return side.value === "buy"
        ? [{ label: "10", qty: Math.min(10, maxBuy.value) }, { label: "25%", qty: part(maxBuy.value, 25) },
           { label: "50%", qty: part(maxBuy.value, 50) }, { label: "Max", qty: maxBuy.value }]
        : [{ label: "25%", qty: part(owned.value, 25) }, { label: "50%", qty: part(owned.value, 50) },
           { label: "All", qty: owned.value }];
    });

    function setSide(next) {
      side.value = next;
      store.tradeMsg = null;
      if (next === "sell" && owned.value > 0 && (qtyNum.value === null || qtyNum.value > owned.value)) {
        qty.value = String(owned.value);
      }
    }
    /* Switching stocks: if you own none of the new one, go back to buying. */
    watch(() => store.selected, () => { if (owned.value === 0) side.value = "buy"; });

    async function submit() {
      if (!canSubmit.value) return;
      const answer = await callServer(`/api/${side.value}?stock=${store.selected}&qty=${qtyNum.value}`);
      if (answer) store.tradeMsg = { ok: answer.ok, text: answer.message };
      /* Sold everything: back to buying, with a fresh amount. */
      if (answer && answer.ok && side.value === "sell" && owned.value === 0) {
        side.value = "buy";
        qty.value = "10";
      }
    }

    return { side, qty, stock, maxBuy, owned, qtyNum, value, fee, error, canSubmit, chips, setSide, submit };
  },
});

const AccountPanel = component("account-panel-tpl", {
  setup() {
    const holdings = computed(() => {
      const you = store.state.you;
      return store.state.stocks.map((stock, i) => ({
        index: i, symbol: stock.symbol, shares: you.shares[i], avg: you.avgCost[i],
        value: you.shares[i] * stock.price, pl: you.profitLoss[i],
        plBp: changeBp(you.costBasis[i], you.costBasis[i] + you.profitLoss[i]),
      })).filter((h) => h.shares > 0);
    });
    return { holdings, select: selectStock };
  },
});

const TradesList = component("trades-list-tpl", {
  setup() {
    return { newestFirst: computed(() => store.state.trades.slice().reverse()) };
  },
});

const BotsList = component("bots-list-tpl", {
  setup() {
    function describe(lastTrades) {
      const parts = [];
      lastTrades.forEach((change, i) => {
        if (change > 0) parts.push("Bought " + count(change) + " " + store.state.stocks[i].symbol);
        if (change < 0) parts.push("Sold " + count(-change) + " " + store.state.stocks[i].symbol);
      });
      return parts.length > 0 ? parts.join(", ") : "No trades";
    }
    return { describe };
  },
});

const AllTime = component("all-time-tpl");

/* ---------- Dialogs: start a game, see the results ---------- */

const StartDialog = component("start-dialog-tpl", {
  setup(props, { expose }) {
    const dlg = ref(null);
    const name = ref(savedName());
    const seed = ref("");
    /* You can go back to the current game once it has started (a day passed or a trade made). */
    const resumable = computed(() => !!store.state && (store.state.day > 0 || store.state.trades.length > 0));
    const seedError = computed(() => {
      if (seed.value === "") return "";
      return /^\d+$/.test(seed.value) && Number(seed.value) <= MAX_SEED
        ? "" : "Use a whole number, or leave it empty for a random game.";
    });

    const open = () => { if (!dlg.value.open) dlg.value.showModal(); };
    const close = () => dlg.value.close();
    /* Esc closes it only when there is a game to go back to. */
    const onCancel = (event) => { if (!resumable.value) event.preventDefault(); };

    async function start() {
      if (seedError.value) return;
      const params = new URLSearchParams();
      if (seed.value !== "") params.set("seed", seed.value);
      if (name.value.trim() !== "") params.set("name", name.value.trim());
      rememberName(name.value.trim());
      store.selected = 0;
      store.tradeMsg = null;
      store.recap = null;
      const answer = await callServer("/api/new" + (params.toString() ? "?" + params : ""));
      if (answer) {
        seed.value = "";
        close();
      }
    }
    expose({ open });
    return { dlg, name, seed, resumable, seedError, open, close, onCancel, start };
  },
});

const ResultsDialog = component("results-dialog-tpl", {
  emits: ["new-game"],
  setup(props, { emit, expose }) {
    const dlg = ref(null);
    const rank = computed(() => store.state.leaderboard.findIndex((p) => p.isYou) + 1);
    const title = computed(() => (rank.value === 1 ? "You won" : "You finished " + ["1st", "2nd", "3rd", "4th"][rank.value - 1]));
    const savedNote = computed(() => {
      const s = store.state;
      if (!s.resultSaved) return "";
      return s.yourPlace > 0
        ? "Saved to scores.txt: you placed #" + s.yourPlace + " on the all-time top 10."
        : "Saved to scores.txt, but not in the top 10 this time.";
    });
    /* The lesson numbers come from the C server (display.c), the same as the terminal. */
    const lessonRows = computed(() => {
      const s = store.state, best = s.stocks[s.lesson.bestStock];
      return [
        { label: "Traded like you did", value: s.you.netWorth },
        { label: "Kept all your cash", value: s.startCash },
        { label: "Followed the market index", value: s.lesson.followIndexValue },
        { label: "Bought " + best.symbol + " on day 0 and held", value: s.lesson.holdBestValue },
      ];
    });
    const verdict = computed(() => {
      const s = store.state;
      const gap = changeBp(s.startCash, s.you.netWorth) - changeBp(s.indexStart, s.index);
      const points = MONEY.format(Math.abs(gap) / 100);
      return (gap >= 0
        ? "You beat the market by " + points + " points."
        : "The market beat you by " + points + " points. Most real traders don't beat it either.") +
        " You paid " + money(s.you.feesPaid) + " in fees.";
    });

    const open = () => { if (!dlg.value.open) dlg.value.showModal(); };
    const close = () => dlg.value.close();
    async function replay() {
      close();
      store.selected = 0;
      store.recap = null;
      store.tradeMsg = null;
      const params = new URLSearchParams({ seed: String(store.state.seed), name: store.state.you.name });
      await callServer("/api/new?" + params);
    }
    const newGame = () => { close(); emit("new-game"); };
    expose({ open });
    return { dlg, title, savedNote, lessonRows, verdict, open, close, replay, newGame };
  },
});

/* ---------- The app ---------- */

const App = component("app-tpl", {
  setup() {
    const start = ref(null);
    const results = ref(null);
    const s = computed(() => store.state);

    const dayText = computed(() => (!s.value ? "Day 0 of 30"
      : "Day " + s.value.day + " of " + s.value.numDays + (s.value.gameOver ? ", game over" : "")));
    const meterWidth = computed(() => (s.value ? (100 * s.value.day) / s.value.numDays : 0) + "%");
    const indexBp = computed(() => changeBp(s.value.indexStart, s.value.index));

    const openStart = () => start.value.open();
    const openResults = () => results.value.open();

    /* Next day, then a one-line recap of what happened to you and the market. */
    async function nextDay() {
      if (!s.value || s.value.gameOver || store.busy) return;
      const youBefore = s.value.you.netWorth;
      const indexBefore = s.value.index;
      store.tradeMsg = null;
      const answer = await callServer("/api/next");
      if (!answer) return;
      store.recap = {
        day: s.value.day,
        youBp: changeBp(youBefore, s.value.you.netWorth),
        marketBp: changeBp(indexBefore, s.value.index),
        news: s.value.news,
        event: s.value.marketEvent,
      };
      if (s.value.gameOver) nextTick(openResults);
    }

    const reload = () => callServer("/api/state", "GET");

    /* Keyboard: N = next day, 1-5 = pick a stock (not while typing or in a dialog). */
    function onKey(event) {
      if (event.target.closest("input, dialog") || event.metaKey || event.ctrlKey || event.altKey || !s.value) return;
      if (event.key === "n" || event.key === "N") nextDay();
      else if (event.key >= "1" && event.key <= String(s.value.stocks.length)) selectStock(Number(event.key) - 1);
    }

    onMounted(async () => {
      document.addEventListener("keydown", onKey);
      await reload();
      /* A fresh game nobody has touched yet: start with the welcome screen. */
      if (s.value && s.value.day === 0 && s.value.trades.length === 0) openStart();
      else if (s.value && s.value.gameOver) openResults();
    });
    onBeforeUnmount(() => document.removeEventListener("keydown", onKey));

    return { start, results, dayText, meterWidth, indexBp, openStart, openResults, nextDay, reload };
  },
});

const app = createApp(App);
app.component("change-text", ChangeText);
app.component("day-recap", DayRecap);
app.component("sparkline", Sparkline);
app.component("standings", Standings);
app.component("stock-list", StockList);
app.component("price-chart", PriceChart);
app.component("race-chart", RaceChart);
app.component("trade-panel", TradePanel);
app.component("account-panel", AccountPanel);
app.component("trades-list", TradesList);
app.component("bots-list", BotsList);
app.component("all-time", AllTime);
app.component("start-dialog", StartDialog);
app.component("results-dialog", ResultsDialog);
app.mount("#app");
