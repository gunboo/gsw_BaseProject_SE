"use strict";

const $ = id => document.getElementById(id);
const LABELS = {
  intro:"시작 화면", play:"플레이 중", dialogue:"대화 중", clues:"단서 목록", ending:"엔딩",
  inactive:"비선택", stats:"능력치 화면", downed:"플레이어 쓰러짐",
  paused:"정지", idle:"대기", moving:"이동", returning:"복귀", fleeing:"도주", talking:"대화",
  alive:"생존", respawning:"재생성 대기", stay:"고정", wander:"배회", patrol:"순찰",
  merchant:"상인", healer:"치료사", guard:"경비", bandit:"산적", flee:"회피", follow:"추종",
  hostile:"적대", resident:"주민", wisp:"도깨비불", shade:"원령", oni:"오니",
};
let current = null;
let unavailable = false;

const label = value => Object.prototype.hasOwnProperty.call(LABELS, value) ? LABELS[value] : value;
const worldKey = (source, world) => JSON.stringify([source.sourceId, source.instanceId, world.id]);
const villageKey = (source, world, village) => JSON.stringify([source.sourceId, source.instanceId, world.id, village.id]);

function element(tag, text, className) {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;
  if (className) node.className = className;
  return node;
}

function badge(text, online) {
  return element("span", text, `badge ${online ? "online" : "offline"}`);
}

function options(id, entries, first) {
  const select = $(id);
  const signature = JSON.stringify([first, entries]);
  if (select.dataset.options === signature) return;
  const selected = select.value;
  const nodes = [new Option(first, ""), ...entries.map(([key, name]) => new Option(name, key))];
  select.replaceChildren(...nodes);
  select.value = entries.some(([key]) => key === selected) ? selected : "";
  select.dataset.options = signature;
}

function render() {
  if (!current) return;
  // A failed receiver request invalidates the previous online labels immediately.
  const sources = current.sources.map(source => ({ ...source, online: source.online && !unavailable }));
  const online = sources.filter(source => source.online);
  const loaded = online.flatMap(source => source.worlds);
  const active = loaded.filter(world => world.active);
  $("sources").textContent = online.length;
  $("worlds").textContent = `${active.length} / ${loaded.length}`;
  $("npcs").textContent = active.reduce((sum, world) => sum + world.counts.npcCount, 0);
  $("monsters").textContent = `${active.reduce((sum, world) => sum + world.counts.monsterAlive, 0)} / ${active.reduce((sum, world) => sum + world.counts.monsterSlots, 0)}`;
  const showOffline = $("offline-filter").checked;
  const visibleSources = sources.filter(source => source.online || showOffline);
  options("world-filter", visibleSources.flatMap(source => source.worlds.map(world =>
    [worldKey(source, world), `${source.sourceId} · ${world.name} · ${source.instanceId}`])), "전체 월드");
  const selectedWorld = $("world-filter").value;
  options("village-filter", visibleSources.flatMap(source => source.worlds
    .filter(world => !selectedWorld || worldKey(source, world) === selectedWorld)
    .flatMap(world => world.villages.map(village =>
      [villageKey(source, world, village), `${world.name} · ${village.name} · ${source.instanceId}`]))), "전체 마을");

  const cards = [];
  for (const source of sources) {
    for (const world of source.worlds) {
      const card = element("article", undefined, "world-card");
      const heading = element("div", undefined, "card-heading");
      heading.append(element("h3", world.name), badge(source.online ? label(world.state) : "오프라인", source.online));
      card.append(heading, element("div", `${source.sourceId} / ${source.instanceId} · ${world.id} · ${world.width}×${world.height} · ${world.active ? "선택됨" : "비선택"}`, "world-meta"));
      const count = element("div", undefined, "counts");
      count.append(element("span", `NPC ${world.counts.npcCount}`),
        element("span", `생존 몹 ${world.counts.monsterAlive} / ${world.counts.monsterSlots}`),
        element("span", `재생성 대기 ${world.counts.monsterRespawning}`), element("span", `누적 처치 ${world.kills}`));
      card.append(count);
      for (const village of world.villages) {
        const row = element("div", undefined, "village");
        const c = village.counts;
        row.append(element("strong", village.name),
          element("p", `${village.id} · NPC ${c.npcCount} · 생존 몹 ${c.monsterAlive} / ${c.monsterSlots} · 재생성 대기 ${c.monsterRespawning} · ${source.online ? label(world.state) : "오프라인"}`));
        card.append(row);
      }
      if (!world.villages.length) card.append(element("p", "등록된 마을 없음 · 월드 단위 집계"));
      card.append(element("div", `마지막 수신 ${new Date(source.receivedAt).toLocaleTimeString("ko-KR")} · ${source.ageSeconds.toFixed(1)}초 전`, "world-meta"));
      cards.push(card);
    }
  }
  $("world-cards").replaceChildren(...(cards.length ? cards : [element("div", "아직 수신한 월드가 없습니다. 수신 서버를 켠 상태에서 게임을 실행하세요.", "empty")]));
  const chosenVillage = $("village-filter").value;
  const kind = $("kind-filter").value;
  const search = $("search").value.trim().toLowerCase();
  const rows = [];
  for (const source of visibleSources) {
    for (const world of source.worlds) {
      if (selectedWorld && worldKey(source, world) !== selectedWorld) continue;
      for (const item of world[kind]) {
        if (chosenVillage && JSON.stringify([source.sourceId, source.instanceId, world.id, item.villageId]) !== chosenVillage) continue;
        const values = [item.id, item.kind, item.behavior, item.state, label(item.kind), label(item.behavior), label(item.state)];
        if (search && !values.join(" ").toLowerCase().includes(search)) continue;
        const row = element("tr");
        const sourceCell = element("td", source.sourceId);
        sourceCell.append(element("small", source.instanceId));
        const place = element("td", world.name);
        place.append(element("small", world.villages.find(village => village.id === item.villageId)?.name || "마을 미지정"));
        const state = element("td");
        state.append(badge(source.online ? label(item.state) : `오프라인 · ${label(item.state)}`, source.online));
        row.append(element("td", item.id), sourceCell, place,
          element("td", `${label(item.kind)} / ${label(item.behavior)}`), state,
          element("td", [item.position.x, item.position.y, item.position.z].map(value => value.toFixed(2)).join(", "), "numeric"),
          element("td", item.health === null ? "—" : `${item.health.toFixed(0)} / ${item.maxHealth.toFixed(0)}`, "numeric"),
          element("td", item.alive ? "—" : `${item.respawnSeconds.toFixed(1)}초`, "numeric"));
        rows.push(row);
      }
    }
  }
  $("table-count").textContent = `${kind === "npcs" ? "NPC" : "몹"} ${rows.length}개 표시`;
  if (!rows.length) {
    const row = element("tr");
    const cell = element("td", "조건에 맞는 엔티티가 없습니다.", "empty");
    cell.colSpan = 8;
    row.append(cell);
    rows.push(row);
  }
  $("entities").replaceChildren(...rows);
}

async function poll() {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 2500);
  try {
    const response = await fetch("/api/state", { cache:"no-store", signal:controller.signal });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    current = await response.json();
    unavailable = false;
    const count = current.sources.filter(source => source.online).length;
    $("connection").className = "badge online";
    $("connection").textContent = "수신 서버 연결됨";
    $("updated").textContent = `갱신 ${new Date(current.serverTime).toLocaleTimeString("ko-KR")}`;
    $("notice").textContent = count ? `${count}개 실행 인스턴스에서 JSON을 받고 있습니다. 비선택 월드는 상태가 정지해 있습니다.`
      : "게임 메시지 대기 중입니다. 게임을 실행하거나 다른 프로젝트에서 POST /api/snapshot으로 JSON을 보내세요.";
  } catch {
    unavailable = true;
    $("connection").className = "badge offline";
    $("connection").textContent = "수신 서버 연결 끊김";
    $("notice").textContent = "로컬 수신 서버에 연결할 수 없습니다. 마지막 수신 상태를 보관하며 연결을 다시 시도합니다.";
  } finally {
    clearTimeout(timeout);
    render();
    setTimeout(poll, 1000);
  }
}

for (const id of ["world-filter", "village-filter", "kind-filter", "offline-filter"]) $(id).addEventListener("change", render);
$("search").addEventListener("input", render);
if (location.protocol === "file:") {
  $("connection").className = "badge offline";
  $("connection").textContent = "서버 주소로 열어 주세요";
  $("notice").textContent = "start-monitor.cmd를 실행한 뒤 아래 링크로 접속하세요. HTML 파일을 직접 열면 실시간 수신이 연결되지 않습니다. ";
  const link = element("a", "로컬 모니터 열기 ↗");
  link.href = "http://127.0.0.1:8765/";
  $("notice").append(link);
  $("json-link").href = "http://127.0.0.1:8765/api/state";
} else {
  poll();
}
