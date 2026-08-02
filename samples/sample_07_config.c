/*
 * 07 - Configuration files
 *
 * Shows: CMUTIL_Config typed accessors, Save and CMUTIL_ConfigLoad.
 *
 * The store is backed by a plain "key = value" properties file; the sample
 * reads the one shipped in samples/conf/sample_app.conf and writes an
 * updated copy into the working directory.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.config")

static const char *OUTPUT = "sample_generated.conf";

static void dump(CMUTIL_Config *conf)
{
    CMLogInfo("server.host = %s", CMCall(conf, Get, "server.host"));
    CMLogInfo("server.port = %ld", CMCall(conf, GetLong, "server.port"));
    CMLogInfo("server.tls  = %s",
              CMCall(conf, GetBoolean, "server.tls") ? "true" : "false");
    CMLogInfo("server.load = %f", CMCall(conf, GetDouble, "server.load"));
    CMLogInfo("app.name    = %s", CMCall(conf, Get, "app.name"));
}

static void sample_load(void)
{
    const char *path = SAMPLE_DATA("sample_app.conf");
    CMUTIL_Config *conf;

    SAMPLE_SECTION("loading a properties file");

    conf = CMUTIL_ConfigLoad(path);
    if (conf == NULL) {
        CMLogWarn("cannot load %s", path);
        return;
    }

    dump(conf);

    /* A missing key reads back as NULL / 0 / CMFalse. */
    CMLogInfo("missing key -> %p", (void*)CMCall(conf, Get, "does.not.exist"));

    CMCall(conf, Destroy);
}

static void sample_create_and_save(void)
{
    CMUTIL_Config *conf;
    CMUTIL_File *file;
    CMUTIL_String *contents;

    SAMPLE_SECTION("building and saving a configuration");

    conf = CMUTIL_ConfigCreate();
    CMCall(conf, Set, "server.host", "0.0.0.0");
    CMCall(conf, SetLong, "server.port", 8080L);
    CMCall(conf, SetBoolean, "server.tls", CMTrue);
    CMCall(conf, SetDouble, "server.load", 0.75);
    CMCall(conf, Set, "app.name", "generated-sample");

    CMCall(conf, Save, OUTPUT);
    CMCall(conf, Destroy);

    file = CMUTIL_FileCreate(OUTPUT);
    contents = CMCall(file, GetContents);
    if (contents) {
        CMLogInfo("%s:\n%s", OUTPUT, CMCall(contents, GetCString));
        CMCall(contents, Destroy);
    }

    SAMPLE_SECTION("reading it back");
    conf = CMUTIL_ConfigLoad(OUTPUT);
    if (conf) {
        dump(conf);
        CMCall(conf, Destroy);
    }

    /* Clean up after ourselves so repeated runs start fresh. */
    CMCall(file, Delete);
    CMCall(file, Destroy);
}

int main(void)
{
    sample_init();

    sample_load();
    sample_create_and_save();

    return sample_exit(0);
}
