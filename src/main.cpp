#include <gtkmm/application.h>

#include "app_window.h"
#include "log.h"

auto
main (int argc, char *argv[]) -> int
{
  // Zero-warning defaults (respect explicit user overrides):
  // - GSK_RENDERER=gl avoids Vulkan GPU probe warnings on GPU-less VMs.
  // - GDK_DEBUG=no-portals skips the org.freedesktop.portal.Inhibit
  //   probe, which warns on systems without xdg-desktop-portal.
  //   We never inhibit/idle-block, so portals buy us nothing here.
  if (g_getenv ("GSK_RENDERER") == nullptr)
    {
      g_setenv ("GSK_RENDERER", "gl", FALSE);
    }
  if (const char *dbg = g_getenv ("GDK_DEBUG"); dbg == nullptr || *dbg == '\0')
    {
      g_setenv ("GDK_DEBUG", "no-portals", FALSE);
    }

  spdlog::info ("salt-default starting");

  auto app = Gtk::Application::create ("org.salt.default");

  spdlog::info ("Application loaded");
  return app->make_window_and_run<salt::MainWindow> (argc, argv);
}
