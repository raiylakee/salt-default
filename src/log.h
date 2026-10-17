#pragma once
// Minimal spdlog-compatible shim (spdlog is not packaged on every distro):
// info/debug/warn/error(fmt, args...) on top of C++26 <print>.

#include <format>
#include <print>
#include <utility>

namespace spdlog
{

template <typename... Args>
void
info (std::format_string<Args...> fmt, Args &&...args)
{
  std::println ("[info] {}", std::format (fmt, std::forward<Args> (args)...));
}

template <typename... Args>
void
debug (std::format_string<Args...> fmt, Args &&...args)
{
  std::println ("[debug] {}", std::format (fmt, std::forward<Args> (args)...));
}

template <typename... Args>
void
warn (std::format_string<Args...> fmt, Args &&...args)
{
  std::println ("[warn] {}", std::format (fmt, std::forward<Args> (args)...));
}

template <typename... Args>
void
error (std::format_string<Args...> fmt, Args &&...args)
{
  std::println (stderr, "[error] {}",
                std::format (fmt, std::forward<Args> (args)...));
}

inline void
info (const char *msg)
{
  std::println ("[info] {}", msg);
}

} // namespace spdlog
