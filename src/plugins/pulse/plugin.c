#include "utils/diag.h"

#include "utils/utils.h"
#include "plugin.h"
#include "pulse.h"

static int pulse_plugin_init(const struct plugin_ops *ops, struct diag *diag)
{
    return pulse_ctx_add_sink(ops, diag);
}

static void pulse_plugin_exit(void)
{
}

const struct plugin_api plugin_api = {
    .version = PLUGIN_API_VERSION,

    .init = pulse_plugin_init,
    .exit = pulse_plugin_exit,
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
