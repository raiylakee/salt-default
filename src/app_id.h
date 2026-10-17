#pragma once

namespace salt
{

// Application id, which is also the basename of the installed desktop file:
// GApplication looks up <app-id>.desktop for window grouping and the shell
// uses it to match the running window to its launcher.
inline constexpr const char *kAppId = "org.salt.default";

} // namespace salt
