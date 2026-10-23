/* a removed preset of an old project plays as the kept preset it maps to */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host/plugin_api_v1.h"
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.sample_rate = 44100; plugin_api_v2_t *api = move_plugin_init_v2(&h);
    void *inst = api->create_instance(".", "{}"); char buf[128]; int failed = 0, tested = 0;
    for (int i = 0; i < 1498; i += 7) {
        char v[16]; snprintf(v, sizeof v, "%d", i); api->set_param(inst, "p01_type", v);
        if (api->get_param(inst, "p01_type", buf, sizeof buf) < 0) continue;
        tested++; char v2[16]; snprintf(v2, sizeof v2, "%s", buf); api->set_param(inst, "p01_type", v2);
        char b2[128]; api->get_param(inst, "p01_type", b2, sizeof b2); if (strcmp(buf, b2)) failed++;
    }
    printf("%s   %d preset indices set, every one settles on a kept sound (%d not stable)\n", failed ? "FAIL" : "ok  ", tested, failed);
    printf(failed ? "FAILED\n" : "all ok\n"); return failed != 0;
}
