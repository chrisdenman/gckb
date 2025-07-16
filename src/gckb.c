#include <gckb.h>
#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHEMA_ID__MEDIA_KEYS "org.gnome.settings-daemon.plugins.media-keys"
#define SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"
#define PATH_PREFIX "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"

#define APP_NAME "gckb"

#define KEY__CUSTOM_KEYBINDINGS "custom-keybindings"
#define KEY__NAME "name"
#define KEY__COMMAND "command"
#define KEY__BINDING "binding"

#define FS__ERROR__NO_VERB "No verb supplied.\n"
#define FS__ERROR__UNKNOWN_VERB "Unknown verb '%s'.\n"
#define FS__ERROR__USAGE "Usage:\n  %s add <name> <command> <binding>\n  %s delete [<index>]\n  %s list\nTry 'man %s' for more information."

#define FS__ERROR__LIST__NO_ARGUMENTS_EXPECTED "No arguments expected.\n"
#define FS__ERROR__LIST__USAGE "Usage:\n  %s list\nTry 'man %s' for more information."

#define FS__ERROR__DELETE__INVALID_INDEX "Invalid index: '%d'.\n"
#define FS__ERROR__DELETE__0_OR_1_ARGUMENTS_EXPECTED "0 or 1 arguments expected.\n"
#define FS__ERROR__DELETE__USAGE "Usage:\n  %s delete [<index>]\nTry 'man %s' for more information."

#define FS__ERROR__ADD__INVALID_KEYBOARD_BINDING "Invalid keyboard binding '%s'.\n"
#define FS__ERROR__ADD__DUPLICATE_NAME "A custom keyboard binding with that name already exists.\n"
#define FS__ERROR__ADD__DUPLICATE_BINDING "A custom keyboard binding with that binding already exists.\n"
#define FS__ERROR__ADD__3_ARGUMENTS_EXPECTED "3 arguments expected.\n"
#define FS__ERROR__ADD__USAGE "Usage:\n  %s add <name> <command> <binding>\nTry 'man %s' for more information."

#define VERB__LIST "list"
#define VERB__ADD "add"
#define VERB__DELETE "delete"

static const char *valid_modifiers[] = {
    "Ctrl", "Alt", "Shift", "Super", "Primary", "Meta", NULL
};

typedef struct {
    gchar *path;
    gchar *name;
    gchar *command;
    gchar *binding;
} Binding;

gboolean str_equal(const char *str1, const char *str2) {
    return g_strcmp0(str1, str2) == 0;
}

static gchar **getDefinedBindingPaths(GSettings *settings) {
    return g_settings_get_strv(settings, KEY__CUSTOM_KEYBINDINGS);
}

static Binding *loadBinding(const gchar *path) {
    Binding *b = g_new0(Binding, 1);
    b->path = g_strdup(path);
    GSettings *s = g_settings_new_with_path(SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING, path);
    b->name = g_settings_get_string(s, KEY__NAME);
    b->command = g_settings_get_string(s, KEY__COMMAND);
    b->binding = g_settings_get_string(s, KEY__BINDING);
    g_object_unref(s);
    return b;
}

static void freeBinding(Binding *binding) {
    g_free(binding->path);
    g_free(binding->name);
    g_free(binding->command);
    g_free(binding->binding);
    g_free(binding);
}

// @todo this logic looks broken
static gboolean isValidModifier(const gchar *modifier) {
    gboolean isValid = FALSE;
    for (int i = 0; !isValid && valid_modifiers[i]; ++i) {
        isValid |= str_equal(modifier, valid_modifiers[i]);
    }

    return isValid;
}

static gboolean isValidBinding(const gchar *binding) {
    if (!binding || *binding == '\0') return FALSE;

    GHashTable *seen_mods = g_hash_table_new(g_str_hash, g_str_equal);
    const gchar *p = binding;
    gboolean found_modifier = FALSE;

    while (*p) {
        if (*p == '<') {
            const gchar *end = strchr(p, '>');
            if (!end) goto fail;

            gchar *mod = g_strndup(p + 1, end - p - 1);
            if (!isValidModifier(mod) || g_hash_table_contains(seen_mods, mod)) {
                g_free(mod);
                goto fail;
            }
            g_hash_table_add(seen_mods, mod);
            found_modifier = TRUE;
            p = end + 1;
        } else {
            break;
        }
    }

    g_hash_table_destroy(seen_mods);
    return found_modifier && *p != '\0';

fail:
    g_hash_table_destroy(seen_mods);
    return FALSE;
}

static void resetBinding(const gchar *path) {
    GSettings *s = g_settings_new_with_path(SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING, path);
    g_settings_reset(s, KEY__NAME);
    g_settings_reset(s, KEY__COMMAND);
    g_settings_reset(s, KEY__BINDING);
    g_settings_sync();
    g_object_unref(s);
}

static int list() {
    GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

    gchar **paths = getDefinedBindingPaths(mediaKeysSettings);
    for (gsize i = 0; paths[i]; ++i) {
        Binding *b = loadBinding(paths[i]);
        g_print("%zu\t%s\t%s\t%s\n", i, b->name, b->command, b->binding);
        freeBinding(b);
    }
    g_strfreev(paths);

    g_object_unref(mediaKeysSettings);

    return EXIT_SUCCESS;
}

static int add(
    const gchar *name,
    const gchar *command,
    const gchar *binding
) {
    int result = EXIT_FAILURE;

    if (isValidBinding(binding)) {
        GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

        gchar **paths = getDefinedBindingPaths(mediaKeysSettings);

        gboolean errorFound = FALSE;
        for (gsize bindingPathIndex = 0; !errorFound && paths[bindingPathIndex]; ++bindingPathIndex) {
            Binding *_binding = loadBinding(paths[bindingPathIndex]);

            if (str_equal(_binding->name, name)) {
                g_printerr(FS__ERROR__ADD__DUPLICATE_NAME);
                errorFound = TRUE;
            }

            if (str_equal(_binding->binding, binding)) {
                g_printerr(FS__ERROR__ADD__DUPLICATE_BINDING);
                errorFound = TRUE;
            }

            freeBinding(_binding);
        }

        if (errorFound) {
            g_strfreev(paths);
        } else {
            int index = 0;
            gchar *new_path = NULL;
            do {
                g_free(new_path);
                new_path = g_strdup_printf(PATH_PREFIX "custom%d/", index++);
                // ReSharper disable once CppRedundantCastExpression
            } while (g_strv_contains((const gchar * const*) paths, new_path));

            const gsize len = g_strv_length(paths);
            gchar **updated = g_malloc0(sizeof(gchar *) * (len + 2));
            for (gsize i = 0; i < len; i++) updated[i] = g_strdup(paths[i]);
            updated[len] = g_strdup(new_path);
            updated[len + 1] = NULL;
            // ReSharper disable once CppRedundantCastExpression
            g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const*) updated);

            GSettings *new_binding = g_settings_new_with_path(SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING, new_path);
            g_settings_set_string(new_binding, KEY__NAME, name);
            g_settings_set_string(new_binding, KEY__COMMAND, command);
            g_settings_set_string(new_binding, KEY__BINDING, binding);

            g_settings_sync();
            g_object_unref(new_binding);

            g_free(new_path);
            g_strfreev(paths);
            g_strfreev(updated);

            g_object_unref(mediaKeysSettings);
            result = EXIT_SUCCESS;
        }
    } else {
        g_printerr(FS__ERROR__ADD__INVALID_KEYBOARD_BINDING, binding);
    }

    return result;
}

static int deleteAll() {
    GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

    gchar **paths = getDefinedBindingPaths(mediaKeysSettings);
    const gsize total = g_strv_length(paths);
    for (gsize i = 0; i < total; ++i) {
        resetBinding(paths[i]);
    }
    g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const[]){NULL});
    g_settings_sync();
    g_strfreev(paths);

    g_object_unref(mediaKeysSettings);

    return EXIT_SUCCESS;
}

static int deleteWithIndex(const long int bindingIndex) {
    int result = EXIT_FAILURE;

    if (bindingIndex < 0) {
        g_printerr(FS__ERROR__DELETE__INVALID_INDEX, bindingIndex);
    } else {
        GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

        gchar **paths = getDefinedBindingPaths(mediaKeysSettings);

        const gsize numberOfCustomKeybindings = g_strv_length(paths);

        if ((gsize) bindingIndex >= numberOfCustomKeybindings) {
            g_printerr(FS__ERROR__DELETE__INVALID_INDEX, bindingIndex);
            g_strfreev(paths);
        } else {
            GPtrArray *new_paths = g_ptr_array_new_with_free_func(g_free);
            for (gsize i = 0; i < numberOfCustomKeybindings; ++i) {
                if (i == bindingIndex) {
                    resetBinding(paths[i]);
                } else {
                    g_ptr_array_add(new_paths, g_strdup(paths[i]));
                }
            }

            g_ptr_array_add(new_paths, NULL);
            g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const *) new_paths->pdata);
            g_settings_sync();
            g_ptr_array_free(new_paths, TRUE);
            g_strfreev(paths);

            result = EXIT_SUCCESS;
        }
        g_object_unref(mediaKeysSettings);
    }

    return result;
}

int main(const int numberOfArgs, char *argv[]) {
    int result = EXIT_FAILURE;

    const int numberOfUserSuppliedArguments = numberOfArgs - 1;
    if (numberOfUserSuppliedArguments == 0) {
        g_printerr(FS__ERROR__NO_VERB);
        g_printerr(FS__ERROR__USAGE, APP_NAME, APP_NAME, APP_NAME, APP_NAME);
    } else {
        const gchar *verb = argv[1];

        const int numberOfCommandArguments = numberOfUserSuppliedArguments - 1;
        if (str_equal(verb, VERB__LIST)) {
            if (numberOfCommandArguments == 0) {
                result = list();
            } else {
                g_printerr(FS__ERROR__LIST__NO_ARGUMENTS_EXPECTED);
                g_printerr(FS__ERROR__LIST__USAGE, APP_NAME, APP_NAME);
            }
        } else if (str_equal(verb, VERB__ADD)) {
            if (numberOfCommandArguments == 3) {
                result = add(argv[2], argv[3], argv[4]);
            } else {
                g_printerr(FS__ERROR__ADD__3_ARGUMENTS_EXPECTED);
                g_printerr(FS__ERROR__ADD__USAGE, APP_NAME, APP_NAME);
            }
        } else if (str_equal(verb, VERB__DELETE)) {
            const int numberOfDeleteIndices = numberOfArgs - 2;
            switch (numberOfDeleteIndices) {
                case 0:
                    result = deleteAll();
                    break;
                case 1:
                    const long int bindingIndex = strtol(argv[2], NULL, 10);
                    result = deleteWithIndex(bindingIndex); // @todo need to parse this properly, pcre2 perhaps?
                    break;
                default:
                    g_printerr(FS__ERROR__DELETE__0_OR_1_ARGUMENTS_EXPECTED);
                    g_printerr(FS__ERROR__DELETE__USAGE, APP_NAME, APP_NAME);
                    break;
            }
        } else {
            g_printerr(FS__ERROR__UNKNOWN_VERB, verb);
            g_printerr(FS__ERROR__USAGE, APP_NAME, APP_NAME, APP_NAME);
        }
    }

    return result;
}
