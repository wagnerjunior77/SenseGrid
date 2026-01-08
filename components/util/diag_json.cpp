
#include "diag_json.h"

static const char* status_str(uint8_t s) {
    switch (s) {
        case 0: return "none";
        case 1: return "move";
        case 2: return "exist";
        default: return "?";
    }
}

void radar_print_json(const RadarParsed* p, Stream& out, uint32_t ts_ms) {
    if (!p) return;
    // Formato enxuto para linha única
    out.print('{');
    out.print("\"ts_ms\":"); out.print(ts_ms);
    out.print(",\"status\":\""); out.print(status_str(p->status)); out.print('"');
    out.print(",\"dist_m\":"); out.print(p->dist_m, 3);
    out.print(",\"speed_mps\":"); out.print(p->speed_mps, 3);
    out.print(",\"snr\":"); out.print(p->snr, 3);
    // extras brutos (úteis p/ calibração)
    out.print(",\"distance_cm\":"); out.print(p->distance_cm);
    out.print(",\"speed_cms\":"); out.print(p->speed_cms);
    out.print(",\"signal\":"); out.print(p->signal);
    out.print(",\"az_deg\":"); out.print((int)p->azim_deg);
    out.print(",\"el_deg\":"); out.print((int)p->elev_deg);
    out.print('}');
    out.println();
}
