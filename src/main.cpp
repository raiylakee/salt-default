// ~/.config/mimeapps.list /

// /usr/share/applications/mimeapps.list ~
// /etc/xdg/mimeapps.list ~

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

auto
main (void) -> int
{
  spdlog::info ("Application loaded");

  spdlog::info ("Getting home directory path...");
  const char *home_dir{ std::getenv ("HOME") };
  if (home_dir == nullptr)
    {
      spdlog::error ("Could not determine the directory of home directory!");
      return 1;
    }

  spdlog::info ("Found home directory: {}", home_dir);

  spdlog::info ("Loading mimeapps.list...");

  fs::path mimeapps_path{ fs::path (home_dir) / ".config" / "mimeapps.list" };
  std::error_code ec;
  if (!fs::exists (mimeapps_path, ec))
    {
      spdlog::error ("Could not find mimeapps configuration, error code: {}",
                     ec.message ());

      return 1;
    }

  spdlog::info ("Reading mimeapps list...");
  std::ifstream file (mimeapps_path);

  if (!file.is_open ())
    {
      spdlog::error ("Could not open file!");

      return 1;
    }

  std::stringstream buffer{};

  buffer << file.rdbuf ();
  std::string file_content = buffer.str ();

  std::size_t line_number{ 0 };

  for (const auto line : std::views::split (file_content, '\n'))
    {
      ++line_number;
      if (line_number == 2)
        {
          continue;
        }

      std::println ("{}", std::ranges::to<std::string> (line));
    }

  file.close ();

  return 0;
}
