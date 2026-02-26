#include "skills/skill_loader.h"

#include "mimi_config.h"

#include "tal_fs.h"

#include <string.h>

static const char *TAG = "skills";

#define BUILTIN_WEATHER                                                                                                \
    "# Weather\n"                                                                                                      \
    "\n"                                                                                                               \
    "Get current weather and forecasts using web_search.\n"                                                            \
    "\n"                                                                                                               \
    "## When to use\n"                                                                                                 \
    "When the user asks about weather, temperature, or forecasts.\n"                                                   \
    "\n"                                                                                                               \
    "## How to use\n"                                                                                                  \
    "1. Use get_current_time to know the current date\n"                                                               \
    "2. Use web_search with a query like \"weather in [city] today\"\n"                                                \
    "3. Extract temperature, conditions, and forecast from results\n"                                                  \
    "4. Present in a concise, friendly format\n"                                                                       \
    "\n"                                                                                                               \
    "## Example\n"                                                                                                     \
    "User: \"What's the weather in Tokyo?\"\n"                                                                         \
    "-> get_current_time\n"                                                                                            \
    "-> web_search \"weather Tokyo today February 2026\"\n"                                                            \
    "-> \"Tokyo: 8C, partly cloudy. High 12C, low 4C. Light wind from the north.\"\n"

#define BUILTIN_DAILY_BRIEFING                                                                                         \
    "# Daily Briefing\n"                                                                                               \
    "\n"                                                                                                               \
    "Compile a personalized daily briefing for the user.\n"                                                            \
    "\n"                                                                                                               \
    "## When to use\n"                                                                                                 \
    "When the user asks for a daily briefing, morning update, or \"what's new today\".\n"                              \
    "Also useful as a heartbeat/cron task.\n"                                                                          \
    "\n"                                                                                                               \
    "## How to use\n"                                                                                                  \
    "1. Use get_current_time for today's date\n"                                                                       \
    "2. Read /spiffs/memory/MEMORY.md for user preferences and context\n"                                              \
    "3. Read today's daily note if it exists\n"                                                                        \
    "4. Use web_search for relevant news based on user interests\n"                                                    \
    "5. Compile a concise briefing covering:\n"                                                                        \
    "   - Date and time\n"                                                                                             \
    "   - Weather (if location known from USER.md)\n"                                                                  \
    "   - Relevant news/updates based on user interests\n"                                                             \
    "   - Any pending tasks from memory\n"                                                                             \
    "   - Any scheduled cron jobs\n"                                                                                   \
    "\n"                                                                                                               \
    "## Format\n"                                                                                                      \
    "Keep it brief - 5-10 bullet points max. Use the user's preferred language.\n"

#define BUILTIN_SKILL_CREATOR                                                                                          \
    "# Skill Creator\n"                                                                                                \
    "\n"                                                                                                               \
    "Create new skills for MimiClaw.\n"                                                                                \
    "\n"                                                                                                               \
    "## When to use\n"                                                                                                 \
    "When the user asks to create a new skill, teach the bot something, or add a new capability.\n"                    \
    "\n"                                                                                                               \
    "## How to create a skill\n"                                                                                       \
    "1. Choose a short, descriptive name (lowercase, hyphens ok)\n"                                                    \
    "2. Write a SKILL.md file with this structure:\n"                                                                  \
    "   - `# Title` - clear name\n"                                                                                    \
    "   - Brief description paragraph\n"                                                                               \
    "   - `## When to use` - trigger conditions\n"                                                                     \
    "   - `## How to use` - step-by-step instructions\n"                                                               \
    "   - `## Example` - concrete example (optional but helpful)\n"                                                    \
    "3. Save to `/spiffs/skills/<name>.md` using write_file\n"                                                         \
    "4. The skill will be automatically available after the next conversation\n"                                       \
    "\n"                                                                                                               \
    "## Best practices\n"                                                                                              \
    "- Keep skills concise - the context window is limited\n"                                                          \
    "- Focus on WHAT to do, not HOW (the agent is smart)\n"                                                            \
    "- Include specific tool calls the agent should use\n"                                                             \
    "- Test by asking the agent to use the new skill\n"                                                                \
    "\n"                                                                                                               \
    "## Example\n"                                                                                                     \
    "To create a \"translate\" skill:\n"                                                                               \
    "write_file path=\"/spiffs/skills/translate.md\" content=\"# Translate\\n\\nTranslate text between "               \
    "languages.\\n\\n"                                                                                                 \
    "## When to use\\nWhen the user asks to translate text.\\n\\n"                                                     \
    "## How to use\\n1. Identify source and target languages\\n"                                                       \
    "2. Translate directly using your language knowledge\\n"                                                           \
    "3. For specialized terms, use web_search to verify\\n\"\n"

typedef struct {
    const char *filename;
    const char *content;
} builtin_skill_t;

static const builtin_skill_t s_builtins[] = {
    {"weather", BUILTIN_WEATHER},
    {"daily-briefing", BUILTIN_DAILY_BRIEFING},
    {"skill-creator", BUILTIN_SKILL_CREATOR},
};

#define NUM_BUILTINS ((int)(sizeof(s_builtins) / sizeof(s_builtins[0])))

static void install_builtin(const builtin_skill_t *skill)
{
    if (!skill || !skill->filename || !skill->content) {
        return;
    }

    char path[160] = {0};
    snprintf(path, sizeof(path), "%s%s.md", MIMI_SKILLS_PREFIX, skill->filename);

    BOOL_T exists = FALSE;
    if (tal_fs_is_exist(path, &exists) == OPRT_OK && exists) {
        MIMI_LOGD(TAG, "skill exists: %s", path);
        return;
    }

    TUYA_FILE f = tal_fopen(path, "w");
    if (!f) {
        MIMI_LOGE(TAG, "cannot write skill: %s", path);
        return;
    }

    int wn = tal_fwrite((void *)skill->content, (int)strlen(skill->content), f);
    tal_fclose(f);
    if (wn < 0) {
        MIMI_LOGE(TAG, "write skill failed: %s", path);
        return;
    }

    MIMI_LOGI(TAG, "installed built-in skill: %s", path);
}

OPERATE_RET skill_loader_init(void)
{
    BOOL_T exists = FALSE;
    if (tal_fs_is_exist(MIMI_SPIFFS_SKILLS_DIR, &exists) != OPRT_OK || !exists) {
        int mk_rt = tal_fs_mkdir(MIMI_SPIFFS_SKILLS_DIR);
        if (mk_rt != OPRT_OK) {
            MIMI_LOGE(TAG, "mkdir failed: %s rt=%d", MIMI_SPIFFS_SKILLS_DIR, mk_rt);
            return mk_rt;
        }
    }

    for (int i = 0; i < NUM_BUILTINS; i++) {
        install_builtin(&s_builtins[i]);
    }

    MIMI_LOGI(TAG, "skills ready (%d built-in)", NUM_BUILTINS);
    return OPRT_OK;
}

static const char *extract_title(const char *line, size_t len, char *out, size_t out_size)
{
    const char *start = line;
    if (!line || !out || out_size == 0) {
        return "";
    }

    if (len >= 2 && line[0] == '#' && line[1] == ' ') {
        start = line + 2;
        len -= 2;
    }

    while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r' || start[len - 1] == ' ')) {
        len--;
    }

    size_t copy = len < out_size - 1 ? len : out_size - 1;
    memcpy(out, start, copy);
    out[copy] = '\0';
    return out;
}

static void extract_description(TUYA_FILE f, char *out, size_t out_size)
{
    if (!f || !out || out_size == 0) {
        return;
    }

    size_t off       = 0;
    char   line[256] = {0};

    while (tal_fgets(line, sizeof(line), f) && off < out_size - 1) {
        size_t len = strlen(line);

        if (len == 0 || (len == 1 && line[0] == '\n') || (len >= 2 && line[0] == '#' && line[1] == '#')) {
            break;
        }

        if (off == 0 && line[0] == '\n') {
            continue;
        }

        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = ' ';
        }

        size_t copy = len < out_size - off - 1 ? len : out_size - off - 1;
        memcpy(out + off, line, copy);
        off += copy;
    }

    while (off > 0 && out[off - 1] == ' ') {
        off--;
    }
    out[off] = '\0';
}

size_t skill_loader_build_summary(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return 0;
    }

    TUYA_DIR dir = NULL;
    if (tal_dir_open(MIMI_SPIFFS_SKILLS_DIR, &dir) != OPRT_OK || !dir) {
        MIMI_LOGW(TAG, "cannot open skills dir: %s", MIMI_SPIFFS_SKILLS_DIR);
        buf[0] = '\0';
        return 0;
    }

    size_t off = 0;

    while (off < size - 1) {
        TUYA_FILEINFO info = NULL;
        if (tal_dir_read(dir, &info) != OPRT_OK || !info) {
            break;
        }

        const char *name = NULL;
        if (tal_dir_name(info, &name) != OPRT_OK || !name) {
            continue;
        }

        size_t name_len = strlen(name);
        if (name_len < 4 || strcmp(name + name_len - 3, ".md") != 0) {
            continue;
        }

        char full_path[256] = {0};
        snprintf(full_path, sizeof(full_path), "%s/%s", MIMI_SPIFFS_SKILLS_DIR, name);

        TUYA_FILE f = tal_fopen(full_path, "r");
        if (!f) {
            continue;
        }

        char first_line[128] = {0};
        if (!tal_fgets(first_line, sizeof(first_line), f)) {
            tal_fclose(f);
            continue;
        }

        char title[64] = {0};
        (void)extract_title(first_line, strlen(first_line), title, sizeof(title));

        char desc[256] = {0};
        extract_description(f, desc, sizeof(desc));
        tal_fclose(f);

        off += (size_t)snprintf(buf + off, size - off, "- **%s**: %s (read with: read_file %s)\n", title,
                                desc[0] ? desc : "(no description)", full_path);
    }

    tal_dir_close(dir);

    if (off >= size) {
        off = size - 1;
    }
    buf[off] = '\0';

    MIMI_LOGI(TAG, "skills summary bytes=%u", (unsigned)off);
    return off;
}
