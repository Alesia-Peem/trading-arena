/*
 * Trading Arena - browser side.
 *
 * This file has NO game rules. Every action is sent to the C server
 * (src/server.c), which answers with the whole game state as JSON.
 * We then redraw the page from that state.
 *
 * Money arrives in cents (5000 = $50.00), the same as in the C code.
 */
"use strict";

/* ---------- Small helpers ---------- */

const MONEY = new Intl.NumberFormat("en-US", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
const COUNT = new Intl.NumberFormat("en-US");
const MAX_ORDER_QTY = 1000000;   /* same limit as portfolio.h */

const money = (cents) => MONEY.format(cents / 100);

/* Basis points -> "+2.35%". Math.trunc matches C's integer division. */
const percent = (bp) => (bp > 0 ? "+" : bp < 0 ? "-" : "") + MONEY.format(Math.abs(bp) / 100) + "%";
const changeBp = (from, to) => (from > 0 ? Math.trunc(((to - from) * 10000) / from) : 0);
const direction = (bp) => (bp > 0 ? "up" : bp < 0 ? "down" : "flat");

const byId = (id) => document.getElementById(id);

/* Create an element: el("p", { class: "x", text: "hi" }, child1, child2) */
function el(tag, props = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(props)) {
    if (key === "class") node.className = value;
    else if (key === "text") node.textContent = value;
    else if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
    else if (value !== false && value != null) node.setAttribute(key, value);
  }
  node.append(...children.flat().filter((c) => c != null));
  return node;
}

const SVG_NS = "http://www.w3.org/2000/svg";
function svgEl(tag, attrs = {}) {
  const node = document.createElementNS(SVG_NS, tag);
  for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
  return node;
}

/* A "+2.35%" text that is green/red AND has a sign, so color is never the only clue. */
function changeText(bp, extra = "") {
  return el("span", { class: "change " + direction(bp), text: percent(bp) + extra });
}

/* ---------- Talking to the C server ---------- */

let state = null;       /* the latest game state from the server */
let selected = 0;       /* which stock is selected (0-4) */
let busy = false;       /* true while waiting for the server */
let gameOverShownForSeed = null;

async function callServer(path, method = "POST") {
  if (busy) return null;
  setBusy(true);
  try {
    const response = await fetch(path, { method });
    if (!response.ok) throw new Error("HTTP " + response.status);
    const answer = await response.json();
    byId("connection-error").hidden = true;
    state = answer.state;
    render();
    return answer;
  } catch (error) {
    byId("connection-error").hidden = false;
    return null;
  } finally {
    setBusy(false);
  }
}

function setBusy(value) {
  busy = value;
  byId("app").setAttribute("aria-busy", String(value));
  updateButtons();
}

function updateButtons() {
  const over = !state || state.gameOver;
  byId("next-day-btn").disabled = busy || over;
  byId("buy-btn").disabled = busy || over;
  byId("sell-btn").disabled = busy || over;
  byId("max-btn").disabled = busy || over;
  byId("new-game-btn").disabled = busy;
}

/* ---------- Drawing everything ---------- */

function render() {
  if (!state) return;
  renderTopbar();
  renderNews();
  renderStockList();
  renderChart();
  renderTradeHelper();
  renderAccount();
  renderBoard(byId("leaderboard"));
  renderBots();
  updateButtons();
  maybeShowGameOver();
}

function renderTopbar() {
  byId("seed").textContent = "Seed " + state.seed;
  byId("day-text").textContent = state.gameOver
    ? "Day " + state.day + " of " + state.numDays + ", game over"
    : "Day " + state.day + " of " + state.numDays;
  byId("day-meter").style.width = (100 * state.day) / state.numDays + "%";
}

function renderNews() {
  byId("news").hidden = state.news === "";
  byId("news-text").textContent = state.news;
}

/* A tiny line with no axes, for the stock list. It always uses the full width,
   so it shows the shape of the price so far. */
function sparkline(history) {
  const width = 96, height = 28, pad = 3;
  const low = Math.min(...history), high = Math.max(...history);
  const lastDay = Math.max(1, history.length - 1);
  const x = (day) => pad + (day / lastDay) * (width - 2 * pad);
  const y = (price) => (high === low ? height / 2 : pad + ((high - price) / (high - low)) * (height - 2 * pad));
  const d = history.map((p, day) => (day === 0 ? "M" : "L") + x(day).toFixed(1) + " " + y(p).toFixed(1)).join(" ");

  const svg = svgEl("svg", { class: "spark", viewBox: `0 0 ${width} ${height}`, "aria-hidden": "true" });
  svg.append(svgEl("path", { d }));
  return svg;
}

function renderStockList() {
  const list = byId("stock-list");
  list.replaceChildren(
    ...state.stocks.map((stock, i) => {
      const h = stock.history;
      const bp = h.length >= 2 ? changeBp(h[h.length - 2], stock.price) : 0;
      const owned = state.you.shares[i];

      return el("button", {
          class: "stock-row",
          type: "button",
          "aria-pressed": String(i === selected),
          onclick: () => selectStock(i),
        },
        el("span", { class: "symbol", text: stock.symbol }),
        el("span", { class: "name-cell" },
          el("span", { class: "stock-name", text: stock.name }),
          owned > 0 ? el("span", { class: "stock-owned", text: "You own " + COUNT.format(owned) }) : null),
        el("span", { class: "spark-cell" }, sparkline(h)),
        el("span", { class: "price", text: money(stock.price) }),
        changeText(bp));
    }));
}

function selectStock(i) {
  selected = i;
  byId("trade-message").textContent = "";
  render();
}

/* ---------- The big chart ---------- */

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

  const start = Math.floor(low / step) * step;
  const end = Math.ceil(high / step) * step;
  const ticks = [];
  for (let t = start; t <= end + step / 2; t += step) ticks.push(t);
  return { min: start, max: end, ticks };
}

function renderChart() {
  const stock = state.stocks[selected];
  const history = stock.history;
  const start = history[0];

  byId("chart-title").replaceChildren(
    el("span", { class: "symbol", text: stock.symbol }), " ", stock.name);
  byId("chart-subtitle").textContent =
    "Price per share. Moves up to " + stock.volatilityBp / 100 + "% a day, plus news.";
  byId("chart-price").textContent = money(stock.price);
  byId("chart-change").replaceChildren(changeText(changeBp(start, stock.price), " since day 0"));

  const wrap = byId("chart-wrap");
  const width = Math.max(wrap.clientWidth, 280);
  const height = 280;
  const margin = { top: 12, right: 12, bottom: 28, left: 64 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  const scale = niceScale(Math.min(...history), Math.max(...history), 4);
  const x = (day) => margin.left + (day / state.numDays) * plotW;
  const y = (price) => margin.top + ((scale.max - price) / (scale.max - scale.min)) * plotH;

  const svg = svgEl("svg", {
    viewBox: `0 0 ${width} ${height}`,
    role: "img",
    "aria-label": `${stock.name} price chart, day 0 to day ${history.length - 1}. Now ${money(stock.price)}.`,
  });

  /* Gridlines and price labels */
  for (const tick of scale.ticks) {
    svg.append(svgEl("line", { class: "gridline", x1: margin.left, x2: width - margin.right, y1: y(tick), y2: y(tick) }));
    const label = svgEl("text", { class: "axis-label", x: margin.left - 10, y: y(tick) + 4, "text-anchor": "end" });
    label.textContent = money(tick);
    svg.append(label);
  }

  /* Day labels every 5 days */
  for (let day = 0; day <= state.numDays; day += 5) {
    const label = svgEl("text", { class: "axis-label", x: x(day), y: height - 6, "text-anchor": "middle" });
    label.textContent = day === 0 ? "Day 0" : String(day);
    svg.append(label);
  }

  /* Area wash + line + latest-price dot */
  const points = history.map((p, day) => [x(day), y(p)]);
  const linePath = points.map(([px, py], i) => (i === 0 ? "M" : "L") + px.toFixed(1) + " " + py.toFixed(1)).join(" ");
  const last = points[points.length - 1];
  const bottom = margin.top + plotH;

  if (points.length > 1) {
    svg.append(svgEl("path", { class: "price-area", d: `${linePath} L${last[0].toFixed(1)} ${bottom} L${points[0][0].toFixed(1)} ${bottom} Z` }));
    svg.append(svgEl("path", { class: "price-line", d: linePath }));
  }
  svg.append(svgEl("circle", { class: "end-dot", cx: last[0], cy: last[1], r: 4 }));

  /* Hover: crosshair + dot + tooltip at the nearest day */
  const crosshair = svgEl("line", { class: "crosshair", y1: margin.top, y2: bottom, visibility: "hidden" });
  const hoverDot = svgEl("circle", { class: "hover-dot", r: 5, visibility: "hidden" });
  const hitArea = svgEl("rect", { x: margin.left, y: 0, width: plotW, height, fill: "transparent" });
  svg.append(crosshair, hoverDot, hitArea);

  const tooltip = el("div", { class: "tooltip", hidden: true });

  hitArea.addEventListener("pointermove", (event) => {
    const box = svg.getBoundingClientRect();
    const mouseX = ((event.clientX - box.left) / box.width) * width;
    const day = Math.round(((mouseX - margin.left) / plotW) * state.numDays);
    if (day < 0 || day >= history.length) {
      crosshair.setAttribute("visibility", "hidden");
      hoverDot.setAttribute("visibility", "hidden");
      tooltip.hidden = true;
      return;
    }
    const [px, py] = points[day];
    crosshair.setAttribute("x1", px);
    crosshair.setAttribute("x2", px);
    crosshair.setAttribute("visibility", "visible");
    hoverDot.setAttribute("cx", px);
    hoverDot.setAttribute("cy", py);
    hoverDot.setAttribute("visibility", "visible");

    const dayChange = day > 0 ? changeBp(history[day - 1], history[day]) : 0;
    tooltip.replaceChildren(
      el("span", { text: "Day " + day }),
      el("strong", { text: money(history[day]) }),
      day > 0 ? changeText(dayChange, " that day") : el("span", { text: "Starting price" }));
    tooltip.hidden = false;

    /* Keep the tooltip inside the chart: flip it to the left near the right edge. */
    const scaleX = box.width / width;
    const tipWidth = tooltip.offsetWidth;
    let left = px * scaleX + 12;
    if (left + tipWidth > box.width) left = px * scaleX - tipWidth - 12;
    tooltip.style.left = left + "px";
    tooltip.style.top = Math.max(0, py * (box.height / height) - 60) + "px";
  });

  hitArea.addEventListener("pointerleave", () => {
    crosshair.setAttribute("visibility", "hidden");
    hoverDot.setAttribute("visibility", "hidden");
    tooltip.hidden = true;
  });

  wrap.replaceChildren(svg, tooltip);
  renderPriceTable(stock);
}

/* The same data as a table, for screen readers and exact numbers. */
function renderPriceTable(stock) {
  const rows = stock.history.map((price, day) => {
    const bp = day > 0 ? changeBp(stock.history[day - 1], price) : 0;
    return el("tr", {},
      el("td", { text: "Day " + day }),
      el("td", { text: money(price) }),
      el("td", {}, day > 0 ? changeText(bp) : "start"));
  });
  byId("price-table").replaceChildren(
    el("thead", {}, el("tr", {}, el("th", { text: "Day" }), el("th", { text: "Price" }), el("th", { text: "Change" }))),
    el("tbody", {}, rows.reverse()));
}

/* ---------- Trade box ---------- */

function readQty() {
  const value = byId("qty").value.trim();
  if (!/^\d+$/.test(value)) return null;
  const qty = Number(value);
  return qty >= 1 && qty <= MAX_ORDER_QTY ? qty : null;
}

function maxAffordable() {
  const price = state.stocks[selected].price;
  return Math.min(MAX_ORDER_QTY, Math.floor(state.you.cash / price));
}

function renderTradeHelper() {
  const stock = state.stocks[selected];
  const owned = state.you.shares[selected];
  byId("trade-title").textContent = "Trade " + stock.symbol;
  byId("qty-helper").textContent =
    "You can afford " + COUNT.format(maxAffordable()) + ". You own " + COUNT.format(owned) + ".";

  const qty = readQty();
  const total = byId("order-total");
  if (qty === null) {
    total.textContent = "Enter a whole number of shares.";
  } else {
    total.replaceChildren("Order value ", el("b", { text: money(qty * stock.price) }));
  }
}

function showTradeMessage(text, ok) {
  const box = byId("trade-message");
  box.textContent = text;
  box.className = "trade-message " + (ok ? "ok" : "error");
}

async function trade(kind) {
  const qty = readQty();
  if (qty === null) {
    showTradeMessage("Shares must be a whole number from 1 to 1,000,000.", false);
    return;
  }
  const answer = await callServer(`/api/${kind}?stock=${selected}&qty=${qty}`);
  if (answer) showTradeMessage(answer.message, answer.ok);
}

/* ---------- Account, leaderboard, bots ---------- */

function renderAccount() {
  const you = state.you;
  byId("net-worth").textContent = money(you.netWorth);
  byId("net-change").replaceChildren(changeText(changeBp(state.startCash, you.netWorth), " since the start"));
  byId("cash").textContent = money(you.cash);
  byId("invested").textContent = money(you.netWorth - you.cash);

  const items = [];
  state.stocks.forEach((stock, i) => {
    const shares = you.shares[i];
    if (shares > 0) {
      items.push(el("li", {},
        el("span", { class: "symbol", text: stock.symbol }),
        el("span", { class: "count", text: COUNT.format(shares) + " shares" }),
        el("span", { class: "value", text: money(shares * stock.price) })));
    }
  });
  if (items.length === 0) {
    items.push(el("li", { class: "empty", text: "No shares yet. Pick a stock and buy some." }));
  }
  byId("holdings").replaceChildren(...items);
}

function renderBoard(list) {
  list.replaceChildren(
    ...state.leaderboard.map((player, rank) =>
      el("li", { class: player.isYou ? "you" : "" },
        el("span", { class: "board-rank", text: String(rank + 1) }),
        el("span", { text: player.name }),
        el("span", { class: "board-worth", text: money(player.netWorth) }),
        el("span", { class: "board-pct" }, changeText(changeBp(state.startCash, player.netWorth))))));
}

function describeTrades(lastTrades) {
  const parts = [];
  lastTrades.forEach((change, i) => {
    if (change > 0) parts.push("Bought " + COUNT.format(change) + " " + state.stocks[i].symbol);
    if (change < 0) parts.push("Sold " + COUNT.format(-change) + " " + state.stocks[i].symbol);
  });
  return parts.length > 0 ? parts.join(", ") : "No trades";
}

function renderBots() {
  byId("bots-title").textContent = state.day === 0 ? "Bots" : "Bots on day " + state.day;
  byId("bots").replaceChildren(
    ...state.bots.map((bot) =>
      el("li", {},
        el("p", { class: "bot-name", text: bot.name }),
        el("p", { class: "bot-strategy", text: bot.strategy }),
        el("p", { class: "bot-trades", text: state.day === 0 ? "Waiting for day 1" : describeTrades(bot.lastTrades) }))));
}

/* ---------- Game over and new game ---------- */

function maybeShowGameOver() {
  if (!state.gameOver || gameOverShownForSeed === state.seed) return;
  gameOverShownForSeed = state.seed;

  const rank = state.leaderboard.findIndex((p) => p.isYou) + 1;
  const places = ["1st", "2nd", "3rd", "4th"];
  byId("game-over-title").textContent = rank === 1 ? "You won" : "You finished " + places[rank - 1];
  byId("game-over-note").textContent =
    state.you.netWorth < state.startCash
      ? "Markets are risky. That's the point."
      : "Was it skill or luck? Try another seed.";
  renderBoard(byId("final-board"));
  byId("game-over-dialog").showModal();
}

async function startGame(seedText) {
  const seed = seedText.trim();
  const query = /^\d+$/.test(seed) ? "?seed=" + seed : "";
  gameOverShownForSeed = null;
  selected = 0;
  byId("trade-message").textContent = "";
  await callServer("/api/new" + query);
}

/* ---------- Wiring up the buttons ---------- */

byId("next-day-btn").addEventListener("click", async () => {
  byId("trade-message").textContent = "";
  await callServer("/api/next");
});

byId("trade-form").addEventListener("submit", (event) => {
  event.preventDefault();
  trade("buy");
});
byId("sell-btn").addEventListener("click", () => trade("sell"));
byId("max-btn").addEventListener("click", () => {
  byId("qty").value = String(Math.max(1, maxAffordable()));
  renderTradeHelper();
});
byId("qty").addEventListener("input", () => state && renderTradeHelper());

byId("new-game-btn").addEventListener("click", () => byId("new-game-dialog").showModal());
byId("new-game-form").addEventListener("submit", (event) => {
  if (event.submitter && event.submitter.value === "start") startGame(byId("seed-input").value);
});

byId("close-over-btn").addEventListener("click", () => byId("game-over-dialog").close());
byId("replay-btn").addEventListener("click", () => {
  byId("game-over-dialog").close();
  startGame(String(state.seed));
});
byId("over-new-btn").addEventListener("click", () => {
  byId("game-over-dialog").close();
  byId("new-game-dialog").showModal();
});

byId("retry-btn").addEventListener("click", () => callServer("/api/state", "GET"));

/* Keyboard: N = next day, 1-5 = pick a stock (not while typing in a box). */
document.addEventListener("keydown", (event) => {
  if (event.target.closest("input, dialog") || event.metaKey || event.ctrlKey || event.altKey) return;
  if (event.key === "n" || event.key === "N") {
    if (!byId("next-day-btn").disabled) byId("next-day-btn").click();
  } else if (state && event.key >= "1" && event.key <= String(state.stocks.length)) {
    selectStock(Number(event.key) - 1);
  }
});

/* Redraw the chart when the window size changes, so text never stretches. */
let resizeTimer = null;
window.addEventListener("resize", () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(() => state && renderChart(), 100);
});

callServer("/api/state", "GET");
