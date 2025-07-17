#include <gckb.h>
#include <gio/gio.h>
#include <glib.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHEMA_ID__MEDIA_KEYS "org.gnome.settings-daemon.plugins.media-keys"
#define SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"
#define PATH_PREFIX "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"

#define MAX_DELETE_INDEX_NUM_DIGITS 4

#define APP_NAME "gckb"

#define KEY__CUSTOM_KEYBINDINGS "custom-keybindings"
#define KEY__NAME "name"
#define KEY__COMMAND "command"
#define KEY__BINDING "binding"

#define FS__ERROR__NO_VERB "No verb supplied.\n"
#define FS__ERROR__UNKNOWN_VERB "Unknown verb '%s'.\n"
#define FS__ERROR__USAGE(APP_NAME) "Usage:\n  " APP_NAME " add <name> <command> <binding>\n  " APP_NAME " delete [<index>]\n  " APP_NAME " list\nTry 'man " APP_NAME "' for more information.\n"

#define FS__ERROR__LIST__NO_ARGUMENTS_EXPECTED "List does not take arguments.\n"
#define FS__ERROR__LIST__USAGE "Usage:\n  %s list\nTry 'man %s' for more information.\n"
#define FS__LIST__BINDING_LINE "%zu\t%s\t%s\t%s\n"

#define FS__ERROR__DELETE__TOO_MAN_INDEX_CHARACTERS "Invalid index only %d characters allowed.\n"
#define FS__ERROR__DELETE__INVALID_INDEX "Invalid index: '%s'.\n"
#define FS__ERROR__DELETE__INVALID_INDEX_OUT_OF_BOUNDS "Index out of bounds: '%zu'.\n"
#define FS__ERROR__DELETE__0_OR_1_ARGUMENTS_EXPECTED "Delete expects 0 or 1 arguments.\n"
#define FS__ERROR__DELETE__USAGE "Usage:\n  %s delete [<index>]\nTry 'man %s' for more information.\n"
#define DELETE__INDEX__REGEX "^(([0-9])|([1-9][0-9]*))$"

#define FS__ERROR__ADD__INVALID_KEYBOARD_BINDING "Invalid keyboard binding '%s'.\n"
#define FS__ERROR__ADD__DUPLICATE_NAME "A custom keyboard binding with that name already exists.\n"
#define FS__ERROR__ADD__DUPLICATE_BINDING "A custom keyboard binding with that binding already exists.\n"
#define FS__ERROR__ADD__3_ARGUMENTS_TO_ADD_EXPECTED "Add expects 3 arguments.\n"
#define FS__ERROR__ADD__USAGE "Usage:\n  %s add <name> <command> <binding>\nTry 'man %s' for more information.\n"

#define VERB__LIST "list"
#define VERB__ADD "add"
#define VERB__DELETE "delete"

#define MODIFIER__CTRL "Ctrl"
#define MODIFIER__ALT "Alt"
#define MODIFIER__SHIFT "Shift"
#define MODIFIER__SUPER "Super"
#define MODIFIER__PRIMARY "Primary"
#define MODIFIER__META "Meta"

/**
 * A struct to encapsulate the information stored in an instance of the
 * 'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding' schema.
 */
typedef struct {
    gchar *path; /* The 'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding' schema's path */
    gchar *name; /* The name of the custom key binding */
    gchar *command; /* The command to execute upon activation */
    gchar *binding; /* The keyboard binding which triggers command execution */
} CustomKeyBinding;

/**
 * Ann array of valid keyboard modifier keys as interpreted by GSettings.
 */
static const char *validKeyboardModifierRepresentations[] = {
    MODIFIER__CTRL, MODIFIER__ALT, MODIFIER__SHIFT, MODIFIER__SUPER, MODIFIER__PRIMARY, NULL
};

/**
 * Loads the custom keybinding settings into a CustomKeyBinding struct to encapsulate them.
 *
 * The caller of the function becomes owner of the CustomKeyBinding struct returned.
 *
 * @param path the custom key binding schema path
 *
 * @return a CustomKeyBinding struct containing the settings requested
 */
static CustomKeyBinding *loadCustomKeyBinding(const gchar *path) {
    CustomKeyBinding *customKeyBinding = g_new0(CustomKeyBinding, 1);
    customKeyBinding->path = g_strdup(path);
    GSettings *customKeyBindingSchemaSettings =
            g_settings_new_with_path(SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING, path);
    customKeyBinding->name = g_settings_get_string(customKeyBindingSchemaSettings, KEY__NAME);
    customKeyBinding->command = g_settings_get_string(customKeyBindingSchemaSettings, KEY__COMMAND);
    customKeyBinding->binding = g_settings_get_string(customKeyBindingSchemaSettings, KEY__BINDING);
    g_object_unref(customKeyBindingSchemaSettings);

    return customKeyBinding;
}

/**
 * Frees the members of and, a CustomKeyBinding.
 *
 * @param customKeyBinding the binding to free
 */
static void freeBinding(CustomKeyBinding *customKeyBinding) {
    g_free(customKeyBinding->path);
    g_free(customKeyBinding->name);
    g_free(customKeyBinding->command);
    g_free(customKeyBinding->binding);
    g_free(customKeyBinding);
}

/**
 * Compares two NULL terminated UTF-8 strings, returns true iff they are both non-null pointers and point to identical
 * strings.
 *
 * All data are owned by the caller.
 *
 * @param str1 the first string to compare
 * @param str2 the second string to compare
 *
 * @return true iff the two pointers point to identical, non-NULL strings
 */
gboolean str_equal(const char *str1, const char *str2) {
    return g_strcmp0(str1, str2) == 0;
}

static gchar **getDefinedBindingPaths(GSettings *settings) {
    return g_settings_get_strv(settings, KEY__CUSTOM_KEYBINDINGS);
}

static gboolean isValidModifier(const gchar *modifier) {
    gboolean isValid = FALSE;
    for (int i = 0; !isValid && validKeyboardModifierRepresentations[i]; ++i) {
        isValid |= str_equal(modifier, validKeyboardModifierRepresentations[i]);
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
            CustomKeyBinding *customKeyBinding = loadCustomKeyBinding(paths[bindingPathIndex]);

            if (str_equal(customKeyBinding->name, name)) {
                g_printerr(FS__ERROR__ADD__DUPLICATE_NAME);
                errorFound = TRUE;
            }

            if (str_equal(customKeyBinding->binding, binding)) {
                g_printerr(FS__ERROR__ADD__DUPLICATE_BINDING);
                errorFound = TRUE;
            }

            freeBinding(customKeyBinding);
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
            for (gsize i = 0; i < len; i++) {
                updated[i] = g_strdup(paths[i]);
            }
            updated[len] = g_strdup(new_path);
            updated[len + 1] = NULL;
            // ReSharper disable once CppRedundantCastExpression
            g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const*) updated);

            GSettings *newBindingSettings =
                    g_settings_new_with_path(SCHEMA_ID__MEDIA_KEYS__CUSTOM_KEYBINDING, new_path);
            g_settings_set_string(newBindingSettings, KEY__NAME, name);
            g_settings_set_string(newBindingSettings, KEY__COMMAND, command);
            g_settings_set_string(newBindingSettings, KEY__BINDING, binding);
            g_settings_sync();
            g_object_unref(newBindingSettings);

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

/**
 * Deletes all custom keyboard bindings.
 *
 * @return EXIT_SUCCESS
 */
static int deleteAll() {
    GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

    gchar **paths = getDefinedBindingPaths(mediaKeysSettings);
    const gsize total = g_strv_length(paths);
    for (gsize index = 0; index < total; ++index) {
        resetBinding(paths[index]);
    }
    g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const[]){NULL});
    g_settings_sync();
    g_strfreev(paths);

    g_object_unref(mediaKeysSettings);

    return EXIT_SUCCESS;
}

/**
 * Deletes a custom keyboard binding by index.
 *
 * The index is the offset into the array contained in the  "org.gnome.settings-daemon.plugins.media-keys" schema's
 * "custom-keybindings" key.
 *
 * @param bindingIndex the index of the binding to delete
 *
 * @return EXIT_FAILURE if we are passed an invalid index else EXIT_SUCCESS
 */
static int deleteByIndex(const long int bindingIndex) {
    int result = EXIT_FAILURE;

    GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);
    gchar **customKeyBindingSchemaPaths = getDefinedBindingPaths(mediaKeysSettings);
    const gsize numberOfCustomKeybindings = g_strv_length(customKeyBindingSchemaPaths);

    if ((gsize) bindingIndex >= numberOfCustomKeybindings) {
        g_printerr(FS__ERROR__DELETE__INVALID_INDEX_OUT_OF_BOUNDS, bindingIndex);
        g_strfreev(customKeyBindingSchemaPaths);
    } else {
        GPtrArray *new_paths = g_ptr_array_new_with_free_func(g_free);
        for (gsize i = 0; i < numberOfCustomKeybindings; ++i) {
            if (i == bindingIndex) {
                resetBinding(customKeyBindingSchemaPaths[i]);
            } else {
                g_ptr_array_add(new_paths, g_strdup(customKeyBindingSchemaPaths[i]));
            }
        }

        g_ptr_array_add(new_paths, NULL);
        g_settings_set_strv(mediaKeysSettings, KEY__CUSTOM_KEYBINDINGS, (const gchar * const *) new_paths->pdata);
        g_settings_sync();
        g_ptr_array_free(new_paths, TRUE);
        g_strfreev(customKeyBindingSchemaPaths);

        result = EXIT_SUCCESS;
    }
    g_object_unref(mediaKeysSettings);

    return result;
}

/**
 * Lists all custom key bindings to standard out, one per line, with the format:
 *
 *  index <tab> name <tab> command <tab> binding
 *
 * @return EXIT_SUCCESS
 */
static int list() {
    GSettings *mediaKeysSettings = g_settings_new(SCHEMA_ID__MEDIA_KEYS);

    gchar **paths = getDefinedBindingPaths(mediaKeysSettings);
    for (gsize index = 0; paths[index]; ++index) {
        CustomKeyBinding *b = loadCustomKeyBinding(paths[index]);
        g_print(FS__LIST__BINDING_LINE, index, b->name, b->command, b->binding);
        freeBinding(b);
    }
    g_strfreev(paths);

    g_object_unref(mediaKeysSettings);

    return EXIT_SUCCESS;
}

int main(const int numberOfArgs, char *argv[]) {
    int result = EXIT_FAILURE;

    const int numberOfUserSuppliedArguments = numberOfArgs - 1;
    if (numberOfUserSuppliedArguments == 0) {
        g_printerr(FS__ERROR__NO_VERB);
        g_printerr(FS__ERROR__USAGE(APP_NAME));
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
                g_printerr(FS__ERROR__ADD__3_ARGUMENTS_TO_ADD_EXPECTED);
                g_printerr(FS__ERROR__ADD__USAGE, APP_NAME, APP_NAME);
            }
        } else if (str_equal(verb, VERB__DELETE)) {
            const int numberOfDeleteIndices = numberOfArgs - 2;
            switch (numberOfDeleteIndices) {
                case 0:
                    result = deleteAll();
                    break;
                case 1:
                    const glong numberOfIndexCharacters = g_utf8_strlen(argv[2], -1);
                    if (numberOfIndexCharacters > MAX_DELETE_INDEX_NUM_DIGITS) {
                        g_printerr(FS__ERROR__DELETE__TOO_MAN_INDEX_CHARACTERS, MAX_DELETE_INDEX_NUM_DIGITS);
                    } else {
                        regex_t regex;
                        regcomp(&regex, DELETE__INDEX__REGEX, REG_EXTENDED | REG_NOSUB);
                        if (!regexec(&regex, argv[2], 0, NULL, 0)) {
                            result = deleteByIndex(strtol(argv[2], NULL, 10));
                        } else {
                            g_printerr(FS__ERROR__DELETE__INVALID_INDEX, argv[2]);
                        }
                        regfree(&regex);
                    }
                    break;
                default:
                    g_printerr(FS__ERROR__DELETE__0_OR_1_ARGUMENTS_EXPECTED);
                    g_printerr(FS__ERROR__DELETE__USAGE, APP_NAME, APP_NAME);
                    break;
            }
        } else {
            g_printerr(FS__ERROR__UNKNOWN_VERB, verb);
            g_printerr(FS__ERROR__USAGE(APP_NAME));
        }
    }

    return result;
}
