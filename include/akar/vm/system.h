#pragma once

namespace akar {

class VM;

// Register all system native libraries:
//   io   - File I/O (open, read, write, lines, etc.)
//   fs   - Filesystem ops (exists, mkdir, readdir, etc.)
//   path - Path manipulation (join, dirname, basename, ext, abs)
//   sys  - Environment, process, OS info
//   net  - TCP networking (connect, listen)
//   File - File handle class with methods (read, write, seek, close, etc.)
//   TcpSocket / TcpServer - TCP socket classes
void register_system_libs(VM& vm);

} // namespace akar
