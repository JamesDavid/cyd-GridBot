// GridBot — a tiny in-memory event log. The firmware records what the kid does (levels,
// wins/fails, brains trained, matches) into a ring buffer that a serial-connected tool (or an
// LLM assistant) can read back with the 'E' command, and a '?' command prints a self-describing
// command list. The log is heap-backed so it doesn't eat the (full) static DRAM budget.
#pragma once

namespace applog {

void event(const char* fmt, ...);  // append a timestamped line (also streamed live as "LOG ...")
void dump();                       // print the whole log over Serial (E command)
void help();                       // print the serial command list + how to read the log (? command)

}  // namespace applog
