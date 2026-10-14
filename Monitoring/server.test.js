"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const { Readable } = require("node:stream");
const { validateSnapshot, SnapshotStore, createHandler, OFFLINE_MS, RETENTION_MS } = require("./server");
const example = require("./example-snapshot.json");
const fixture = () => JSON.parse(JSON.stringify(example));

test("derives world and village totals from alive entities, preserving unknown NPC health", () => {
  const snapshot = validateSnapshot(fixture());
  const world = snapshot.worlds[0];
  assert.deepEqual(world.counts, { npcCount:1, npcAlive:1, monsterAlive:1, monsterSlots:2, monsterRespawning:1 });
  assert.deepEqual(world.villages[0].counts, world.counts);
  assert.equal(world.npcs[0].health, null);
});

test("separates multiple villages and entities without a village", () => {
  const raw = fixture();
  const world = raw.worlds[0];
  world.villages.push({ id:"town-b", name:"Town B" });
  world.monsters[0].villageId = "town-b";
  world.monsters[1].villageId = "";
  const parsed = validateSnapshot(raw).worlds[0];
  assert.equal(parsed.villages[0].counts.monsterSlots, 0);
  assert.equal(parsed.villages[1].counts.monsterAlive, 1);
  assert.equal(parsed.counts.monsterSlots, 2);
});

test("rejects unsupported schemas, duplicates, dangling village references, and invalid numbers", () => {
  const mutations = [
    raw => { raw.schemaVersion = 2; },
    raw => { raw.sequence = 0; },
    raw => { raw.sequence = Number.MAX_SAFE_INTEGER + 1; },
    raw => { raw.worlds.push(raw.worlds[0]); },
    raw => { raw.worlds[0].npcs.push(raw.worlds[0].npcs[0]); },
    raw => { raw.worlds[0].monsters[0].id = raw.worlds[0].npcs[0].id; },
    raw => { raw.worlds[0].npcs[0].villageId = "missing"; },
    raw => { raw.worlds[0].npcs[0].position.x = Infinity; },
    raw => { raw.worlds[0].monsters[0].health = -1; },
    raw => { raw.worlds[0].monsters[0].health = 99; },
    raw => { raw.worlds[0].npcs[0].alive = "true"; },
  ];
  for (const mutate of mutations) {
    const raw = fixture();
    mutate(raw);
    assert.throws(() => validateSnapshot(raw));
  }
});

test("full snapshots remove departed entities and unloaded worlds", () => {
  const store = new SnapshotStore(() => 10000);
  store.accept(fixture());
  const second = fixture();
  second.sequence = 2;
  second.worlds[0].npcs = [];
  store.accept(second);
  assert.equal(store.state().sources[0].worlds[0].counts.npcCount, 0);
  const third = fixture();
  third.sequence = 3;
  third.worlds = [];
  store.accept(third);
  assert.deepEqual(store.state().sources[0].worlds, []);
});

test("old and duplicate sequences do not replace state or refresh connection status", () => {
  let now = 10000;
  const store = new SnapshotStore(() => now);
  const raw = fixture();
  raw.sequence = 10;
  store.accept(raw);
  now += OFFLINE_MS + 1;
  assert.equal(store.accept(raw), false);
  assert.equal(store.accept(fixture()), false);
  assert.equal(store.state().sources[0].sequence, 10);
  assert.equal(store.state().sources[0].online, false);
  raw.sequence = 11;
  assert.equal(store.accept(raw), true);
  assert.equal(store.state().sources[0].online, true);
});

test("different projects and simultaneous processes cannot overwrite one another", () => {
  const store = new SnapshotStore(() => 10000);
  const first = fixture();
  const second = fixture();
  second.instanceId = "another-process";
  const third = fixture();
  third.sourceId = "other-project";
  store.accept(first);
  store.accept(second);
  store.accept(third);
  assert.equal(store.state().sources.length, 3);
});

test("invalid snapshots preserve the last good state", () => {
  const store = new SnapshotStore(() => 10000);
  store.accept(fixture());
  const invalid = fixture();
  invalid.sequence = 2;
  invalid.worlds[0].monsters[0].health = -2;
  assert.throws(() => store.accept(invalid));
  assert.equal(store.state().sources[0].sequence, 1);
});

test("offline state is retained briefly, then removed", () => {
  let now = 10000;
  const store = new SnapshotStore(() => now);
  store.accept(fixture());
  now += OFFLINE_MS + 1;
  assert.equal(store.state().sources[0].online, false);
  now += RETENTION_MS;
  assert.equal(store.state().sources.length, 0);
});

// Exercise the actual HTTP handler without starting an application or listener.
async function request(handler, method, url, body = "", extraHeaders = {}) {
  const req = Readable.from([Buffer.isBuffer(body) ? body : Buffer.from(body)]);
  req.method = method;
  req.url = url;
  req.headers = { host:"127.0.0.1:8765", "content-type":"application/json", ...extraHeaders };
  const res = {
    status:0, headers:{}, body:"", writableEnded:false, destroyed:false,
    writeHead(status, headers) { this.status = status; this.headers = headers; },
    end(content) { this.body = content; this.writableEnded = true; },
  };
  await handler(req, res);
  return res;
}

test("HTTP handler accepts UTF-8 JSON and returns collected state", async () => {
  const handler = createHandler(new SnapshotStore(() => 10000));
  const posted = await request(handler, "POST", "/api/snapshot", JSON.stringify(fixture()));
  assert.equal(posted.status, 200);
  assert.equal(JSON.parse(posted.body).accepted, true);
  const state = await request(handler, "GET", "/api/state");
  assert.equal(JSON.parse(state.body).sources[0].worlds[0].name, "예제 월드 A");
  assert.equal(state.headers["Cache-Control"], "no-store");
});

test("HTTP handler rejects broken JSON, invalid UTF-8, unsupported content type, and oversized input", async () => {
  const handler = createHandler();
  assert.equal((await request(handler, "POST", "/api/snapshot", "{")).status, 400);
  assert.equal((await request(handler, "POST", "/api/snapshot", Buffer.from([0xff]))).status, 400);
  assert.equal((await request(handler, "POST", "/api/snapshot", "{}", { "content-type":"text/plain" })).status, 415);
  assert.equal((await request(handler, "POST", "/api/snapshot", "{}", { "content-type":"application/jsonevil" })).status, 415);
  assert.equal((await request(handler, "POST", "/api/snapshot", "x".repeat(2 * 1024 * 1024 + 1))).status, 413);
  assert.equal((await request(handler, "POST", "/api/snapshot", "{}", { "content-length":"2097153" })).status, 413);
});

test("HTTP handler rejects other hosts and origins and limits static file routes", async () => {
  const handler = createHandler();
  assert.equal((await request(handler, "GET", "/api/state", "", { host:"other.example:8765" })).status, 403);
  assert.equal((await request(handler, "GET", "/api/state", "", { origin:"https://other.example" })).status, 403);
  assert.equal((await request(handler, "GET", "/server.js")).status, 404);
  assert.equal((await request(handler, "GET", "/")).status, 200);
});

test("dashboard assets resolve beside a directly opened HTML file and through HTTP", async () => {
  const handler = createHandler();
  const page = await request(handler, "GET", "/index.html");
  const assets = [...page.body.matchAll(/(?:href|src)="([^"]+\.(css|js))"/g)];
  assert.equal(assets.length, 2);
  for (const [, reference, extension] of assets) {
    const local = new URL(reference, "file:///C:/project/Monitoring/index.html");
    assert.equal(local.pathname, `/C:/project/Monitoring/dashboard.${extension}`);
    const remote = new URL(reference, "http://127.0.0.1:8765/index.html");
    const asset = await request(handler, "GET", remote.pathname);
    assert.equal(asset.status, 200);
    assert.match(asset.headers["Content-Type"], extension === "css" ? /^text\/css/ : /^text\/javascript/);
    assert.ok(asset.body.length > 0);
  }
});

test("source capacity rejects overflow while live, then reuses an offline slot", () => {
  let now = 10000;
  const store = new SnapshotStore(() => now);
  for (let index = 0; index < 64; ++index) {
    const raw = fixture();
    raw.instanceId = `instance-${index}`;
    store.accept(raw);
  }
  const overflow = fixture();
  overflow.instanceId = "overflow";
  assert.throws(() => store.accept(overflow), error => error.status === 503);
  now += OFFLINE_MS + 1;
  assert.equal(store.accept(overflow), true);
  assert.equal(store.state().sources.length, 64);
});
