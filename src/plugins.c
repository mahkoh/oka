#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>

#include "utils/utils.h"
#include "utils/xmalloc.h"
#include "utils/diag.h"
#include "utils/vec.h"

#include "globals.h"
#include "plugins.h"
#include "plugin.h"
#include "player.h"

#define PLUGIN_ENV "OKA_PLUGIN_DIR"

UTILS_VECTOR(plugin, struct plugin *)
UTILS_VECTOR(sink, struct sink *)
UTILS_VECTOR(decoder, struct decoder *)

struct plugin {
    const struct plugin_api *api;
    void *handle;
};

static char *plugins_dir;
static struct plugin_vector plugins;
static struct decoder_vector plugins_decoders;
static struct sink_vector plugins_sinks;
static struct sink *plugins_current_sink;

static int plugins_add_sink(struct sink *sink, const struct sink_ops **ops,
        struct loop **loop)
{
    for (size_t i = 0; i < plugins_sinks.len; i++) {
        auto sink2 = plugins_sinks.ptr[i];
        if (strcmp(sink2->name, sink->name) == 0)
            return -1;
    }

    player_get_sink_ops(ops, loop);

    sink_vector_push(&plugins_sinks, sink);

    return 0;
}

static int plugins_add_decoder(struct decoder *decoder)
{
    for (size_t i = 0; i < plugins_decoders.len; i++) {
        auto d = plugins_decoders.ptr[i];
        if (strcmp(d->name, decoder->name) == 0)
            return -1;
    }

    decoder_vector_push(&plugins_decoders, decoder);

    return 0;
}

static const struct plugin_ops plugin_ops = {
    .add_sink = plugins_add_sink,
    .add_decoder = plugins_add_decoder,
};

static void plugins_load_one(const char *name)
{
    auto path = xstrjoin(plugins_dir, "/", name);
    auto handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        diag_err(main_diag, "could not open %s: %s", path, dlerror());
        return;
    }

    struct plugin_api *api = dlsym(handle, "plugin_api");
    if (!api || api->version != PLUGIN_API_VERSION) {
        diag_err(main_diag, "%s: plugin is incompatible", path);
        return;
    }

    auto plugin = xnew_uninit(struct plugin);
    plugin->handle = handle;
    plugin->api = api;

    plugin_vector_push(&plugins, plugin);
    api->init(&plugin_ops, main_diag);
}

static void plugins_load(void)
{
    auto d = opendir(plugins_dir);
    if (!d)
        diag_fatal(main_diag, "could not open " PLUGIN_ENV " (%s): %s", plugins_dir,
                utils_strerr(errno));

    struct dirent *entry;
    while ((entry = readdir(d))) {
        auto dot = strrchr(entry->d_name, '.');
        if (dot && strcmp(dot, ".so") == 0)
            plugins_load_one(entry->d_name);
    }
    closedir(d);
}

static void plugins_dir_init(void)
{
    auto dir = getenv(PLUGIN_ENV);
    if (!dir)
        diag_fatal(main_diag, "the " PLUGIN_ENV " environment variable is not set");
    plugins_dir = strdup(dir);
}

void plugins_init(void)
{
    plugins_dir_init();
    plugins_load();

    if (plugins_sinks.len > 0) {
        plugins_current_sink = plugins_sinks.ptr[0];
        player_set_sink(plugins_current_sink);
    }
}

static void plugin_free(struct plugin *p)
{
    if (p->api->exit)
        p->api->exit();

    dlclose(p->handle);
    free(p);
}

void plugins_exit(void)
{
    free(plugins_dir);

    for (size_t i = 0; i < plugins_decoders.len; i++) {
        auto decoder = plugins_decoders.ptr[i];
        decoder->free(decoder);
    }
    free(plugins_decoders.ptr);

    for (size_t i = 0; i < plugins_sinks.len; i++) {
        auto sink = plugins_sinks.ptr[i];
        sink->free(sink);
    }
    free(plugins_sinks.ptr);

    for (size_t i = 0; i < plugins.len; i++)
        plugin_free(plugins.ptr[i]);
    free(plugins.ptr);
}

struct decoder_stream *plugins_open(const char *path)
{
    auto decoder = plugins_decoders.ptr[0];
    char *metadata[METADATA_NUM_TAGS] = { 0 };
    decoder->metadata(decoder, path, metadata);
    for (size_t i = 0; i < METADATA_NUM_TAGS; i++) {
        if (metadata[i]) {
            diag_info(main_diag, "%zu: %s", i, metadata[i]);
            free(metadata[i]);
        }
    }
    return decoder->open(decoder, path, &plugins_current_sink->range);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
