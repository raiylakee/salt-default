// ~/.config/mimeapps.list /

// /usr/share/applications/mimeapps.list ~
// /etc/xdg/mimeapps.list ~

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtkmm-4.0/gtkmm/application.h>
#include <gtkmm-4.0/gtkmm/applicationwindow.h>
#include <gtkmm-4.0/gtkmm/enums.h>
#include <iostream>
#include <print>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <system_error>

#include <gtkmm-4.0/gtkmm.h>

class MainWindow : public Gtk::ApplicationWindow
{
public:
  MainWindow ()
  {
    set_title ("Hello gtk4");
    set_default_size (400, 300);

    root.set_orientation (Gtk::Orientation::HORIZONTAL);
    set_child (root);

    sidebar.set_stack (stack);
    root.append (sidebar);

    stack.add (page1, "Page 1", "Page 1");
    stack.add (page2, "Page 2", "Page 2");

    page1.set_label ("Page 1");
    page2.set_label ("Page 2");

    root.append (stack);
  }

private:
  Gtk::Box root;
  Gtk::Stack stack;
  Gtk::StackSidebar sidebar;

  Gtk::Label page1, page2;
};

namespace fs = std::filesystem;

auto
main (int argc, char *argv[]) -> int
{
  auto app{ Gtk::Application::create ("org.salt.default") };

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

  return app->make_window_and_run<MainWindow> (argc, argv);
}
