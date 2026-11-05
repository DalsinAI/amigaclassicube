/* ClassiCube is written for a stack of 256 KB (CC_BUILD_MAXSTACK) and keeps
 * big buffers on it while loading maps and texture packs; a Shell gives 4 KB.
 * libnix's start-up sets this much up instead. BSD-3-Clause, like ClassiCube. */
unsigned long __stack = 1024 * 1024;
static const char version[] __attribute__((used)) = "$VER: ClassiCube 1.3.9 Amiga (05.11.2026)";
