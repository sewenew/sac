# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
cmake -B build
cmake --build build
```

To rebuild after changes:

```bash
cmake --build build
```

Run the binary:

```bash
./build/sac
```

## Project Structure

```
src/sw/sac/
    errors.h / errors.cpp              # LlmError hierarchy + throw_* helpers
    http_client.h / http_client.cpp    # HttpClient (abstract) + CurlHttpClient
    llm_client.h / llm_client.cpp      # Message, Role, LlmConfig, LlmClient facade
                                       # + make_anthropic_provider / make_openai_provider
    providers/
        llm_provider.h                 # LlmProvider strategy interface (no .cpp)
        anthropic_provider.h / .cpp    # Anthropic Messages API
        openai_provider.h / .cpp       # OpenAI-compatible (also ARK, Kimi)
```

- `build/` — out-of-source build directory (not committed)

### Key design decisions

- **`LlmClient`** is a stable facade that delegates to a `LlmProvider` strategy. Adding a new provider never changes `LlmClient`'s header.
- **`HttpClient`** is an abstract base; `CurlHttpClient` is the libcurl impl. Swap to cpp-httplib later without touching providers.
- **`LlmClient` does not own `HttpClient`** — it holds a reference. One `CurlHttpClient` can serve multiple `LlmClient` instances.
- **ARK and Kimi** reuse `OpenAiProvider` with a different `LlmConfig.base_url`; the wire format is OpenAI-compatible.
- **SSE parsing** lives in `CurlHttpClient::_dispatch_events`: accumulates raw bytes, splits on `"\n\n"`, strips `"data: "` prefix, skips `"[DONE]"`.
- **Anthropic** differs from OpenAI: `x-api-key` auth header, `system` is a top-level field (not a messages entry), `max_tokens` required, response at `content[0].text`.

## Standards

- C++17
- Warnings: `-Wall -Wextra -Wpedantic`

## Coding Style

Based on the redis-plus-plus coding conventions.

### Naming

| Entity | Convention | Example |
|---|---|---|
| Classes / structs | `UpperCamelCase` | `ConnectionPool`, `SafeConnection` |
| Public methods | `snake_case` | `bool broken() const` |
| Private methods | `_snake_case` (leading `_`) | `void _set_options()` |
| Data members | `_snake_case` (leading `_`) | `ConnectionOptions _opts` |
| Free functions | `snake_case` | `void throw_error(...)` |
| `enum class` type | `UpperCamelCase` | `enum class Role` |
| `enum class` values | `UPPER_CASE` | `Role::MASTER` |
| Template parameters | `UpperCamelCase` | `template <typename Input>` |
| Type aliases | `UpperCamelCase` + suffix | `using ContextUPtr = ...` |
| Namespaces | lowercase | `namespace sw::redis {` |
| Macros | `UPPER_CASE_WITH_PREFIX` | `PROJECT_HAS_FEATURE` |

### Formatting

- 4 spaces, no tabs.
- Opening brace on the same line (K&R style) for classes, functions, control flow, and namespaces.
- Namespace bodies are **not** indented; each `namespace` on its own line.
- Continuation lines in multi-line signatures: 8-space indent (double) to visually separate from the body.

### Headers

- Traditional `#ifndef` guards (no `#pragma once`). Guard name: `PROJECTNAME_<FILENAME_UPPER>_H`. `#endif` carries a `// end GUARD_NAME` comment.
- In `.cpp` files: own header first, then system headers, then project headers.
- In `.h` files: system headers first, then project headers.
- Use full include paths (e.g., `"sw/redis++/errors.h"`), not bare filenames.

### Class Layout

```
public:
    constructors (explicit single-arg first)
    copy / move constructors and assignment operators (= delete or = default)
    destructor
    public methods and accessors
    friend declarations
private:
    private helper methods  (_prefixed)
    nested private classes / structs
    data members             (_prefixed)
```

Use `struct` for plain data; `class` for everything with behaviour.

### Qualifiers

- `const` on all non-mutating methods.
- `noexcept` on methods guaranteed not to throw, on `swap`, and on defaulted special members where appropriate.
- `override` on every virtual override, including destructors.
- `[[noreturn]]` on functions that always throw.
- `explicit` on single-argument constructors.
- In-member initializers for default values (e.g., `int port = 6379;`).

### Error Handling

- Exceptions exclusively; no error-code returns.
- Exception hierarchy rooted at a project `Error` base class derived from `std::runtime_error`.
- `[[noreturn]]` free functions (`throw_error`) translate low-level errors into typed exceptions.
- `assert()` for internal invariants (not for user-facing validation).

### Modern C++ Usage

- Use `auto` for local variables when the type is clear from context; use trailing return types (`-> T`) for complex dependent types.
- Perfect forwarding (`std::forward`) and variadic templates used pervasively.
- SFINAE via `std::enable_if` for template overload selection.
- Custom deleters as private nested `struct` functors with `unique_ptr`.
- `swap` as a `friend` free function (not a member).
- Anonymous namespaces in `.cpp` files for translation-unit-local helpers; `using namespace` only inside anonymous namespaces in `.cpp` files, never in headers.

### Comments

- Prefer good (variable/function) naming than comments.
- Plain `//` line comments. Explain *why*, not *what*.
- License header at top of every file (block `/* */` style).
- No `//` comment clutter on obvious code.
