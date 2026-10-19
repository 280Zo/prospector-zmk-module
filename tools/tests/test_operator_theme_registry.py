"""Compile/run the real palette registry without Zephyr, ignored fixtures or downloads."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
OP = ROOT / "boards/shields/prospector_adapter/src/layouts/operator"


class RegistryTests(unittest.TestCase):
    def compile_run(self, body, entries=None, start=0, valid=True, flags=()):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            test = tmp / "test.c"
            test.write_text(
                '#include <assert.h>\n#include <string.h>\n#include "operator_theme.h"\n'
                "int main(void) {\n" + body + "\nreturn 0; }\n"
            )
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(OP)]
            if entries is not None:
                registry = tmp / "registry.h"
                values = ",".join(
                    '{.id="'
                    + ident
                    + '",.palette={.name="'
                    + name
                    + '",.wpm_text='
                    + color
                    + "}}"
                    for ident, name, color in entries
                )
                registry.write_text(
                    f"#define OPERATOR_THEME_REGISTRY_VERSION 1\n"
                    f"#define OPERATOR_THEME_REGISTRY_START {start}\n"
                    f"#define OPERATOR_THEME_REGISTRY_ENTRIES {values}\n"
                )
                command += ["-include", str(registry)]
            binary = tmp / "verify"
            result = subprocess.run(
                command
                + list(flags)
                + [str(test), str(OP / "operator_theme.c"), "-o", str(binary)],
                capture_output=True,
                text=True,
            )
            if not valid:
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                return
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_no_registry_retains_six_and_configured_start(self):
        self.compile_run(
            "assert(operator_theme_count() == 6); assert(operator_theme_index() == 3);"
            'assert(strcmp(operator_theme_id_at(3), "synthwave") == 0);',
            flags=["-DCONFIG_PROSPECTOR_OPERATOR_THEME_SYNTHWAVE=1"],
        )

    def test_selected_subset_and_named_custom(self):
        self.compile_run(
            "assert(operator_theme_count() == 2); assert(operator_theme_index() == 1);"
            'assert(strcmp(operator_theme_current()->name, "Ocean") == 0);'
            'size_t i=99; assert(operator_theme_find_id("ember", &i) && i==0);'
            'assert(!operator_theme_find_id("default", &i));'
            "operator_theme_select(0); assert(operator_theme_current()->wpm_text==0xff8800);",
            [
                ("ember", "Ember", "0xff8800"),
                ("00000000-0000-4000-8000-000000000001", "Ocean", "0x123456"),
            ],
            1,
        )

    def test_custom_only_and_bounds(self):
        self.compile_run(
            "assert(operator_theme_count()==1); operator_theme_select(900);"
            "assert(operator_theme_index()==0); assert(operator_theme_at(900)==operator_theme_at(0));",
            [("00000000-0000-4000-8000-000000000001", "Only", "0x123456")],
        )

    def test_empty_registry_and_invalid_start_fail_compilation(self):
        self.compile_run("", [], valid=False)
        self.compile_run("", [("default", "Default", "0")], start=1, valid=False)

    def test_legacy_custom_cannot_silently_override_generated_selection(self):
        self.compile_run(
            "",
            [("default", "Default", "0")],
            valid=False,
            flags=["-DCONFIG_PROSPECTOR_OPERATOR_CUSTOM_THEME=1"],
        )

    def test_state_roundtrip_and_recolour(self):
        self.compile_run(
            "uint8_t state[OPERATOR_THEME_STATE_SIZE]; operator_theme_select(1);"
            "operator_theme_encode_state(state); assert(state[0]==1);"
            'assert(strcmp((char *)&state[1], "default")==0);'
            'assert(strcmp((char *)&state[38], "ember")==0);'
            "assert(operator_theme_resolve_state(state, sizeof(state))==1);",
            [("default", "Default", "0"), ("ember", "Renamed", "0xabc123")],
        )

    def test_restore_after_reorder_or_rename(self):
        self.compile_run(
            "uint8_t state[OPERATOR_THEME_STATE_SIZE]={1};"
            'strcpy((char *)&state[1], "default"); strcpy((char *)&state[38], "ember");'
            "assert(operator_theme_resolve_state(state, sizeof(state))==0);",
            [("ember", "Renamed", "0"), ("default", "Default", "0")],
            1,
        )

    def test_removed_theme_and_changed_start_fall_back(self):
        self.compile_run(
            "uint8_t state[OPERATOR_THEME_STATE_SIZE]={1};"
            'strcpy((char *)&state[1], "default"); strcpy((char *)&state[38], "gone");'
            "assert(operator_theme_resolve_state(state, sizeof(state))==1);"
            'strcpy((char *)&state[1], "ember"); strcpy((char *)&state[38], "ember");'
            "assert(operator_theme_resolve_state(state, sizeof(state))==1);",
            [("ember", "Ember", "0"), ("default", "Default", "0")],
            1,
        )

    def test_legacy_truncated_unknown_unterminated_state_ignored(self):
        self.compile_run(
            "uint8_t state[OPERATOR_THEME_STATE_SIZE]={1};"
            "assert(operator_theme_resolve_state(state, 2)==0);"
            "assert(operator_theme_resolve_state(state, 74)==0);"
            "assert(operator_theme_resolve_state(NULL, 75)==0);"
            "state[0]=2; assert(operator_theme_resolve_state(state, 75)==0);"
            "memset(state, 65, sizeof(state)); state[0]=1;"
            "assert(operator_theme_resolve_state(state, 75)==0);"
        )


class SettingsStoreTests(unittest.TestCase):
    def test_real_settings_handler_still_debounces_and_restores(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp / "zephyr/settings").mkdir(parents=True)
            (tmp / "zephyr/kernel.h").write_text("""#pragma once
#include <errno.h>
#define IS_ENABLED(x) (x)
#define K_MSEC(x) (x)
struct k_work { int unused; };
struct k_work_delayable { void (*handler)(struct k_work *); };
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {fn}
int k_work_reschedule(struct k_work_delayable *, int);
""")
            (tmp / "zephyr/settings/settings.h").write_text("""#pragma once
#include <sys/types.h>
typedef ssize_t (*settings_read_cb)(void *, void *, size_t);
int settings_save_one(const char *, const void *, size_t);
#define SETTINGS_STATIC_HANDLER_DEFINE(a,b,c,d,e,f)
""")
            source = tmp / "store.c"
            source.write_text("""#include <assert.h>
#include <string.h>
#include "operator_theme_store.c"
static int now, due, saves;
static struct k_work_delayable *pending;
static uint8_t saved[OPERATOR_THEME_STATE_SIZE];
int k_work_reschedule(struct k_work_delayable *w, int ms) {
  assert(ms==3000); pending=w; due=now+ms; return 0;
}
int settings_save_one(const char *name, const void *data, size_t len) {
  assert(strcmp(name,"prospector/theme/state")==0);
  assert(len==75); memcpy(saved,data,len); saves++; return 0;
}
void operator_theme_restore(size_t index) { operator_theme_select(index); }
static void advance(int ms) { now+=ms; if(pending && now>=due) {
  struct k_work_delayable *w=pending; pending=0; w->handler(0); } }
static ssize_t read_saved(void *arg, void *data, size_t len) {
  (void)arg; memcpy(data,saved,len); return (ssize_t)len;
}
int main(void) {
  operator_theme_select(1); operator_theme_store_schedule(); advance(1000);
  operator_theme_select(2); operator_theme_store_schedule(); advance(2999);
  assert(saves==0); advance(1); assert(saves==1);
  assert(saved[0]==1 && strcmp((char *)&saved[1],"default")==0);
  assert(strcmp((char *)&saved[38],"glacier")==0);
  operator_theme_select(0);
  assert(theme_settings_set("state",75,read_saved,0)==0);
  assert(operator_theme_index()==2);
  operator_theme_select(0);
  assert(theme_settings_set("state",2,read_saved,0)==0);
  assert(operator_theme_index()==0);
  return 0;
}
""")
            binary = tmp / "verify"
            result = subprocess.run(
                [
                    "cc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DCONFIG_SETTINGS=1",
                    "-I",
                    str(tmp),
                    "-I",
                    str(OP),
                    str(source),
                    str(OP / "operator_theme.c"),
                    "-o",
                    str(binary),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
