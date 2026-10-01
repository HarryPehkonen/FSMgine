#!/usr/bin/env python3
"""check_doc_examples — compile every C++ example in the public docs.

A documented example is an executable claim: if an agent or a reader copies it, it must
compile. This extracts every C++ block from README.md, CLAUDE.md and include/**.hpp,
supplies the context the surrounding prose establishes, and compiles it with -fsyntax-only.

WHAT THE WRAPPER SUPPLIES, and what it deliberately does NOT:

  supplies    the umbrella header and the std headers a snippet implies;
              PLACEHOLDER TYPES the prose calls "your event type" (Event, EventType,
              MyEvent); the MACHINES the prose says the reader already has (fsm,
              turnstile) — only when the block does not declare them itself.
  does NOT    `using namespace fsmgine;`. Every block must stand on its own, exactly as a
              reader copying it out of the page would need — which is the rule the fail
              list below is meant to enforce.

Excluded by name: REQUIREMENTS.md — a historical design document whose API sketches
predate the current implementation (it carries a banner saying so).
"""
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DOCS = ["README.md", "CLAUDE.md"]
HEADERS = sorted((REPO / "include").rglob("*.hpp"))
EXCLUDED = {"REQUIREMENTS.md": "historical design doc; API sketches predate the implementation"}

BASE = """
#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <string>
#include <variant>

// --- context the prose establishes (placeholders it names) ---
struct Event { std::string type; };
struct EventType { std::string type; };
struct MyEvent { std::string type; };
static bool condition = true;
static bool coin_is_inserted = false;
static bool is_ready = false;
inline bool get_user_input() { return false; }
"""


def context_for(body):
    """Add the machines the prose says exist, but only if the block does not declare them.

    Declared at FUNCTION scope on purpose: the umbrella header defines
    `namespace fsm = fsmgine`, and every example in this repo names its machine `fsm`, so
    a file-scope variable of that name is a redeclaration of the namespace alias.
    """
    extra = ""
    # match the placeholder event type THIS block uses, so a typed machine lines up
    ev = next((c for c in ("EventType", "MyEvent", "Event") if re.search(rf"\b{c}\b", body)), "EventType")
    if re.search(r"\bfsm\.", body) and not re.search(r"\b(fsmgine::)?FSM<[^>]*>\s*fsm\b", body) \
            and not re.search(r"\bfsm\s*;", body):
        extra += f"    fsmgine::FSM<{ev}> fsm;\n"
    if re.search(r"\bturnstile\.", body) and not re.search(r"turnstile\s*;", body):
        extra += "    fsmgine::EventlessFSM turnstile;\n"
    return extra


def extract():
    out = []
    for doc in DOCS:
        p = REPO / doc
        if not p.exists():
            continue
        lines = p.read_text().split("\n")
        i = 0
        while i < len(lines):
            if re.match(r"^\s*```\s*(cpp|c\+\+|cxx)\s*$", lines[i]):
                start = i + 2
                i += 1
                buf = []
                while i < len(lines) and not lines[i].strip().startswith("```"):
                    buf.append(lines[i])
                    i += 1
                out.append((doc, start, "\n".join(buf)))
            i += 1
    for h in HEADERS:
        lines = h.read_text().split("\n")
        i = 0
        while i < len(lines):
            if re.match(r"^\s*///\s*@code", lines[i]) and ".cmake" not in lines[i]:
                start = i + 2
                i += 1
                buf = []
                while i < len(lines) and not re.match(r"^\s*///\s*@endcode", lines[i]):
                    buf.append(re.sub(r"^\s*///\s?", "", lines[i]))
                    i += 1
                out.append((str(h.relative_to(REPO)), start, "\n".join(buf)))
            i += 1
    return [(f, ln, b) for f, ln, b in out if b.strip()]


def build_source(body):
    if "int main" in body:
        head = body if "#include" in body else "#include <FSMgine/FSMgine.hpp>\n" + body
        return head + "\n"
    return BASE + "\nint main() {\n" + context_for(body) + body + "\n}\n"


def main():
    found = extract()
    print(f"doc examples: {len(found)} C++ block(s) in {', '.join(DOCS)} + include/**.hpp")
    for name, why in EXCLUDED.items():
        if (REPO / name).exists():
            print(f"  excluded by name: {name} ({why})")
    if not found:
        print("FAIL: no doc examples found — an empty subject is not a pass")
        return 1

    failed = []
    for path, line, body in found:
        r = subprocess.run(
            ["g++", "-std=c++17", "-fsyntax-only", "-I", str(REPO / "include"), "-x", "c++", "-"],
            input=build_source(body), capture_output=True, text=True,
        )
        err = next((l.split("error: ")[-1] for l in r.stderr.split("\n") if "error: " in l), "")
        print(f"  {'ok  ' if r.returncode == 0 else 'FAIL'} {path}:{line}" + (f"  -> {err[:86]}" if err else ""))
        if r.returncode != 0:
            failed.append((path, line, err))

    print(f"\n{len(found) - len(failed)}/{len(found)} doc example(s) compile")
    for path, line, err in failed:
        print(f"  {path}:{line}: {err}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
