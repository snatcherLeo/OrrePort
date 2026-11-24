r"""Build the port's bundled GPU pipeline cache from replays' caches (2026-11-03; the reference playthrough P03 note 2).

    python -I tools/local/port/pipeline_seed.py <out initial_pipeline_cache.db> --ref <current pipeline_cache.db>
                                                <cache file or folder> [...]

Why: since port-gaps 52's fix, a draw waits for its GPU pipeline (its compiled shaders plus render state) the first
time it is needed, which can stall a frame (P03: 45 ms at a battle's first send-out, verified). Aurora reads a file
named `initial_pipeline_cache.db` next to the exe at start-up ("bundled initial pipeline cache") and builds those
pipelines in the background before the game needs them. This tool merges the `pipeline_cache.db` files that replays
leave in their `cache\` folders into that file.

Rows are kept only in the reference cache's format: the same schema value and, per pipeline type, the same
config_version (a cache from an older build with another version would be skipped by Aurora anyway). Each pipeline
(type, hash) is kept once, with its earliest first_frame_used (Aurora builds the seed in that order). Folders are
searched for `pipeline_cache.db` files. The rows are render-state descriptions the port generated while drawing, no
disc data. The output must not exist (the tool never overwrites).
"""
import argparse
import os
import sqlite3
import sys


def open_ro(path):
    return sqlite3.connect("file:%s?mode=ro" % path.replace("\\", "/"), uri=True)


def find_caches(paths):
    for p in paths:
        if os.path.isdir(p):
            for root, _dirs, files in os.walk(p):
                if "pipeline_cache.db" in files:
                    yield os.path.join(root, "pipeline_cache.db")
        elif os.path.isfile(p):
            yield p


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("out", help="the new initial_pipeline_cache.db (must not exist)")
    ap.add_argument("--ref", required=True, help="a pipeline_cache.db from the current build (its format is kept)")
    ap.add_argument("inputs", nargs="+", help="pipeline_cache.db files or folders to search")
    a = ap.parse_args()
    if os.path.exists(a.out):
        sys.exit("refusing: %s exists" % a.out)

    ref = open_ro(a.ref)
    schema_sql = [r[0] for r in ref.execute("SELECT sql FROM sqlite_master WHERE sql IS NOT NULL")]
    schema_value = ref.execute("SELECT value FROM aurora_schema").fetchone()[0]
    versions = dict(ref.execute("SELECT DISTINCT type, config_version FROM pipeline_cache"))
    ref.close()

    rows = {}
    used = skipped_files = skipped_rows = 0
    for path in find_caches(a.inputs + [a.ref]):
        try:
            c = open_ro(path)
            if c.execute("SELECT value FROM aurora_schema").fetchone()[0] != schema_value:
                skipped_files += 1
                c.close()
                continue
            for t, h, ver, size, cfg, first in c.execute(
                    "SELECT type, hash, config_version, config_size, config, first_frame_used FROM pipeline_cache"):
                if versions.get(t) != ver:
                    skipped_rows += 1
                    continue
                old = rows.get((t, h))
                if old is None or first < old[4]:
                    rows[(t, h)] = (ver, size, cfg, None, first)
            c.close()
            used += 1
        except sqlite3.Error as e:
            print("skipped %s: %s" % (path, e))
            skipped_files += 1

    out = sqlite3.connect(a.out)
    for sql in schema_sql:
        out.execute(sql)
    out.execute("INSERT INTO aurora_schema(value) VALUES (?)", (schema_value,))
    out.executemany("INSERT INTO pipeline_cache(type, hash, config_version, config_size, config, first_frame_used) "
                    "VALUES (?, ?, ?, ?, ?, ?)",
                    [(t, h, v[0], v[1], v[2], v[4]) for (t, h), v in sorted(rows.items(), key=lambda kv: kv[1][4])])
    out.commit()
    out.close()
    by_type = {}
    for t, _h in rows:
        by_type[t] = by_type.get(t, 0) + 1
    print("%s: %d pipelines (%s) from %d caches; %d caches and %d rows skipped (another format)" % (
        a.out, len(rows), ", ".join("type %d: %d" % kv for kv in sorted(by_type.items())), used, skipped_files,
        skipped_rows))


if __name__ == "__main__":
    main()
