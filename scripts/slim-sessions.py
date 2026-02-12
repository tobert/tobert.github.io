#!/usr/bin/env python3
"""Slim Claude Code JSONL session files for publishing.

Keeps the conversation flow (user prompts, assistant text, tool calls)
while stripping bulk data (progress events, file snapshots, full tool
output, thinking blocks).

Tool results are truncated to a summary line. The goal is a readable
conversation log that a blog formatter can render as HTML.

Usage:
    python3 slim-sessions.py INPUT_DIR OUTPUT_DIR
    python3 slim-sessions.py ~/.claude/projects/-home-atobey-src-kaish/ out/
"""

import json
import os
import sys
from pathlib import Path

TOOL_RESULT_MAX = 200  # chars of tool output to keep


def slim_content(content):
    """Slim a message content array, keeping structure but dropping bulk."""
    if isinstance(content, str):
        return content

    if not isinstance(content, list):
        return content

    slimmed = []
    for block in content:
        if not isinstance(block, dict):
            slimmed.append(block)
            continue

        t = block.get("type")

        if t == "thinking":
            # Drop thinking blocks entirely — they're huge and private
            continue

        if t == "tool_result":
            raw = block.get("content", "")
            if isinstance(raw, str):
                preview = raw[:TOOL_RESULT_MAX]
                total = len(raw)
            elif isinstance(raw, list):
                # Sometimes content is a list of blocks
                flat = json.dumps(raw)
                preview = flat[:TOOL_RESULT_MAX]
                total = len(flat)
            else:
                preview = str(raw)[:TOOL_RESULT_MAX]
                total = len(str(raw))

            slimmed.append({
                "type": "tool_result",
                "tool_use_id": block.get("tool_use_id"),
                "is_error": block.get("is_error", False),
                "content_preview": preview,
                "content_length": total,
            })
            continue

        if t == "tool_use":
            # Keep tool name and input, but truncate large input values
            inp = block.get("input", {})
            slim_input = {}
            for k, v in inp.items():
                if isinstance(v, str) and len(v) > 500:
                    slim_input[k] = v[:500] + f"... ({len(v)} chars)"
                else:
                    slim_input[k] = v
            slimmed.append({
                "type": "tool_use",
                "id": block.get("id"),
                "name": block.get("name"),
                "input": slim_input,
            })
            continue

        if t == "text":
            slimmed.append(block)
            continue

        # Keep anything else as-is (rare)
        slimmed.append(block)

    return slimmed


def slim_message(record):
    """Slim a single JSONL record. Returns None to drop it entirely."""
    t = record.get("type")

    # Drop entirely: progress events, file snapshots
    if t in ("progress", "file-history-snapshot"):
        return None

    # Keep summary records as-is (tiny)
    if t == "summary":
        return record

    # Keep system records as-is
    if t == "system":
        return record

    # Slim user and assistant messages
    if t in ("user", "assistant"):
        msg = record.get("message", {})
        if isinstance(msg, dict):
            content = msg.get("content")
            if content is not None:
                msg = dict(msg)
                msg["content"] = slim_content(content)
                # Drop usage stats
                msg.pop("usage", None)

            record = dict(record)
            record["message"] = msg

        # Keep essential metadata, drop noise
        keep_keys = {
            "type", "message", "sessionId", "uuid", "parentUuid",
            "timestamp", "isSidechain", "agentId", "slug",
        }
        return {k: v for k, v in record.items() if k in keep_keys}

    # Unknown type — keep but warn
    return record


def slim_session(input_path, output_path):
    """Process one JSONL session file."""
    kept = 0
    dropped = 0

    with open(input_path) as fin, open(output_path, "w") as fout:
        for line in fin:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue

            result = slim_message(record)
            if result is None:
                dropped += 1
            else:
                fout.write(json.dumps(result, ensure_ascii=False) + "\n")
                kept += 1

    return kept, dropped


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} INPUT_DIR OUTPUT_DIR", file=sys.stderr)
        sys.exit(1)

    input_dir = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    output_dir.mkdir(parents=True, exist_ok=True)

    # Copy sessions-index.json as-is
    idx = input_dir / "sessions-index.json"
    if idx.exists():
        (output_dir / "sessions-index.json").write_text(idx.read_text())

    total_kept = 0
    total_dropped = 0
    files_processed = 0

    for jsonl in sorted(input_dir.glob("*.jsonl")):
        out = output_dir / jsonl.name
        kept, dropped = slim_session(jsonl, out)
        total_kept += kept
        total_dropped += dropped
        files_processed += 1

        in_size = jsonl.stat().st_size
        out_size = out.stat().st_size
        ratio = out_size / in_size * 100 if in_size > 0 else 0
        print(f"  {jsonl.name}: {in_size/1024:.0f}K → {out_size/1024:.0f}K ({ratio:.0f}%)")

    print(f"\n{files_processed} files, {total_kept} records kept, {total_dropped} dropped")

    # Total size comparison
    in_total = sum(f.stat().st_size for f in input_dir.glob("*.jsonl"))
    out_total = sum(f.stat().st_size for f in output_dir.glob("*.jsonl"))
    print(f"Total: {in_total/1024/1024:.0f}MB → {out_total/1024/1024:.0f}MB ({out_total/in_total*100:.0f}%)")


if __name__ == "__main__":
    main()
