#include <gtkmm/application.h>

#include "app_window.h"
#include "log.h"

auto
main (int argc, char *argv[]) -> int
{
  // Warning-free host defaults, explicit user values win: GSK_RENDERER=gl
  // avoids the Vulkan probe on GPU-less machines, GDK_DEBUG=no-portals the
  // Inhibit probe on systems without xdg-desktop-portal. Both are skipped
  // inside a Flatpak, where no-portals would also cut off the settings portal
  // that supplies the host theme, fonts and colour scheme.
  if (g_getenv ("FLATPAK_ID") == nullptr)
    {
      if (g_getenv ("GSK_RENDERER") == nullptr)
        {
          g_setenv ("GSK_RENDERER", "gl", FALSE);
        }
      if (const char *dbg = g_getenv ("GDK_DEBUG");
          dbg == nullptr || *dbg == '\0')
        {
          g_setenv ("GDK_DEBUG", "no-portals", FALSE);
        }
    }

  spdlog::info ("salt-default starting");

  auto app = Gtk::Application::create ("org.salt.default");

  spdlog::info ("Application loaded");
  return app->make_window_and_run<salt::MainWindow> (argc, argv);
}
