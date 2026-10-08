# The shared session writer is independently pinned from the native force ABI.
get_filename_component(_session_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${_session_root}/lib/toolkit/session/VERSION.json" _session_pin)
string(JSON _session_expected GET "${_session_pin}" sha256)
file(SHA256 "${_session_root}/lib/toolkit/session/session_writer.h" _session_actual)
string(TOLOWER "${_session_expected}" _session_expected)
if(NOT _session_actual STREQUAL _session_expected)
    message(FATAL_ERROR "Vendored session_writer.h differs from its recorded pin")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_session_root}/lib/toolkit/session/session_writer.h"
    "${_session_root}/lib/toolkit/session/VERSION.json")
