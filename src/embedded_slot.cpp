#include "embedded.h"

// One reserved, read-only Mach-O section per architecture. The exporter writes
// only this section before signing; it never compiles authored panel text as code.
#ifdef Q_OS_MACOS
__attribute__((section("__TEXT,__cp_panel"), used))
#endif
extern const char cpEmbeddedPanel[cp::embeddedSlotSize] = "CP_PANEL_SLOT_V1";
