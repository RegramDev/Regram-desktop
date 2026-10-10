# MSVC for ARM64 rejects the generated key lookup with C1053 (function too
# large) in unoptimized builds, so that one function is compiled for size.

file(READ "${lang_source}" content)
string(FIND "${content}" "NAGRAM_LANG_LOOKUP" patched)
if (NOT patched EQUAL -1)
    return()
endif()

set(signature "\nushort GetKeyIndex(QLatin1String key) {")
string(FIND "${content}" "${signature}" position)
if (position EQUAL -1)
    message(FATAL_ERROR "Nagram: GetKeyIndex not found in ${lang_source}.")
endif()

set(prefix "\n#if defined _MSC_VER && !defined __clang__ // NAGRAM_LANG_LOOKUP")
string(APPEND prefix "\n#pragma runtime_checks(\"\", off)")
string(APPEND prefix "\n#pragma optimize(\"gs\", on)")
string(APPEND prefix "\n#endif")
string(REPLACE "${signature}" "${prefix}${signature}" content "${content}")
file(WRITE "${lang_source}" "${content}")
