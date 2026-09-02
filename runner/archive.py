"""SQLite archive of every experiment.

Nothing is overwritten. Each orchestration writes a new run row and its results
reference it, so an earlier measurement stays readable after the code that
produced it has changed.
"""

import json
import sqlite3
from pathlib import Path

SCHEMA = """
CREATE TABLE IF NOT EXISTS runs (
    run_id      TEXT PRIMARY KEY,
    started_at  TEXT NOT NULL,
    git_commit  TEXT,
    host        TEXT,
    cpu         TEXT,
    compiler    TEXT,
    build_flags TEXT,
    repeats     INTEGER,
    warmup      INTEGER
);

CREATE TABLE IF NOT EXISTS candidates (
    run_id          TEXT NOT NULL,
    candidate       TEXT NOT NULL,
    track           TEXT,
    island          INTEGER,
    origin          TEXT,
    parents         TEXT,
    novelty_status  TEXT,
    expect_verify   TEXT,
    hypothesis      TEXT,
    representation  TEXT,
    path            TEXT,
    PRIMARY KEY (run_id, candidate)
);

CREATE TABLE IF NOT EXISTS workloads (
    run_id     TEXT NOT NULL,
    workload   TEXT NOT NULL,
    visibility TEXT,
    track      TEXT,
    path       TEXT,
    spec       TEXT,
    PRIMARY KEY (run_id, workload)
);

CREATE TABLE IF NOT EXISTS verifications (
    run_id       TEXT NOT NULL,
    candidate    TEXT NOT NULL,
    workload     TEXT NOT NULL,
    status       TEXT,
    expected     TEXT,
    agreed       INTEGER,
    ops_checked  INTEGER,
    sweeps       INTEGER,
    checksum     TEXT,
    failure      TEXT,
    PRIMARY KEY (run_id, candidate, workload)
);

CREATE TABLE IF NOT EXISTS measurements (
    run_id            TEXT NOT NULL,
    candidate         TEXT NOT NULL,
    workload          TEXT NOT NULL,
    visibility        TEXT,
    total_ops         INTEGER,
    frames            INTEGER,
    total_ns          INTEGER,
    ops_per_second    REAL,
    frame_ns_p50      INTEGER,
    frame_ns_p95      INTEGER,
    frame_ns_p99      INTEGER,
    frame_ns_max      INTEGER,
    peak_bytes        INTEGER,
    reported_bytes    INTEGER,
    bytes_per_entity  REAL,
    alloc_count       INTEGER,
    free_count        INTEGER,
    alloc_total_bytes INTEGER,
    final_entities    INTEGER,
    checksum          TEXT,
    checksum_matches_oracle INTEGER,
    pmu_available     INTEGER,
    pmu               TEXT,
    repetition_total_ns TEXT,
    PRIMARY KEY (run_id, candidate, workload)
);

CREATE TABLE IF NOT EXISTS pareto (
    run_id     TEXT NOT NULL,
    scope      TEXT NOT NULL,
    workload   TEXT,
    candidate  TEXT NOT NULL,
    objectives TEXT,
    PRIMARY KEY (run_id, scope, workload, candidate)
);
"""


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(str(self.path))
        self.db.executescript(SCHEMA)
        self.db.commit()

    def close(self):
        self.db.commit()
        self.db.close()

    def add_run(self, run):
        self.db.execute(
            "INSERT OR REPLACE INTO runs VALUES (:run_id,:started_at,:git_commit,:host,"
            ":cpu,:compiler,:build_flags,:repeats,:warmup)",
            run,
        )

    def add_candidate(self, run_id, manifest, path):
        self.db.execute(
            "INSERT OR REPLACE INTO candidates VALUES (?,?,?,?,?,?,?,?,?,?,?)",
            (
                run_id,
                manifest.get("name"),
                manifest.get("track"),
                manifest.get("island", -1),
                manifest.get("origin"),
                json.dumps(manifest.get("parents", [])),
                manifest.get("novelty_status"),
                manifest.get("expect_verify", "pass"),
                (manifest.get("hypothesis") or "").strip(),
                (manifest.get("representation") or "").strip(),
                str(path),
            ),
        )

    def add_workload(self, run_id, name, visibility, track, path, spec_text):
        self.db.execute(
            "INSERT OR REPLACE INTO workloads VALUES (?,?,?,?,?,?)",
            (run_id, name, visibility, track, str(path), spec_text),
        )

    def add_verification(self, run_id, candidate, workload, result, expected):
        status = result.get("status", "error")
        agreed = int((status == "passed") == (expected == "pass"))
        self.db.execute(
            "INSERT OR REPLACE INTO verifications VALUES (?,?,?,?,?,?,?,?,?,?)",
            (
                run_id,
                candidate,
                workload,
                status,
                expected,
                agreed,
                result.get("ops_checked", 0),
                result.get("sweeps", 0),
                str(result.get("checksum", "")),
                result.get("failure", ""),
            ),
        )

    def add_measurement(self, run_id, candidate, workload, m, checksum_matches):
        self.db.execute(
            "INSERT OR REPLACE INTO measurements VALUES "
            "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (
                run_id,
                candidate,
                workload,
                m.get("visibility"),
                m.get("total_ops"),
                m.get("frames"),
                m.get("total_ns"),
                m.get("ops_per_second"),
                m.get("frame_ns_p50"),
                m.get("frame_ns_p95"),
                m.get("frame_ns_p99"),
                m.get("frame_ns_max"),
                m.get("peak_bytes"),
                m.get("reported_bytes"),
                m.get("bytes_per_entity"),
                m.get("alloc_count"),
                m.get("free_count"),
                m.get("alloc_total_bytes"),
                m.get("final_entities"),
                str(m.get("checksum", "")),
                int(bool(checksum_matches)),
                int(bool(m.get("pmu_available"))),
                json.dumps(m.get("pmu")),
                json.dumps(m.get("repetition_total_ns", [])),
            ),
        )

    def add_pareto(self, run_id, scope, workload, candidate, objectives):
        self.db.execute(
            "INSERT OR REPLACE INTO pareto VALUES (?,?,?,?,?)",
            (run_id, scope, workload or "", candidate, json.dumps(objectives)),
        )
