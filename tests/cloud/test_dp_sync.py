"""Host regression tests for DP sync submission and retry behavior.

Run with Python 3 and a C compiler, e.g. CC='cc' python tests/cloud/test_dp_sync.py.
The production sync function is compiled with fake transport/schema services.
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/tuya_cloud_service/schema/tuya_iot_dp.c"
COMPILER = shlex.split(os.environ.get("CC", "cc"))

PREFIX = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define OPRT_OK 0
#define LOOP_ONCE 0
#define PR_ERR(...) (++errors)
typedef struct { int unused; } dp_rept_valid_t;
typedef struct { struct { char *devid; } activate; } tuya_iot_client_t;
enum { EMPTY, DATA, DUMP_ERROR, ENCODING_FAILURE, REJECTED, OFFLINE };
static int scenario, reports, retries, dumps, errors, allocations;
static void *s_tmm_dp_sync, *accepted_data;
static void *allocate(size_t size) { ++allocations; return malloc(size); }
static void tal_free(void *ptr) { if (ptr) { --allocations; free(ptr); } }
static bool tuya_iot_is_connected(void) { return scenario != OFFLINE; }
static int tal_workq_start_delayed(void *handle, int delay, int loop) {
    (void)handle; assert(delay == 5000); assert(loop == LOOP_ONCE); ++retries; return OPRT_OK;
}
static int dp_obj_dump_stat_local_json(char *devid, dp_rept_valid_t **valid, char **json, int flags) {
    (void)devid; assert(flags == 0); ++dumps;
    /* Real schema contract: no dirty DPs returns OK without assigning outputs. */
    if (scenario == EMPTY) return OPRT_OK;
    if (scenario == DUMP_ERROR) return -1;
    *valid = allocate(sizeof(**valid));
    if (scenario != ENCODING_FAILURE) { *json = allocate(3); (*json)[0] = '{'; (*json)[1] = '}'; (*json)[2] = 0; }
    return OPRT_OK;
}
static void dp_sync_cb(int result, void *data) { (void)result; tal_free(data); }
static int tuya_iot_dp_report_json_async(tuya_iot_client_t *client, const char *json, const char *time,
                                       void (*callback)(int, void *), void *data, int timeout) {
    (void)client; assert(time == NULL); assert(callback == dp_sync_cb); assert(timeout == 5000); ++reports;
    if (!json || scenario == REJECTED) return -1;
    accepted_data = data;
    return OPRT_OK;
}
"""

SUFFIX = r"""
int main(int argc, char **argv) {
    assert(argc == 2);
    scenario = atoi(argv[1]);
    tuya_iot_client_t client = {{"test-device"}};
    tuya_iot_dp_sync_process(&client);
    switch (scenario) {
    case EMPTY:
        assert(reports == 0 && retries == 0 && errors == 0);
        /* A later tick with no new DP must also remain idle. */
        tuya_iot_dp_sync_process(&client);
        assert(reports == 0 && retries == 0 && errors == 0 && dumps == 2);
        break;
    case DATA:
        assert(reports == 1 && retries == 0 && allocations == 1 && accepted_data != NULL);
        dp_sync_cb(OPRT_OK, accepted_data);
        break;
    case DUMP_ERROR:
        assert(reports == 0 && retries == 1 && errors == 1);
        break;
    case ENCODING_FAILURE:
    case REJECTED:
        assert(reports == 1 && retries == 1 && accepted_data == NULL);
        break;
    case OFFLINE:
        assert(dumps == 0 && reports == 0 && retries == 1);
        break;
    default:
        return 2;
    }
    assert(allocations == 0);
    return 0;
}
"""


@unittest.skipUnless(COMPILER and shutil.which(COMPILER[0]), "Set CC to an available C compiler")
class DpSyncTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        source = Path(os.environ.get("DP_SYNC_SOURCE", SOURCE)).read_text(encoding="utf-8")
        function = source[source.index("void tuya_iot_dp_sync_process(void *data)"):]
        function = function[:function.index("\n/**")]
        harness = Path(cls.temp.name) / "dp_sync.c"
        harness.write_text(PREFIX + function + SUFFIX, encoding="utf-8")
        cls.exe = Path(cls.temp.name) / ("dp_sync.exe" if os.name == "nt" else "dp_sync")
        subprocess.run(COMPILER + ["-std=c99", "-Wall", "-Wextra", "-Werror", str(harness), "-o", str(cls.exe)],
                       check=True, timeout=120)

    def test_sync_outcomes(self):
        for case, name in enumerate(("empty", "data", "dump error", "encoding failure", "rejected", "offline")):
            with self.subTest(outcome=name):
                subprocess.run([str(self.exe), str(case)], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
